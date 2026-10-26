#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ParamIDs
{
    inline constexpr const char* level    = "level";
    inline constexpr const char* drive    = "drive";
    inline constexpr const char* low      = "low";
    inline constexpr const char* mid      = "mid";
    inline constexpr const char* high     = "high";
    inline constexpr const char* presence = "presence";
    inline constexpr const char* blend    = "blend";
    inline constexpr const char* cab      = "cab";
    inline constexpr const char* driveOn  = "driveOn";
}

// Frequências do tone stack (usadas pelo DSP e pela curva de EQ desenhada na tela).
// Ajuste aqui ao comparar com uma referência no Plugin Doctor (veja o README).
namespace Tone
{
    inline constexpr float lowFreq      = 80.0f;
    inline constexpr float midFreq      = 600.0f;
    inline constexpr float midQ         = 0.8f;
    inline constexpr float highFreq     = 2500.0f;
    inline constexpr float presenceFreq = 5000.0f;
    inline constexpr float presenceQ    = 1.0f;
}

//==============================================================================
// AURA Bass — overdrive de baixo.
//
// Fluxo do sinal:
//   entrada -> HPF 25 Hz -> pré-ênfase (+médios) -> [4x oversampling]
//   ganho de Drive -> waveshaper assimétrico -> [downsample]
//   -> remove DC + LPF anti-fizz -> Blend (limpo x saturado; DRIVE desligado = só o limpo)
//   -> Low / Mid / High / Presence -> Cab (opcional) -> Level
//==============================================================================
class AuraBassAudioProcessor : public juce::AudioProcessor
{
public:
    AuraBassAudioProcessor();
    ~AuraBassAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets de fábrica (aparecem no menu de presets da DAW)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // A curva de saturação fica pública/estática para poder ser testada isoladamente.
    static float shape (float x) noexcept;

    // Ganho que deixa o som saturado no mesmo volume do limpo (sinal de referência de -12 dBFS)
    static float levelCompensation (float driveDb) noexcept;

    // Resposta de impulso do CAB: "builtin:0" (4x10), "builtin:1" (1x15) ou "file:<caminho>"
    void setCabIR (const juce::String& source);
    juce::String getCabIR() const;
    juce::String getCabIRDisplayName() const;
    static juce::StringArray builtinCabNames() { return { "4x10", "1x15" }; }

    // Escala da interface (100/125/150%), salva junto com a sessão
    float getUiScale() const;
    void setUiScale (float scale);

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    using Coefs  = juce::dsp::IIR::ArrayCoefficients<float>;

    void setupFixedFilters();
    void updateToneFilters (bool force);
    float effectiveBlend() const noexcept;
    void applyCabIR();

    int currentProgram = 0;
    float lastCompDrive = -1000.0f;

    double currentSampleRate = 44100.0;

    // Filtros fixos
    Filter inputHpf, preEmphasis, postDcBlock, postLpf;
    juce::dsp::Convolution cabConvolution;

    // Tone stack
    Filter lowShelf, midPeak, highShelf, presencePeak;
    float lastLow = -100.f, lastMid = -100.f, lastHigh = -100.f, lastPresence = -100.f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1024 };

    juce::AudioBuffer<float> dryBuffer, preCabBuffer;

    juce::SmoothedValue<float> driveGain, blendMix, cabMix, outputGain, autoGain;

    std::atomic<float>* pLevel    = nullptr;
    std::atomic<float>* pDrive    = nullptr;
    std::atomic<float>* pLow      = nullptr;
    std::atomic<float>* pMid      = nullptr;
    std::atomic<float>* pHigh     = nullptr;
    std::atomic<float>* pPresence = nullptr;
    std::atomic<float>* pBlend    = nullptr;
    std::atomic<float>* pCab      = nullptr;
    std::atomic<float>* pDriveOn  = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraBassAudioProcessor)
};
