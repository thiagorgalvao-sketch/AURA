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
// Knob: a arte já está no fundo; aqui só o indicador gira. Shift = ajuste fino.
class PointerKnob : public juce::Slider
{
public:
    explicit PointerKnob (const AuraAssets&);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

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
// O painel inteiro, sempre no tamanho original (726x339); o editor aplica a escala
class AuraPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit AuraPanel (AuraBassAudioProcessor&);
    ~AuraPanel() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    std::function<void (float)> onScaleChosen;
    float currentScale = 1.0f;

    static juce::String formatValue (const juce::String& paramId, float value);

private:
    void timerCallback() override;
    void showCabMenu();
    void showScaleMenu();
    juce::Rectangle<float> cabIrArea() const;
    juce::Rectangle<float> scaleArea() const;

    AuraBassAudioProcessor& processor;
    juce::SharedResourcePointer<AuraAssets> assets;

    std::vector<std::unique_ptr<PointerKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    HotSpot driveSpot, cabSpot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> driveAttachment, cabAttachment;
    bool driveOn = true, cabOn = true;
    juce::String cabIrName;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraPanel)
};

//==============================================================================
class AuraBassAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AuraBassAudioProcessorEditor (AuraBassAudioProcessor&);

    void resized() override;
    void setScale (float scale);

private:
    AuraBassAudioProcessor& processor;
    AuraPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessorEditor)
};
