#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BatAnalysis.h"
#include <atomic>
#include <vector>

// A stereo 5-band EQ laid out like the AMEK EQ 250, with BAT built in:
// BAT listens to the input, suggests a setting for the chosen engineer mode
// and loads it into slot C so it can be compared with the user's slot A.
class BatProcessor : public juce::AudioProcessor
{
public:
    BatProcessor();
    ~BatProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "bx_BAT EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::String gainId (int b) { return "b" + juce::String (b) + "_gain"; }
    static juce::String freqId (int b) { return "b" + juce::String (b) + "_freq"; }
    static juce::String qId (int b)    { return "b" + juce::String (b) + "_q"; }

    juce::AudioProcessorValueTreeState apvts;

    // ---- BAT, message thread only ----
    enum class BatState { Idle, Listening, Suggested };
    static constexpr float listenSeconds = 8.0f;
    static constexpr int slotA = 0, slotC = 2, numSlots = 4;

    void startOrCancelListening();
    float listeningProgress() const;      // 0..1
    bool pollListeningDone();             // true once, when a suggestion is ready
    void setMode (bat::Mode);
    void keepSuggestion();
    void dismissSuggestion();
    void selectSlot (int);
    void setLevelMatch (bool);

    BatState getBatState() const { return batState; }
    bat::Mode getMode() const { return mode; }
    bool hasSuggestion() const { return hasC; }
    bool heardNothing() const { return lastListenEmpty; }
    int getCurrentSlot() const { return currentSlot; }
    bool getLevelMatch() const { return levelMatch; }
    float getSuggestionTrimDb() const { return slotOut[(size_t) slotC]; }
    const bat::Analysis& getAnalysis() const { return analysis; }
    const bat::EqSetting& getSlot (int i) const { return slots[(size_t) i]; }
    float getSlotOutDb (int i) const { return slotOut[(size_t) i]; }
    double getAnalysisRate() const { return currentRate; }

    bat::EqSetting readCurrent() const;
    float readOutDb() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void applySetting (const bat::EqSetting&, float outDb);
    void storeCurrentInSlot();
    void makeSuggestion();
    void updateFilters();

    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (const std::array<float, 6>& c)
        {
            const float ia = 1.0f / c[3];
            b0 = c[0] * ia; b1 = c[1] * ia; b2 = c[2] * ia; a1 = c[4] * ia; a2 = c[5] * ia;
        }
        void reset() { z1 = z2 = 0; }
        void process (float* d, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                d[i] = y;
            }
        }
    };

    double currentRate = 48000.0;
    Biquad filters[bat::numBands][2];
    bat::EqSetting lastApplied {};
    bool filtersDirty = true;
    juce::SmoothedValue<float> outGain;

    std::atomic<float>* gainParam[bat::numBands] {};
    std::atomic<float>* freqParam[bat::numBands] {};
    std::atomic<float>* qParam[bat::numBands] {};
    std::atomic<float>* outParam = nullptr;
    std::atomic<float>* eqInParam = nullptr;

    // Capture of non-silent input, written by the audio thread.
    std::vector<float> capture;
    std::atomic<int> capturePos { 0 }, captureTarget { 0 };
    std::atomic<bool> capturing { false };

    BatState batState = BatState::Idle;
    bat::Mode mode = bat::Mode::Neutral;
    bat::Analysis analysis;
    bool hasC = false, levelMatch = true, lastListenEmpty = false;
    int currentSlot = slotA;
    std::array<bat::EqSetting, numSlots> slots;
    std::array<float, numSlots> slotOut {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatProcessor)
};
