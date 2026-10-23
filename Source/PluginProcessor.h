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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // A curva de saturação fica pública/estática para poder ser testada isoladamente.
    static float shape (float x) noexcept;

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    using Coefs  = juce::dsp::IIR::ArrayCoefficients<float>;

    void setupFixedFilters();
    void updateToneFilters (bool force);
    float effectiveBlend() const noexcept;

    double currentSampleRate = 44100.0;

    // Filtros fixos
    Filter inputHpf, preEmphasis, postDcBlock, postLpf;
    Filter cabHpf, cabLowBump, cabMidScoop, cabLpf1, cabLpf2;

    // Tone stack
    Filter lowShelf, midPeak, highShelf, presencePeak;
    float lastLow = -100.f, lastMid = -100.f, lastHigh = -100.f, lastPresence = -100.f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1024 };

    juce::AudioBuffer<float> dryBuffer, preCabBuffer;

    juce::SmoothedValue<float> driveGain, blendMix, cabMix, outputGain;

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
