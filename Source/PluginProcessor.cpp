#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String hzText (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                        : juce::String (juce::roundToInt (v)) + " Hz";
}
} // namespace

BatProcessor::BatProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int b = 0; b < bat::numBands; ++b)
    {
        gainParam[b] = apvts.getRawParameterValue (gainId (b));
        freqParam[b] = apvts.getRawParameterValue (freqId (b));
        qParam[b] = bat::bandSpecs()[(size_t) b].type == bat::Peak ? apvts.getRawParameterValue (qId (b)) : nullptr;
    }
    outParam = apvts.getRawParameterValue ("out");
    eqInParam = apvts.getRawParameterValue ("eqin");

    for (auto& s : slots) s = bat::flatSetting();
}

juce::AudioProcessorValueTreeState::ParameterLayout BatProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto dbText = [] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; };

    for (int b = 0; b < bat::numBands; ++b)
    {
        const auto& s = bat::bandSpecs()[(size_t) b];
        const juce::String name (s.name);

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { gainId (b), 1 }, name + " Gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));

        juce::NormalisableRange<float> fr (s.fMin, s.fMax, 1.0f);
        fr.setSkewForCentre (std::sqrt (s.fMin * s.fMax));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { freqId (b), 1 }, name + " Freq", fr, s.fDefault,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); })));

        if (s.type == bat::Peak)
        {
            juce::NormalisableRange<float> qr (0.3f, 4.0f, 0.01f);
            qr.setSkewForCentre (1.0f);
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { qId (b), 1 }, name + " Q", qr, s.qDefault,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "Q " + juce::String (v, 2); })));
        }
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "out", 1 }, "Out Gain", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "eqin", 1 }, "EQ In", true));
    return layout;
}

bool BatProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void BatProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    capturing.store (false);
    capture.assign ((size_t) (sampleRate * listenSeconds) + (size_t) samplesPerBlock, 0.0f);
    if (batState == BatState::Listening)
        batState = BatState::Idle;

    for (auto& band : filters)
        for (auto& f : band) f.reset();
    filtersDirty = true;

    outGain.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outParam->load()));
}

void BatProcessor::updateFilters()
{
    bat::EqSetting now;
    for (int b = 0; b < bat::numBands; ++b)
        now[(size_t) b] = { gainParam[b]->load(), freqParam[b]->load(),
                            qParam[b] != nullptr ? qParam[b]->load() : bat::bandSpecs()[(size_t) b].qDefault };

    for (int b = 0; b < bat::numBands; ++b)
    {
        const auto& n = now[(size_t) b];
        const auto& l = lastApplied[(size_t) b];
        if (! filtersDirty && n.gainDb == l.gainDb && n.freq == l.freq && n.q == l.q)
            continue;

        const auto c = bat::coefficientsFor (bat::bandSpecs()[(size_t) b], n, currentRate);
        for (auto& f : filters[b]) f.set (c);
    }
    lastApplied = now;
    filtersDirty = false;
}

void BatProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (2, getTotalNumInputChannels());
    const int n = buffer.getNumSamples();
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // BAT listens to the input before the EQ, and only to audio that is playing.
    if (capturing.load (std::memory_order_acquire) && numCh > 0)
    {
        const float scale = 1.0f / (float) numCh;
        double e = 0.0;
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) m += buffer.getReadPointer (ch)[i];
            m *= scale;
            e += (double) m * m;
        }

        if (std::sqrt (e / std::max (1, n)) > 0.001)
        {
            const int pos = capturePos.load (std::memory_order_relaxed);
            const int target = captureTarget.load (std::memory_order_relaxed);
            const int count = std::min (n, target - pos);
            for (int i = 0; i < count; ++i)
            {
                float m = 0.0f;
                for (int ch = 0; ch < numCh; ++ch) m += buffer.getReadPointer (ch)[i];
                capture[(size_t) (pos + i)] = m * scale;
            }
            capturePos.store (pos + count, std::memory_order_release);
            if (pos + count >= target)
                capturing.store (false, std::memory_order_release);
        }
    }

    updateFilters();

    if (eqInParam->load() > 0.5f)
        for (int ch = 0; ch < numCh; ++ch)
            for (int b = 0; b < bat::numBands; ++b)
                filters[b][ch].process (buffer.getWritePointer (ch), n);

    outGain.setTargetValue (juce::Decibels::decibelsToGain (outParam->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

// ---- parameters and slots ----

void BatProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

bat::EqSetting BatProcessor::readCurrent() const
{
    bat::EqSetting s;
    for (int b = 0; b < bat::numBands; ++b)
        s[(size_t) b] = { gainParam[b]->load(), freqParam[b]->load(),
                          qParam[b] != nullptr ? qParam[b]->load() : bat::bandSpecs()[(size_t) b].qDefault };
    return s;
}

float BatProcessor::readOutDb() const { return outParam->load(); }

void BatProcessor::applySetting (const bat::EqSetting& s, float outDb)
{
    for (int b = 0; b < bat::numBands; ++b)
    {
        setParam (gainId (b), s[(size_t) b].gainDb);
        setParam (freqId (b), s[(size_t) b].freq);
        if (qParam[b] != nullptr) setParam (qId (b), s[(size_t) b].q);
    }
    setParam ("out", outDb);
}

void BatProcessor::storeCurrentInSlot()
{
    slots[(size_t) currentSlot] = readCurrent();
    slotOut[(size_t) currentSlot] = readOutDb();
}

void BatProcessor::selectSlot (int i)
{
    if (i < 0 || i >= numSlots || i == currentSlot)
        return;
    if (i == slotC && ! hasC)
        return;
    storeCurrentInSlot();
    currentSlot = i;
    applySetting (slots[(size_t) i], slotOut[(size_t) i]);
}

// ---- BAT ----

void BatProcessor::startOrCancelListening()
{
    if (batState == BatState::Listening)
    {
        capturing.store (false);
        batState = hasC ? BatState::Suggested : BatState::Idle;
        return;
    }
    if (capture.empty())
        return;

    lastListenEmpty = false;
    captureTarget.store ((int) (currentRate * listenSeconds));
    capturePos.store (0);
    capturing.store (true, std::memory_order_release);
    batState = BatState::Listening;
}

float BatProcessor::listeningProgress() const
{
    const int target = captureTarget.load();
    return target > 0 ? juce::jlimit (0.0f, 1.0f, (float) capturePos.load() / (float) target) : 0.0f;
}

bool BatProcessor::pollListeningDone()
{
    if (batState != BatState::Listening || capturing.load (std::memory_order_acquire))
        return false;

    const int got = capturePos.load (std::memory_order_acquire);
    if (got < captureTarget.load())
        return false;   // cancelled

    analysis = bat::analyse (capture.data(), got, currentRate);
    if (! analysis.valid)
    {
        lastListenEmpty = true;
        batState = hasC ? BatState::Suggested : BatState::Idle;
        return true;
    }
    makeSuggestion();
    batState = BatState::Suggested;
    return true;
}

void BatProcessor::makeSuggestion()
{
    if (! analysis.valid)
        return;

    if (currentSlot != slotC)
        storeCurrentInSlot();

    const auto s = bat::suggest (analysis, mode, currentRate);
    slots[(size_t) slotC] = s.eq;
    slotOut[(size_t) slotC] = 0.0f;
    if (levelMatch)
    {
        const float levelA = bat::weightedLevelDb (analysis, slots[(size_t) slotA], currentRate);
        const float levelC = bat::weightedLevelDb (analysis, s.eq, currentRate);
        slotOut[(size_t) slotC] = juce::jlimit (-12.0f, 12.0f, slotOut[(size_t) slotA] + levelA - levelC);
        slotOut[(size_t) slotC] = std::round (slotOut[(size_t) slotC] * 10.0f) / 10.0f;
    }
    hasC = true;
    currentSlot = slotC;
    applySetting (slots[(size_t) slotC], slotOut[(size_t) slotC]);
}

void BatProcessor::setMode (bat::Mode m)
{
    mode = m;
    if (batState == BatState::Suggested && analysis.valid)
        makeSuggestion();
}

void BatProcessor::setLevelMatch (bool on)
{
    levelMatch = on;
    if (batState == BatState::Suggested && analysis.valid)
        makeSuggestion();
}

void BatProcessor::keepSuggestion()
{
    batState = BatState::Idle;
}

void BatProcessor::dismissSuggestion()
{
    if (currentSlot == slotC)
    {
        currentSlot = slotA;
        applySetting (slots[(size_t) slotA], slotOut[(size_t) slotA]);
    }
    slots[(size_t) slotC] = bat::flatSetting();
    slotOut[(size_t) slotC] = 0.0f;
    hasC = false;
    batState = BatState::Idle;
}

// ---- state ----

void BatProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto batTree = state.getOrCreateChildWithName ("BAT", nullptr);
    batTree.removeAllChildren (nullptr);

    storeCurrentInSlot();
    batTree.setProperty ("slot", currentSlot, nullptr);
    batTree.setProperty ("hasC", hasC, nullptr);
    batTree.setProperty ("mode", (int) mode, nullptr);
    batTree.setProperty ("levelMatch", levelMatch, nullptr);
    for (int s = 0; s < numSlots; ++s)
    {
        juce::ValueTree t ("Slot");
        t.setProperty ("out", slotOut[(size_t) s], nullptr);
        for (int b = 0; b < bat::numBands; ++b)
        {
            const auto& band = slots[(size_t) s][(size_t) b];
            t.setProperty ("g" + juce::String (b), band.gainDb, nullptr);
            t.setProperty ("f" + juce::String (b), band.freq, nullptr);
            t.setProperty ("q" + juce::String (b), band.q, nullptr);
        }
        batTree.appendChild (t, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BatProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    auto batTree = state.getChildWithName ("BAT");
    if (batTree.isValid())
    {
        currentSlot = juce::jlimit (0, numSlots - 1, (int) batTree.getProperty ("slot", 0));
        hasC = batTree.getProperty ("hasC", false);
        mode = (bat::Mode) juce::jlimit (0, bat::numModes - 1, (int) batTree.getProperty ("mode", 0));
        levelMatch = batTree.getProperty ("levelMatch", true);
        for (int s = 0; s < numSlots && s < batTree.getNumChildren(); ++s)
        {
            auto t = batTree.getChild (s);
            slotOut[(size_t) s] = t.getProperty ("out", 0.0f);
            for (int b = 0; b < bat::numBands; ++b)
            {
                auto& band = slots[(size_t) s][(size_t) b];
                band.gainDb = t.getProperty ("g" + juce::String (b), 0.0f);
                band.freq = t.getProperty ("f" + juce::String (b), bat::bandSpecs()[(size_t) b].fDefault);
                band.q = t.getProperty ("q" + juce::String (b), bat::bandSpecs()[(size_t) b].qDefault);
            }
        }
        state.removeChild (batTree, nullptr);
    }
    apvts.replaceState (state);
    batState = BatState::Idle;
}

juce::AudioProcessorEditor* BatProcessor::createEditor() { return new BatEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BatProcessor(); }
