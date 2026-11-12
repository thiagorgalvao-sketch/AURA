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

// Frequências do tone stack. Ajuste aqui ao comparar com uma referência (veja o README).
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
//   entrada -> HPF 18 Hz -> pré-ênfase (+médios) -> [4x oversampling]
//   ganho de Drive -> waveshaper assimétrico -> [downsample]
//   -> remove DC + LPF anti-fizz -> compensação de volume
//   -> Blend (limpo x saturado; DRIVE desligado = só o limpo)
//   -> Low / Mid / High / Presence (suavizados) -> CAB (convolução, opcional) -> Level
//
// Regras de tempo real: processBlock não aloca memória, não trava e não lê arquivos.
// Blocos maiores que o anunciado em prepareToPlay são processados em pedaços.
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
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return (double) cabTailSeconds.load(); }

    // Presets de fábrica (aparecem no menu de presets da DAW)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram.load(); }
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

    // Resposta de impulso do CAB: "builtin:0" (4x10), "builtin:1" (1x15) ou "file:<caminho>".
    // Chamar da thread de mensagens (lê o arquivo); a troca no áudio é feita sem travar.
    void setCabIR (const juce::String& source);
    juce::String getCabIR() const;
    juce::String getCabIRDisplayName() const;
    int getCabIRVersion() const noexcept { return cabIrVersion.load(); }
    static juce::StringArray builtinCabNames() { return { "4x10", "1x15" }; }

    // Escala da interface (100/125/150%), salva junto com a sessão
    float getUiScale() const noexcept { return uiScale.load(); }
    void setUiScale (float scale) noexcept { uiScale.store (juce::jlimit (1.0f, 1.5f, scale)); }

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    using Coefs  = juce::dsp::IIR::ArrayCoefficients<float>;

    void processChunk (juce::AudioBuffer<float>& buffer, int numChannels);
    void processToneStack (juce::dsp::AudioBlock<float>& block);
    void setupFixedFilters();
    void setToneCoefficients (float low, float mid, float high, float presence);
    float effectiveBlend() const noexcept;
    void applyCabIR();

    std::atomic<int> currentProgram { 0 };
    float lastCompDrive = -1000.0f;

    double currentSampleRate = 44100.0;
    int maxBlockSize = 0;

    // Estado fora dos parâmetros: guardado em membros (e só copiado para o XML ao salvar),
    // para não mexer na ValueTree do APVTS de duas threads ao mesmo tempo.
    mutable juce::CriticalSection cabIrLock;
    juce::String cabIrSource { "builtin:0" };
    std::atomic<int> cabIrVersion { 0 };
    std::atomic<float> cabTailSeconds { 0.05f };
    std::atomic<float> uiScale { 1.0f };

    // Filtros fixos
    Filter inputHpf, preEmphasis, postDcBlock, postLpf;
    juce::dsp::Convolution cabConvolution;

    // Tone stack (ganhos suavizados, coeficientes recalculados a cada 32 amostras)
    Filter lowShelf, midPeak, highShelf, presencePeak;
    juce::SmoothedValue<float> lowDb, midDb, highDb, presenceDb;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1024 }, bypassDelay { 1024 };

    juce::AudioBuffer<float> dryBuffer, preCabBuffer;

    juce::SmoothedValue<float> driveGain, blendMix, cabMix, outputGain, autoGain;

    // Economia de CPU: a saturação (DRIVE desligado) e a convolução (CAB desligado) param de
    // ser processadas; ao voltar, os filtros são zerados e o crossfade evita qualquer clique.
    bool drivePathIdle = false, cabIdle = false;

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
