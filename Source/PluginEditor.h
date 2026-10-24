#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Layout.h"

//==============================================================================
// Imagens da interface (Resources/ui) e fonte dos valores, compartilhadas entre instâncias
struct AuraAssets
{
    AuraAssets();

    juce::Image background, pointer, ledDriveOn, ledCabOn, cabOff, footswitchDown;
    juce::Typeface::Ptr valueTypeface;
};

//==============================================================================
// Knob: a arte já está no fundo; aqui só o indicador gira
class PointerKnob : public juce::Slider
{
public:
    explicit PointerKnob (const AuraAssets&);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

    juce::Point<float> centre;   // centro exato do knob, em coordenadas locais

private:
    const AuraAssets& assets;
};

//==============================================================================
// Área clicável invisível (footswitch do DRIVE e chave do CAB)
class HotSpot : public juce::ToggleButton
{
public:
    HotSpot() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool, bool) override {}
};

//==============================================================================
class AuraBassAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit AuraBassAudioProcessorEditor (AuraBassAudioProcessor&);
    ~AuraBassAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::String formatValue (const juce::String& paramId, float value);

    AuraBassAudioProcessor& processor;
    juce::SharedResourcePointer<AuraAssets> assets;

    std::vector<std::unique_ptr<PointerKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    HotSpot driveSpot, cabSpot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> driveAttachment, cabAttachment;
    bool driveOn = true, cabOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessorEditor)
};
