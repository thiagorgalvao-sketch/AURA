#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
class GritLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GritLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;

    static inline const juce::Colour panel  { 0xff1b1d21 };
    static inline const juce::Colour face   { 0xff2a2d33 };
    static inline const juce::Colour accent { 0xffe8a33d };
    static inline const juce::Colour text   { 0xffe6e2d8 };
    static inline const juce::Colour dim    { 0xff8a8a86 };
};

//==============================================================================
class GritDIAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GritDIAudioProcessorEditor (GritDIAudioProcessor&);
    ~GritDIAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId, const juce::String& name);

    GritDIAudioProcessor& processor;
    GritLookAndFeel lnf;

    Knob level, drive, low, mid, high, presence, blend;
    juce::ToggleButton cabButton { "CAB" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GritDIAudioProcessorEditor)
};
