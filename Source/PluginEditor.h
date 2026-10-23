#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace AuraColours
{
    inline const juce::Colour background { 0xff121214 };
    inline const juce::Colour engraving  { 0xff3c3e42 };
    inline const juce::Colour valueText  { 0xffb8471c };
    inline const juce::Colour ledRed     { 0xffff3b30 };
    inline const juce::Colour ledGreen   { 0xff34e07a };
}

//==============================================================================
// Knobs de alumínio usinado com borda serrilhada
class AuraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// Footswitch metálico com LED (liga/desliga um parâmetro booleano)
class Footswitch : public juce::ToggleButton
{
public:
    Footswitch (const juce::String& label, juce::Colour ledColour);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Colour led;
};

//==============================================================================
class AuraBassAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit AuraBassAudioProcessorEditor (AuraBassAudioProcessor&);
    ~AuraBassAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildChassisTexture();
    static juce::String formatValue (const juce::String& paramId, float value);

    struct Knob
    {
        juce::String paramId, name;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        double lastTouched = -100.0;   // mostra o valor no rótulo logo após mexer
    };

    struct Group
    {
        juce::String title;
        juce::Rectangle<int> area;
    };

    AuraBassAudioProcessor& processor;
    AuraLookAndFeel lnf;

    std::array<Knob, 7> knobs;
    std::array<Group, 3> groups;
    Footswitch driveSwitch { "DRIVE", AuraColours::ledRed };
    Footswitch cabSwitch   { "CAB",   AuraColours::ledGreen };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> driveAttachment, cabAttachment;

    juce::Rectangle<int> chassisBounds, wordmarkBounds;
    int separatorY = 0, labelY = 0;
    juce::Image chassisTexture;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessorEditor)
};
