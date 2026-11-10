#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace batui
{
const juce::Colour bg        { 0xff1a1a1c };
const juce::Colour panel     { 0xff121214 };
const juce::Colour bar       { 0xff050506 };
const juce::Colour line      { 0xff2e2e33 };
const juce::Colour text      { 0xffe6e6e6 };
const juce::Colour quiet     { 0xff9a9aa0 };
const juce::Colour accent    { 0xff4fc3f7 };
const juce::Colour accentInk { 0xff06202b };
const juce::Colour green     { 0xff3d9a3a };
const juce::Colour amber     { 0xffd89a22 };
const juce::Colour red       { 0xffc8352e };
const juce::Colour darkCap   { 0xff1e1e21 };
} // namespace batui

class BatLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BatLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
};

class BatButton : public juce::Button
{
public:
    BatButton() : juce::Button ("BAT") {}
    float progress = 0.0f;
    bool listening = false;
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class GraphView : public juce::Component
{
public:
    explicit GraphView (BatProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    BatProcessor& proc;
};

class BatPanel : public juce::Component
{
public:
    explicit BatPanel (BatProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BatProcessor& proc;
    juce::TextButton modeButtons[bat::numModes];
    juce::TextButton keepButton { "Keep" }, compareButton { "Hear A" }, dismissButton { "Dismiss" };
};

class BatEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BatEditor (BatProcessor&);
    ~BatEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateGlows();
    static void setGlow (juce::Slider&, bool);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct BandUI
    {
        juce::Label name;
        juce::Slider gain, freq, q;
        std::unique_ptr<SliderAttachment> gainAtt, freqAtt, qAtt;
        bool hasQ = false;
    };

    BatProcessor& proc;
    BatLookAndFeel lnf;
    juce::TextButton slotButtons[BatProcessor::numSlots];
    BatButton batButton;
    GraphView graph;
    BatPanel batPanel;
    std::array<BandUI, bat::numBands> bands;
    juce::Slider outKnob;
    juce::Label outLabel;
    std::unique_ptr<SliderAttachment> outAtt;
    juce::ToggleButton eqIn { "EQ IN" }, levelMatch { "Level match" };
    std::unique_ptr<ButtonAttachment> eqInAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BatEditor)
};
