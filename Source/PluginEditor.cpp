#include "PluginEditor.h"

using namespace batui;

namespace
{
juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String hzShort (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}

juce::String dbShort (float v)
{
    return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
}
} // namespace

// ---------------------------------------------------------------- look and feel

BatLookAndFeel::BatLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2f));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff16313d));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, accent);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
}

void BatLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (9.0f);
    const float d = std::min (area.getWidth(), area.getHeight());
    const auto r = area.withSizeKeepingCentre (d, d);
    const auto c = r.getCentre();
    const bool glow = s.getProperties()["bat"];

    // Scale ticks around the knob.
    g.setColour (quiet.withAlpha (0.7f));
    for (int t = 0; t <= 10; ++t)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) t / 10.0f;
        const auto p0 = c.getPointOnCircumference (d * 0.5f + 2.0f, a);
        const auto p1 = c.getPointOnCircumference (d * 0.5f + 6.0f, a);
        g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.2f);
    }

    if (glow)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (r.expanded (7.0f));
        g.setColour (accent);
        g.drawEllipse (r.expanded (4.0f), 2.2f);
    }

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffdcdce0), c.x, r.getY(),
                                             juce::Colour (0xff85858b), c.x, r.getBottom(), false));
    g.fillEllipse (r);

    const auto inner = r.reduced (d * 0.13f);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.fillEllipse (inner);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (inner, 1.0f);

    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -inner.getHeight() * 0.5f + 3.0f, 3.2f, inner.getHeight() * 0.34f, 1.6f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c));
}

void BatLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto col = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : base;
    if (over) col = col.brighter (0.08f);
    if (down) col = col.darker (0.1f);
    if (! b.isEnabled()) col = col.withAlpha (0.4f);
    g.setColour (col);
    g.fillRoundedRectangle (r, 6.0f);
    if (b.getToggleState())
    {
        g.setColour (accent.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 6.0f, 1.2f);
    }
}

void BatLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds();
    auto box = r.removeFromLeft (18).withSizeKeepingCentre (14, 14).toFloat();
    g.setColour (b.getToggleState() ? juce::Colour (0xff7ddc4a) : juce::Colour (0xff3a3a3f));
    g.fillRoundedRectangle (box, 2.5f);
    if (b.getToggleState())
    {
        g.setColour (juce::Colour (0xff7ddc4a).withAlpha (0.4f));
        g.drawRoundedRectangle (box.expanded (2.0f), 3.0f, 2.0f);
    }
    g.setColour (over ? text : text.withAlpha (0.85f));
    g.setFont (font (13.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- BAT button

void BatButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;

    g.setColour (juce::Colour (0xff0f2430));
    g.fillRoundedRectangle (r, rad);

    if (listening)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.withWidth (r.getWidth() * progress).toNearestInt());
        g.setColour (accent);
        g.fillRoundedRectangle (r, rad);
    }

    g.setColour (over ? accent.brighter (0.2f) : accent);
    g.drawRoundedRectangle (r, rad, 1.6f);

    // Waveform glyph
    const bool inkOnFill = listening && progress > 0.3f;
    g.setColour (inkOnFill ? accentInk : accent);
    const float gx = r.getX() + 16.0f, gy = r.getCentreY();
    const float hs[] = { 2.0f, 8.0f, 13.0f, 7.0f, 3.0f };
    for (int i = 0; i < 5; ++i)
        g.fillRoundedRectangle (gx + (float) i * 4.5f, gy - hs[i] * 0.5f, 2.4f, hs[i], 1.2f);

    g.setColour (listening && progress > 0.5f ? accentInk : accent);
    g.setFont (font (15.0f, true));
    const juce::String label = listening ? "LISTENING " + juce::String (juce::roundToInt (progress * 100.0f)) + "%" : "BAT";
    g.drawText (label, r.withTrimmedLeft (34.0f).withTrimmedRight (12.0f), juce::Justification::centred);
}

// ---------------------------------------------------------------- graph

void GraphView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (r, 8.0f);
    auto plot = r.reduced (40.0f, 18.0f).withTrimmedBottom (6.0f);

    const double sr = proc.getAnalysisRate();
    const float dbRange = 12.0f;
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); };
    auto yOf = [&] (float db) { return plot.getCentreY() - juce::jlimit (-dbRange, dbRange, db) / dbRange * plot.getHeight() * 0.5f; };

    // Grid
    g.setFont (font (11.0f));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        g.setColour (db == 0.0f ? line.brighter (0.3f) : line);
        g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
        g.setColour (quiet);
        g.drawText (dbShort (db).upToFirstOccurrenceOf (".", false, false), juce::Rectangle<float> (2.0f, yOf (db) - 7.0f, 34.0f, 14.0f),
                    juce::Justification::centredRight);
    }
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        g.setColour (line);
        g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
        g.setColour (quiet);
        const juce::String label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (xOf (f) - 20.0f, plot.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centred);
    }

    const auto& a = proc.getAnalysis();
    auto curvePath = [&] (auto valueAt)
    {
        juce::Path p;
        for (int i = 0; i < bat::gridSize; ++i)
        {
            const float f = bat::gridFreq (i);
            const auto pt = juce::Point<float> (xOf (f), yOf (valueAt (i)));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    };

    const auto eq = proc.readCurrent();
    const bool eqOn = proc.apvts.getRawParameterValue ("eqin")->load() > 0.5f;
    juce::Array<float> eqDb;
    for (int i = 0; i < bat::gridSize; ++i)
        eqDb.add (eqOn ? bat::eqResponseDb (eq, bat::gridFreq (i), sr) : 0.0f);

    if (a.valid)
    {
        const auto target = bat::targetCurve (proc.getMode());

        auto heardPath = curvePath ([&] (int i) { return a.spectrumDb[(size_t) i]; });
        auto fill = heardPath;
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (juce::Colour (0xff3a3a40).withAlpha (0.55f));
        g.fillPath (fill);
        g.setColour (juce::Colour (0xff7a7a82));
        g.strokePath (heardPath, juce::PathStrokeType (1.2f));

        auto targetPath = curvePath ([&] (int i) { return target[(size_t) i]; });
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, targetPath, dashes, 2);
        g.setColour (juce::Colour (0xfff0b54a));
        g.fillPath (dashed);

        auto resultPath = curvePath ([&] (int i) { return a.spectrumDb[(size_t) i] + eqDb[i]; });
        g.setColour (text.withAlpha (0.8f));
        g.strokePath (resultPath, juce::PathStrokeType (1.0f));
    }

    auto eqPath = curvePath ([&] (int i) { return eqDb[i]; });
    g.setColour (accent);
    g.strokePath (eqPath, juce::PathStrokeType (2.4f));

    // Legend
    auto legend = juce::Rectangle<float> (plot.getX() + 8.0f, plot.getY() + 4.0f, 520.0f, 16.0f);
    auto item = [&] (juce::Colour c, const juce::String& t, bool dashedLine)
    {
        auto sw = legend.removeFromLeft (22.0f);
        g.setColour (c);
        if (dashedLine)
            for (int k = 0; k < 3; ++k) g.fillRect (sw.getX() + (float) k * 7.0f, sw.getCentreY() - 0.8f, 4.0f, 1.6f);
        else
            g.fillRect (sw.getX(), sw.getCentreY() - 1.0f, 18.0f, 2.0f);
        g.setColour (quiet);
        g.setFont (font (11.5f));
        auto tw = juce::GlyphArrangement::getStringWidth (font (11.5f), t) + 16.0f;
        g.drawText (t, legend.removeFromLeft (tw), juce::Justification::centredLeft);
    };
    item (accent, "EQ", false);
    if (a.valid)
    {
        item (juce::Colour (0xff7a7a82), "Heard", false);
        item (juce::Colour (0xfff0b54a), "Target: " + bat::modeName (proc.getMode()), true);
        item (text.withAlpha (0.8f), "Heard + EQ", false);
    }
}

// ---------------------------------------------------------------- BAT panel

BatPanel::BatPanel (BatProcessor& p) : proc (p)
{
    for (int m = 0; m < bat::numModes; ++m)
    {
        auto& b = modeButtons[m];
        b.setButtonText (bat::modeName ((bat::Mode) m));
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.onClick = [this, m] { proc.setMode ((bat::Mode) m); refresh(); };
        addAndMakeVisible (b);
    }
    keepButton.setColour (juce::TextButton::buttonColourId, accent);
    keepButton.setColour (juce::TextButton::textColourOffId, accentInk);
    keepButton.onClick = [this] { proc.keepSuggestion(); refresh(); };
    compareButton.onClick = [this] { proc.selectSlot (proc.getCurrentSlot() == BatProcessor::slotC ? BatProcessor::slotA : BatProcessor::slotC); refresh(); };
    dismissButton.onClick = [this] { proc.dismissSuggestion(); refresh(); };
    addAndMakeVisible (keepButton);
    addAndMakeVisible (compareButton);
    addAndMakeVisible (dismissButton);
    refresh();
}

void BatPanel::refresh()
{
    for (int m = 0; m < bat::numModes; ++m)
        modeButtons[m].setToggleState ((int) proc.getMode() == m, juce::dontSendNotification);

    const bool suggested = proc.getBatState() == BatProcessor::BatState::Suggested;
    keepButton.setVisible (suggested);
    compareButton.setVisible (suggested);
    dismissButton.setVisible (suggested);
    compareButton.setButtonText (proc.getCurrentSlot() == BatProcessor::slotC ? "Hear A" : "Hear C");
    repaint();
}

void BatPanel::resized()
{
    auto r = getLocalBounds().reduced (18);
    const int bw = (r.getWidth() - 8) / 2;
    for (int m = 0; m < bat::numModes; ++m)
        modeButtons[m].setBounds (r.getX() + (m % 2) * (bw + 8), 104 + (m / 2) * 44, bw, 36);

    auto row = juce::Rectangle<int> (r.getX(), getHeight() - 18 - 44, r.getWidth(), 44);
    keepButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    compareButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    dismissButton.setBounds (row);
}

void BatPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    const int x = 18, w = getWidth() - 36;
    g.setColour (accent);
    g.setFont (font (28.0f, true));
    g.drawText ("BAT", x, 14, w, 34, juce::Justification::centredLeft);
    g.setColour (quiet);
    g.setFont (font (13.0f));
    g.drawText ("Brainworx Assistive Technology", x, 46, w, 18, juce::Justification::centredLeft);

    auto section = [&] (const juce::String& t, int y)
    {
        g.setColour (quiet);
        g.setFont (font (11.5f, true));
        g.drawText (t.toUpperCase(), x, y, w, 16, juce::Justification::centredLeft);
    };

    section ("Engineer mode", 82);

    g.setColour (text.withAlpha (0.85f));
    g.setFont (font (13.0f));
    g.drawFittedText (bat::modeDescription (proc.getMode()), x, 196, w, 36, juce::Justification::topLeft, 2);

    const auto state = proc.getBatState();
    int y = 246;

    if (state == BatProcessor::BatState::Listening)
    {
        section ("Listening", y);
        const float p = proc.listeningProgress();
        auto barR = juce::Rectangle<float> ((float) x, (float) y + 26.0f, (float) w, 6.0f);
        g.setColour (line);
        g.fillRoundedRectangle (barR, 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (barR.withWidth (barR.getWidth() * p), 3.0f);
        g.setColour (text);
        g.setFont (font (13.0f));
        g.drawFittedText ("Play the loudest part of the track. BAT needs " + juce::String ((int) BatProcessor::listenSeconds)
                              + " seconds of audio and pauses while the track is silent.\n\n"
                              + juce::String (p * BatProcessor::listenSeconds, 1) + " of "
                              + juce::String ((int) BatProcessor::listenSeconds) + " s heard. Click BAT again to cancel.",
                          x, y + 44, w, 110, juce::Justification::topLeft, 6);
        return;
    }

    if (state == BatProcessor::BatState::Suggested)
    {
        const auto& a = proc.getAnalysis();
        section ("Heard", y);
        float px = (float) x, py = (float) y + 22.0f;
        g.setFont (font (12.5f));
        for (auto& tag : a.heard)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (font (12.5f), tag) + 20.0f;
            if (px + tw > (float) (x + w)) { px = (float) x; py += 30.0f; }
            g.setColour (juce::Colour (0xff2a2a2f));
            g.fillRoundedRectangle (px, py, tw, 24.0f, 12.0f);
            g.setColour (text);
            g.drawText (tag, juce::Rectangle<float> (px, py, tw, 24.0f), juce::Justification::centred);
            px += tw + 6.0f;
        }

        y = (int) py + 40;
        section ("Suggested in slot C", y);
        y += 22;
        const auto& c = proc.getSlot (BatProcessor::slotC);
        const bool onC = proc.getCurrentSlot() == BatProcessor::slotC;
        const auto shown = onC ? proc.readCurrent() : c;
        g.setFont (font (13.0f));
        int changes = 0;
        for (int b = 0; b < bat::numBands; ++b)
        {
            const auto& band = shown[(size_t) b];
            if (std::abs (band.gainDb) < 0.05f) continue;
            g.setColour (text);
            g.drawText (juce::String (bat::bandSpecs()[(size_t) b].name), x, y, 90, 18, juce::Justification::centredLeft);
            g.setColour (accent);
            g.drawText (dbShort (band.gainDb), x + 90, y, 70, 18, juce::Justification::centredLeft);
            g.setColour (quiet);
            juce::String where = "at " + hzShort (band.freq);
            if (bat::bandSpecs()[(size_t) b].type == bat::Peak) where << ", Q " << juce::String (band.q, 1);
            g.drawText (where, x + 160, y, w - 160, 18, juce::Justification::centredLeft);
            y += 20;
            ++changes;
        }
        if (changes == 0)
        {
            g.setColour (text);
            g.drawText ("No changes needed for this mode.", x, y, w, 18, juce::Justification::centredLeft);
            y += 20;
        }
        g.setColour (quiet);
        g.setFont (font (12.5f));
        y += 6;
        if (proc.getLevelMatch())
            g.drawText ("Level matched to A: out " + dbShort (proc.getSuggestionTrimDb()), x, y, w, 18, juce::Justification::centredLeft);
        y += 20;
        g.drawFittedText (onC ? "You're hearing C. Your own setting is safe in A." : "You're hearing A, your own setting.",
                          x, y, w, 36, juce::Justification::topLeft, 2);
        return;
    }

    // Idle
    section (proc.hasSuggestion() ? "Kept" : "How it works", y);
    g.setColour (text.withAlpha (0.9f));
    g.setFont (font (13.0f));
    juce::String msg = proc.hasSuggestion()
        ? "The suggestion is in slot C. Tweak it, compare it with A, or press BAT to listen again."
        : "1. Pick an engineer mode.\n2. Press BAT and play the loudest part of your track.\n"
          "3. BAT loads its suggestion into slot C. Your setting stays in A.\n4. Compare, then keep or dismiss it.";
    if (proc.heardNothing())
        msg = "BAT didn't hear any audio. Start playback, then press BAT again.\n\n" + msg;
    g.drawFittedText (msg, x, y + 24, w, 160, juce::Justification::topLeft, 10);
}

// ---------------------------------------------------------------- editor

BatEditor::BatEditor (BatProcessor& p)
    : AudioProcessorEditor (p), proc (p), graph (p), batPanel (p)
{
    setLookAndFeel (&lnf);

    const char* slotNames[] = { "A", "B", "C", "D" };
    for (int i = 0; i < BatProcessor::numSlots; ++i)
    {
        slotButtons[i].setButtonText (slotNames[i]);
        slotButtons[i].onClick = [this, i] { proc.selectSlot (i); batPanel.refresh(); };
        addAndMakeVisible (slotButtons[i]);
    }

    batButton.onClick = [this] { proc.startOrCancelListening(); batPanel.refresh(); };
    addAndMakeVisible (batButton);
    addAndMakeVisible (graph);
    addAndMakeVisible (batPanel);

    const juce::Colour caps[] = { green, green, amber, red, red };
    auto setupKnob = [this] (juce::Slider& s, juce::Colour cap, int textW)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textW, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, cap);
        addAndMakeVisible (s);
    };

    for (int b = 0; b < bat::numBands; ++b)
    {
        auto& ui = bands[(size_t) b];
        const auto& spec = bat::bandSpecs()[(size_t) b];
        ui.name.setText (spec.name, juce::dontSendNotification);
        ui.name.setFont (font (13.0f, true));
        ui.name.setJustificationType (juce::Justification::centred);
        ui.name.setColour (juce::Label::outlineColourId, juce::Colour (0xff4a4a50));
        addAndMakeVisible (ui.name);

        setupKnob (ui.gain, caps[b], 80);
        setupKnob (ui.freq, darkCap, 80);
        ui.gainAtt = std::make_unique<SliderAttachment> (proc.apvts, BatProcessor::gainId (b), ui.gain);
        ui.freqAtt = std::make_unique<SliderAttachment> (proc.apvts, BatProcessor::freqId (b), ui.freq);
        ui.gain.setDoubleClickReturnValue (true, 0.0);
        ui.freq.setDoubleClickReturnValue (true, spec.fDefault);

        ui.hasQ = spec.type == bat::Peak;
        if (ui.hasQ)
        {
            setupKnob (ui.q, darkCap, 70);
            ui.qAtt = std::make_unique<SliderAttachment> (proc.apvts, BatProcessor::qId (b), ui.q);
            ui.q.setDoubleClickReturnValue (true, spec.qDefault);
        }
    }

    setupKnob (outKnob, darkCap, 80);
    outAtt = std::make_unique<SliderAttachment> (proc.apvts, "out", outKnob);
    outKnob.setDoubleClickReturnValue (true, 0.0);
    outLabel.setText ("OUT GAIN", juce::dontSendNotification);
    outLabel.setFont (font (13.0f, true));
    outLabel.setJustificationType (juce::Justification::centred);
    outLabel.setColour (juce::Label::outlineColourId, juce::Colour (0xff4a4a50));
    addAndMakeVisible (outLabel);

    eqInAtt = std::make_unique<ButtonAttachment> (proc.apvts, "eqin", eqIn);
    addAndMakeVisible (eqIn);
    levelMatch.setToggleState (proc.getLevelMatch(), juce::dontSendNotification);
    levelMatch.onClick = [this] { proc.setLevelMatch (levelMatch.getToggleState()); batPanel.refresh(); };
    addAndMakeVisible (levelMatch);

    setSize (1320, 600);
    startTimerHz (30);
}

BatEditor::~BatEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BatEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (bar);
    g.fillRect (0, 0, getWidth(), 52);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());

    g.setColour (text);
    g.setFont (font (20.0f, true));
    g.drawText ("bx_BAT EQ", 20, 0, 200, 52, juce::Justification::centredLeft);
    g.setColour (quiet);
    g.setFont (font (12.0f));
    g.drawText ("PROTOTYPE", 132, 0, 120, 52, juce::Justification::centredLeft);
    g.drawText ("BRAINWORX", getWidth() - 220, 0, 200, 52, juce::Justification::centredRight);

    // Section labels for each knob row.
    g.setFont (font (11.5f));
    for (int b = 0; b < bat::numBands; ++b)
    {
        const int cx = bands[(size_t) b].gain.getBounds().getCentreX();
        g.drawText ("dB", cx - 20, 288, 40, 14, juce::Justification::centred);
        g.drawText ("Hz", cx - 20, 412, 40, 14, juce::Justification::centred);
        if (bands[(size_t) b].hasQ)
            g.drawText ("Q", cx - 20, 512, 40, 14, juce::Justification::centred);
    }
}

void BatEditor::resized()
{
    for (int i = 0; i < BatProcessor::numSlots; ++i)
        slotButtons[i].setBounds (420 + i * 44, 10, 38, 32);
    batButton.setBounds (620, 9, 190, 34);

    graph.setBounds (16, 64, 900, 180);
    batPanel.setBounds (932, 64, 372, 524);

    const int colW = 150;
    for (int b = 0; b < bat::numBands; ++b)
    {
        auto& ui = bands[(size_t) b];
        const int x0 = 16 + b * colW;
        ui.name.setBounds (x0 + 30, 258, colW - 60, 24);
        ui.gain.setBounds (x0 + 15, 300, colW - 30, 112);
        ui.freq.setBounds (x0 + 20, 424, colW - 40, 92);
        if (ui.hasQ)
            ui.q.setBounds (x0 + 35, 524, colW - 70, 72);
    }

    outLabel.setBounds (16 + 5 * colW + 18, 258, 120, 24);
    outKnob.setBounds (16 + 5 * colW + 18, 300, 120, 112);
    eqIn.setBounds (16 + 5 * colW + 24, 440, 120, 24);
    levelMatch.setBounds (16 + 5 * colW + 24, 474, 130, 24);
}

void BatEditor::setGlow (juce::Slider& s, bool on)
{
    if ((bool) s.getProperties()["bat"] != on)
    {
        s.getProperties().set ("bat", on);
        s.repaint();
    }
}

void BatEditor::updateGlows()
{
    const bool onC = proc.hasSuggestion() && proc.getCurrentSlot() == BatProcessor::slotC;
    const auto cur = proc.readCurrent();
    const auto& a = proc.getSlot (BatProcessor::slotA);
    for (int b = 0; b < bat::numBands; ++b)
    {
        const auto& c = cur[(size_t) b];
        const auto& o = a[(size_t) b];
        const bool active = std::abs (c.gainDb) > 0.05f || std::abs (o.gainDb) > 0.05f;
        auto& ui = bands[(size_t) b];
        setGlow (ui.gain, onC && std::abs (c.gainDb - o.gainDb) > 0.05f);
        setGlow (ui.freq, onC && active && std::abs (c.freq - o.freq) > 1.0f);
        if (ui.hasQ)
            setGlow (ui.q, onC && active && std::abs (c.q - o.q) > 0.01f);
    }
    setGlow (outKnob, onC && std::abs (proc.readOutDb() - proc.getSlotOutDb (BatProcessor::slotA)) > 0.05f);
}

void BatEditor::timerCallback()
{
    const auto before = proc.getBatState();
    if (proc.pollListeningDone() || before != proc.getBatState())
        batPanel.refresh();

    const bool listening = proc.getBatState() == BatProcessor::BatState::Listening;
    batButton.listening = listening;
    batButton.progress = proc.listeningProgress();
    batButton.repaint();
    if (listening)
        batPanel.repaint();

    for (int i = 0; i < BatProcessor::numSlots; ++i)
    {
        slotButtons[i].setToggleState (proc.getCurrentSlot() == i, juce::dontSendNotification);
        slotButtons[i].setEnabled (i != BatProcessor::slotC || proc.hasSuggestion());
    }

    updateGlows();
    graph.repaint();
}
