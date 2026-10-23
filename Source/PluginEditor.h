#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Layout.h"

//==============================================================================
// Imagens pré-renderizadas (Tools/AuraRender), carregadas uma vez e compartilhadas
// entre todas as instâncias do plugin.
struct AuraAssets
{
    AuraAssets();

    juce::Image background;
    std::array<juce::Image, 3> knobStrips;     // ARGB, quadros empilhados na vertical
    std::array<int, 3> knobFrameHeight {};
    std::array<juce::Image, 2> switchDown, ledOn;
};

//==============================================================================
// Knob que desenha o quadro pré-renderizado correspondente ao valor
class FilmstripKnob : public juce::Slider
{
public:
    FilmstripKnob (const AuraAssets&, int knobClass);
    void paint (juce::Graphics&) override;

    double lastTouched = -100.0;

private:
    const AuraAssets& assets;
    int cls;
};

//==============================================================================
// Área clicável invisível sobre o footswitch renderizado
class InvisibleFootswitch : public juce::ToggleButton
{
public:
    InvisibleFootswitch() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
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

    std::vector<std::unique_ptr<FilmstripKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    std::array<InvisibleFootswitch, 2> switches;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> switchAttachments;
    std::array<bool, 2> ledState { false, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessorEditor)
};
