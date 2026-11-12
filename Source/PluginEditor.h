#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Layout.h"

//==============================================================================
// Imagens da interface (Resources/ui) e fonte dos valores, compartilhadas entre instâncias
struct AuraAssets
{
    AuraAssets();

    juce::Image background, pointer, ledDriveOn, ledCabOn, cabOff, footswitchDown, arcsLit;
    juce::Typeface::Ptr valueTypeface;
    juce::Font valueFont { juce::FontOptions() };
};

//==============================================================================
// Knob: a arte já está no fundo; aqui só o indicador gira.
// Shift = ajuste fino; clique direito = menu da DAW para o parâmetro (automação etc.).
class PointerKnob : public juce::Slider
{
public:
    explicit PointerKnob (const AuraAssets&);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Point<float> centre;                 // centro exato do knob, em coordenadas locais
    std::function<void()> onContextMenu;

private:
    const AuraAssets& assets;
    bool popupClick = false;
};

//==============================================================================
// Área clicável invisível (footswitch do DRIVE e chave do CAB). Só o clique esquerdo alterna;
// o clique direito abre o menu (da DAW para o parâmetro).
class HotSpot : public juce::ToggleButton
{
public:
    HotSpot() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool, bool) override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onContextMenu;

private:
    bool popupClick = false;
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
    std::function<void (juce::RangedAudioParameter&)> onParameterContextMenu;
    float currentScale = 1.0f;

    static juce::String formatValue (const juce::String& paramId, float value);
    void showScaleMenu();

private:
    void timerCallback() override;
    void showCabMenu();
    void chooseIrFile();
    void repaintKnobArea (size_t index);
    juce::Rectangle<float> cabIrArea() const;

    AuraBassAudioProcessor& processor;
    juce::SharedResourcePointer<AuraAssets> assets;

    std::vector<std::unique_ptr<PointerKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;

    HotSpot driveSpot, cabSpot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> driveAttachment, cabAttachment;
    bool driveOn = true, cabOn = true;
    int cabIrVersion = -1;
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
    void showHostMenu (juce::RangedAudioParameter&);

    AuraBassAudioProcessor& processor;
    AuraPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessorEditor)
};
