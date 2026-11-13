// AURA Bass — processamento de áudio
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
#include "PluginProcessor.h"
#include "Autoria.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    using namespace Tone;

    constexpr float driveRefDb    = 20.0f;   // Drive 0 dB = +20 dB antes do clipper (faixa ±15 dB → +5 a +35 dB)
    constexpr float clipBias      = 0.15f;   // assimetria -> harmônicos pares
    constexpr int   osFactorLog2  = 2;       // 2^2 = 4x oversampling
    constexpr float wetTrim       = 0.7f;    // -3 dB: deixa Drive máximo perto de 0 dBFS
    constexpr float inputHpfHz    = 18.0f;   // abaixo do Si grave (30,9 Hz) de um baixo de 5 cordas
    constexpr int   toneSubBlock  = 32;      // EQ: coeficientes recalculados a cada 32 amostras ao girar
    constexpr double maxIrSeconds = 1.0;

    float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }

    struct Preset
    {
        const char* name;
        float level, drive, low, mid, high, presence, blend;
        bool cab, driveOn;
    };

    // Presets de fábrica (o primeiro é o padrão)
    constexpr Preset presets[] = {
        { "AURA (padrão)",   0.0f,   0.0f, 0.0f,  0.0f,  0.0f, 3.0f, 100.0f, true, true  },
        { "Clean DI",        0.0f,   0.0f, 2.0f,  0.0f,  1.0f, 2.0f, 100.0f, true, false },
        { "Grit leve",       0.0f,  -8.0f, 1.0f,  2.0f,  0.0f, 3.0f,  60.0f, true, true  },
        { "Drive pesado",    0.0f,  10.0f, 3.0f,  4.0f, -1.0f, 4.0f,  85.0f, true, true  },
        { "Slap brilhante",  0.0f, -12.0f, 4.0f, -5.0f,  6.0f, 6.0f,  35.0f, true, true  },
    };
    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    bool isIdle (const juce::SmoothedValue<float>& v) noexcept
    {
        return juce::exactlyEqual (v.getTargetValue(), 0.0f) && ! v.isSmoothing();
    }
}

//==============================================================================
AuraBassAudioProcessor::AuraBassAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // Mantém o selo de autoria gravado no binário (volatile impede o descarte).
    static const char* volatile auraSelo = AuraAutoria::marca;
    juce::ignoreUnused (auraSelo);

    pTrim     = apvts.getRawParameterValue (ParamIDs::trim);
    pLevel    = apvts.getRawParameterValue (ParamIDs::level);
    pDrive    = apvts.getRawParameterValue (ParamIDs::drive);
    pLow      = apvts.getRawParameterValue (ParamIDs::low);
    pMid      = apvts.getRawParameterValue (ParamIDs::mid);
    pHigh     = apvts.getRawParameterValue (ParamIDs::high);
    pPresence = apvts.getRawParameterValue (ParamIDs::presence);
    pBlend    = apvts.getRawParameterValue (ParamIDs::blend);
    pCab      = apvts.getRawParameterValue (ParamIDs::cab);
    pDriveOn  = apvts.getRawParameterValue (ParamIDs::driveOn);
    pTuner    = apvts.getRawParameterValue (ParamIDs::tuner);
    applyCabIR();
}

//==============================================================================
int AuraBassAudioProcessor::getNumPrograms() { return numPresets; }

const juce::String AuraBassAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? juce::String::fromUTF8 (presets[index].name) : juce::String();
}

void AuraBassAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets))
        return;
    currentProgram.store (index);
    const auto& pr = presets[index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    set (ParamIDs::level, pr.level);
    set (ParamIDs::drive, pr.drive);
    set (ParamIDs::low, pr.low);
    set (ParamIDs::mid, pr.mid);
    set (ParamIDs::high, pr.high);
    set (ParamIDs::presence, pr.presence);
    set (ParamIDs::blend, pr.blend);
    set (ParamIDs::cab, pr.cab ? 1.0f : 0.0f);
    set (ParamIDs::driveOn, pr.driveOn ? 1.0f : 0.0f);
}

//==============================================================================
juce::String AuraBassAudioProcessor::getCabIR() const
{
    const juce::ScopedLock sl (cabIrLock);
    return cabIrSource;
}

juce::String AuraBassAudioProcessor::getCabIRDisplayName() const
{
    const auto src = getCabIR();
    if (src.startsWith ("file:"))
        return juce::File (src.fromFirstOccurrenceOf ("file:", false, false)).getFileNameWithoutExtension();
    return builtinCabNames()[juce::jlimit (0, 1, src.getTrailingIntValue())];
}

void AuraBassAudioProcessor::setCabIR (const juce::String& source)
{
    {
        const juce::ScopedLock sl (cabIrLock);
        cabIrSource = source.isNotEmpty() ? source : juce::String ("builtin:0");
    }
    applyCabIR();
}

void AuraBassAudioProcessor::applyCabIR()
{
    // Lê a IR (embutida ou arquivo), usa só o primeiro canal, limita a 1 s e ajusta o ganho para
    // 0 dB na região de 100-800 Hz: ligar o CAB não muda o volume, qualquer que seja a IR.
    // A Convolution do JUCE faz a troca no áudio sem travar (e com crossfade).
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader;

    const auto src = getCabIR();
    if (src.startsWith ("file:"))
    {
        const juce::File f (src.fromFirstOccurrenceOf ("file:", false, false));
        if (f.existsAsFile())
            reader.reset (fm.createReaderFor (f));
    }
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    {
        // Arquivo inexistente ou ilegível: volta para a IR embutida (mantendo a escolha salva)
        const bool second = ! src.startsWith ("file:") && src.getTrailingIntValue() == 1;
        auto stream = std::make_unique<juce::MemoryInputStream> (second ? BinaryData::cab_1x15_wav : BinaryData::cab_4x10_wav,
                                                                 (size_t) (second ? BinaryData::cab_1x15_wavSize : BinaryData::cab_4x10_wavSize), false);
        reader.reset (fm.createReaderFor (std::move (stream)));
    }
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return;

    const double irRate = reader->sampleRate;
    const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (irRate * maxIrSeconds));
    juce::AudioBuffer<float> ir (1, len);
    ir.clear();
    reader->read (&ir, 0, len, 0, true, false);

    // Ganho médio (em dB) em 16 pontos entre 100 e 800 Hz: robusto contra vales estreitos da IR
    constexpr int numPoints = 16;
    double sumDb = 0.0;
    const float* d = ir.getReadPointer (0);
    for (int k = 0; k < numPoints; ++k)
    {
        const double f = 100.0 * std::pow (8.0, (double) k / (numPoints - 1));
        const double w = juce::MathConstants<double>::twoPi * f / irRate;
        double re = 0.0, im = 0.0;
        for (int n = 0; n < len; ++n) { re += d[n] * std::cos (w * n); im -= d[n] * std::sin (w * n); }
        sumDb += juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im), -100.0);
    }
    const double avgDb = sumDb / numPoints;
    if (avgDb > -90.0)
        ir.applyGain ((float) juce::Decibels::decibelsToGain (-avgDb));

    cabTailSeconds.store ((float) ((double) len / irRate));

    using C = juce::dsp::Convolution;
    cabConvolution.loadImpulseResponse (std::move (ir), irRate, C::Stereo::no, C::Trim::yes, C::Normalise::no);
    cabIrVersion.fetch_add (1);
}

//==============================================================================
float AuraBassAudioProcessor::levelCompensation (float driveDb) noexcept
{
    // Mede o ganho RMS do caminho saturado com uma senoide de -12 dBFS (nível típico de baixo
    // na entrada) e devolve o inverso: com o DRIVE ligado o volume fica parecido com o limpo.
    const float g = dbToGain (driveDb + driveRefDb);
    const float makeup = wetTrim / std::tanh (g);
    constexpr int n = 256;
    float y[n];
    double mean = 0.0, sumIn = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * (float) i / 64.0f);
        y[i] = shape (x * g) * makeup;
        mean += y[i];
        sumIn += (double) x * x;
    }
    mean /= n;
    double sumOut = 0.0;
    for (int i = 0; i < n; ++i)
        sumOut += (y[i] - mean) * (y[i] - mean);
    return (float) std::sqrt (sumIn / juce::jmax (1.0e-12, sumOut));
}

juce::AudioProcessorValueTreeState::ParameterLayout AuraBassAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    // Textos dos parâmetros (também usados pela DAW): dB com uma casa decimal, % inteiro.
    // valueFromString permite digitar valores na DAW ("3", "-4.5 dB", "60%").
    auto parseNumber = [] (const String& t) { return t.retainCharacters ("0123456789.,-+").replaceCharacter (',', '.').getFloatValue(); };
    auto dbAttr = AudioParameterFloatAttributes()
                      .withLabel ("dB")
                      .withStringFromValueFunction ([] (float v, int) { return String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; })
                      .withValueFromStringFunction (parseNumber);
    auto pctAttr = AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; })
                       .withValueFromStringFunction (parseNumber);
    auto eq = [] { return NormalisableRange<float> (-12.0f, 12.0f, 0.1f); };

    // Trim: ajuste de entrada, antes de tudo. Serve para casar baixos passivos e
    // ativos, e a saída de cada interface, sem mexer no Drive.
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::trim, 1 }, "Trim",
                     NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::level, 1 }, "Level",
                     NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drive, 1 }, "Drive",
                     NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::low, 1 }, "Low", eq(), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mid, 1 }, "Mid", eq(), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::high, 1 }, "High", eq(), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::presence, 1 }, "Presence",
                     NormalisableRange<float> (-7.0f, 7.0f, 0.1f), 3.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::blend, 1 }, "Blend",
                     NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::cab, 1 }, "Cab", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::driveOn, 1 }, "Drive On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::tuner, 1 }, "Tuner", false));

    return { p.begin(), p.end() };
}

bool AuraBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono(), stereo = juce::AudioChannelSet::stereo();

    if (out != mono && out != stereo)
        return false;
    // mono→mono, estéreo→estéreo e mono→estéreo (faixa de baixo mono saindo em estéreo)
    return in == out || (in == mono && out == stereo);
}

//==============================================================================
// Com o DRIVE desligado o Blend vai a 0% (só o sinal limpo, já alinhado), com
// crossfade de 50 ms: sem cliques e sem mudar a latência. EQ, Cab e Level seguem ativos.
float AuraBassAudioProcessor::effectiveBlend() const noexcept
{
    return pDriveOn->load() > 0.5f ? pBlend->load() / 100.0f : 0.0f;
}

float AuraBassAudioProcessor::shape (float x) noexcept
{
    // Soft clip assimétrico: satura antes no semiciclo positivo, gerando
    // harmônicos pares (caráter "valvulado"). O DC resultante é removido depois.
    static const float offset = std::tanh (clipBias);
    return std::tanh (x + clipBias) - offset;
}

void AuraBassAudioProcessor::setupFixedFilters()
{
    const auto sr = currentSampleRate;
    *inputHpf.state    = Coefs::makeHighPass (sr, inputHpfHz, 0.707f);
    *preEmphasis.state = Coefs::makePeakFilter (sr, 800.0f, 0.7f, dbToGain (6.0f));  // médios entram mais quentes no clipper
    *postDcBlock.state = Coefs::makeHighPass (sr, 20.0f, 0.707f);
    *postLpf.state     = Coefs::makeLowPass  (sr, juce::jmin (7000.0f, (float) sr * 0.45f), 0.707f);  // corta o "fizz" digital
}

void AuraBassAudioProcessor::setToneCoefficients (float low, float mid, float high, float presence)
{
    const auto sr = currentSampleRate;
    const float nyq = (float) sr * 0.45f;
    *lowShelf.state     = Coefs::makeLowShelf   (sr, lowFreq, 0.707f, dbToGain (low));
    *midPeak.state      = Coefs::makePeakFilter (sr, midFreq, midQ, dbToGain (mid));
    *highShelf.state    = Coefs::makeHighShelf  (sr, juce::jmin (highFreq, nyq), 0.707f, dbToGain (high));
    *presencePeak.state = Coefs::makePeakFilter (sr, juce::jmin (presenceFreq, nyq), presenceQ, dbToGain (presence));
}

void AuraBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    const auto numChannels = (juce::uint32) juce::jmax (1, getTotalNumInputChannels(), getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) maxBlockSize, numChannels };

    // Tone stack: começa já nos valores atuais (sem deslizar na abertura)
    for (auto* s : { &lowDb, &midDb, &highDb, &presenceDb })
        s->reset (currentSampleRate, 0.03);
    lowDb.setCurrentAndTargetValue (pLow->load());
    midDb.setCurrentAndTargetValue (pMid->load());
    highDb.setCurrentAndTargetValue (pHigh->load());
    presenceDb.setCurrentAndTargetValue (pPresence->load());

    setupFixedFilters();
    setToneCoefficients (lowDb.getCurrentValue(), midDb.getCurrentValue(), highDb.getCurrentValue(), presenceDb.getCurrentValue());

    for (auto* f : { &inputHpf, &preEmphasis, &postDcBlock, &postLpf,
                     &lowShelf, &midPeak, &highShelf, &presencePeak })
    {
        f->prepare (spec);
        f->reset();
    }

    // A IR já carregada é reaproveitada (e reamostrada) pelo prepare
    cabConvolution.prepare (spec);
    cabConvolution.reset();

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlockSize);

    const int latency = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    for (auto* dl : { &dryDelay, &bypassDelay })
    {
        dl->prepare (spec);
        dl->setMaximumDelayInSamples (juce::jmax (latency + 1, 64));
        dl->setDelay ((float) latency);
        dl->reset();
    }

    dryBuffer.setSize ((int) numChannels, maxBlockSize);
    preCabBuffer.setSize ((int) numChannels, maxBlockSize);

    const double osRate = currentSampleRate * (1 << osFactorLog2);
    driveGain.reset (osRate, 0.05);
    driveGain.setCurrentAndTargetValue (dbToGain (pDrive->load() + driveRefDb));
    blendMix.reset (currentSampleRate, 0.05);
    blendMix.setCurrentAndTargetValue (effectiveBlend());
    cabMix.reset (currentSampleRate, 0.03);
    cabMix.setCurrentAndTargetValue (pCab->load() > 0.5f ? 1.0f : 0.0f);
    trimGain.reset (currentSampleRate, 0.03);
    trimGain.setCurrentAndTargetValue (dbToGain (pTrim != nullptr ? pTrim->load() : 0.0f));
    outputGain.reset (currentSampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (dbToGain (pLevel->load()));
    autoGain.reset (currentSampleRate, 0.05);
    lastCompDrive = pDrive->load();
    autoGain.setCurrentAndTargetValue (levelCompensation (lastCompDrive));

    tuner.prepare (currentSampleRate);
    tunerMute.reset (currentSampleRate, 0.02);
    tunerMute.setCurrentAndTargetValue (pTuner->load() > 0.5f ? 0.0f : 1.0f);

    drivePathIdle = cabIdle = false;
}

//==============================================================================
void AuraBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int procChannels = juce::jmin (numIn, numOut, (int) dryBuffer.getNumChannels());
    if (numSamples == 0 || procChannels <= 0 || oversampling == nullptr)
        return;

    // Alvos dos parâmetros (uma vez por bloco; os SmoothedValue fazem a transição)
    lowDb.setTargetValue (pLow->load());
    midDb.setTargetValue (pMid->load());
    highDb.setTargetValue (pHigh->load());
    presenceDb.setTargetValue (pPresence->load());
    driveGain.setTargetValue (dbToGain (pDrive->load() + driveRefDb));
    blendMix.setTargetValue (effectiveBlend());
    cabMix.setTargetValue (pCab->load() > 0.5f ? 1.0f : 0.0f);
    outputGain.setTargetValue (dbToGain (pLevel->load()));
    if (const float d = pDrive->load(); ! juce::exactlyEqual (d, lastCompDrive))
    {
        lastCompDrive = d;
        autoGain.setTargetValue (levelCompensation (d));
    }

    // Trim de entrada, antes de tudo: o afinador e o medidor já veem o sinal
    // ajustado, e o Drive não precisa compensar a saída da interface.
    trimGain.setTargetValue (dbToGain (pTrim->load()));
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = trimGain.getNextValue();
        for (int ch = 0; ch < procChannels; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            d[i] *= g;
            peak = juce::jmax (peak, std::abs (d[i]));
        }
    }
    inputPeak.store (peak, std::memory_order_relaxed);
    if (peak >= 0.99f) inputClip.store (true, std::memory_order_relaxed);

    // Afinador: o detector recebe a entrada logo depois do trim
    const bool tunerOn = pTuner->load() > 0.5f;
    if (tunerOn)
        tuner.push (buffer.getArrayOfReadPointers(), procChannels, numSamples);

    // Blocos maiores que o anunciado são processados em pedaços (nada é realocado aqui)
    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int len = juce::jmin (maxBlockSize, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), procChannels, start, len);
        processChunk (chunk, procChannels);
    }

    // Afinador ligado = saída muda (fade de 20 ms, sem estalo)
    tunerMute.setTargetValue (tunerOn ? 0.0f : 1.0f);
    if (tunerMute.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = tunerMute.getNextValue();
            for (int ch = 0; ch < procChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else if (tunerOn)
    {
        for (int ch = 0; ch < procChannels; ++ch)
            buffer.clear (ch, 0, numSamples);
    }

    // Entrada mono, saída estéreo: copia o resultado para os outros canais
    for (int ch = procChannels; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

void AuraBassAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int numChannels)
{
    const int numSamples = buffer.getNumSamples();
    auto block = juce::dsp::AudioBlock<float> (buffer).getSubsetChannelBlock (0, (size_t) numChannels);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 1) Entrada
    inputHpf.process (ctx);

    // 2) Cópia limpa (atrasada pela latência do oversampling, para o Blend não dar phasing)
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = buffer.getReadPointer (ch);
        float* dst = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            dryDelay.pushSample (ch, src[i]);
            dst[i] = dryDelay.popSample (ch);
        }
    }

    // 3) Saturação em 4x + Blend. Com DRIVE desligado (ou Blend em 0%) o caminho saturado
    //    não é processado: a saída é o sinal limpo alinhado.
    if (isIdle (blendMix))
    {
        driveGain.skip (numSamples << osFactorLog2);
        autoGain.skip (numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
        drivePathIdle = true;
    }
    else
    {
        if (drivePathIdle)
        {
            // Volta do repouso: zera os estados antigos (o Blend sobe do 0, sem clique)
            preEmphasis.reset();
            oversampling->reset();
            postDcBlock.reset();
            postLpf.reset();
            drivePathIdle = false;
        }

        preEmphasis.process (ctx);
        auto up = oversampling->processSamplesUp (block);
        const int upSamples = (int) up.getNumSamples();
        const int upChannels = (int) up.getNumChannels();

        if (driveGain.isSmoothing())
        {
            for (int i = 0; i < upSamples; ++i)
            {
                const float g = driveGain.getNextValue();
                const float makeup = wetTrim / std::tanh (g);   // ~1/g com pouco drive, ~1 com muito
                for (int ch = 0; ch < upChannels; ++ch)
                    up.setSample (ch, i, shape (up.getSample (ch, i) * g) * makeup);
            }
        }
        else
        {
            const float g = driveGain.getTargetValue();
            const float makeup = wetTrim / std::tanh (g);
            for (int ch = 0; ch < upChannels; ++ch)
            {
                float* x = up.getChannelPointer ((size_t) ch);
                for (int i = 0; i < upSamples; ++i)
                    x[i] = shape (x[i] * g) * makeup;
            }
        }
        oversampling->processSamplesDown (block);

        postDcBlock.process (ctx);
        postLpf.process (ctx);

        // Blend paralelo (o saturado já com o volume compensado)
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = blendMix.getNextValue();
            const float comp = autoGain.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[i] = dryBuffer.getSample (ch, i) * (1.0f - m) + d[i] * comp * m;
            }
        }
    }

    // 4) Tone stack (ganhos suavizados)
    processToneStack (block);

    // 5) CAB + Level. Com o CAB desligado a convolução não é processada.
    if (isIdle (cabMix))
    {
        cabIdle = true;
        if (outputGain.isSmoothing())
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float out = outputGain.getNextValue();
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.getWritePointer (ch)[i] *= out;
            }
        }
        else
        {
            buffer.applyGain (0, numSamples, outputGain.getTargetValue());
        }
        return;
    }

    if (cabIdle)
    {
        cabConvolution.reset();   // volta do repouso sem a cauda antiga
        cabIdle = false;
    }

    for (int ch = 0; ch < numChannels; ++ch)
        preCabBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    cabConvolution.process (ctx);

    for (int i = 0; i < numSamples; ++i)
    {
        const float c = cabMix.getNextValue();
        const float out = outputGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = (preCabBuffer.getSample (ch, i) * (1.0f - c) + d[i] * c) * out;
        }
    }
}

void AuraBassAudioProcessor::processToneStack (juce::dsp::AudioBlock<float>& block)
{
    const int n = (int) block.getNumSamples();
    for (int pos = 0; pos < n; pos += toneSubBlock)
    {
        const int len = juce::jmin (toneSubBlock, n - pos);
        if (lowDb.isSmoothing() || midDb.isSmoothing() || highDb.isSmoothing() || presenceDb.isSmoothing())
            setToneCoefficients (lowDb.skip (len), midDb.skip (len), highDb.skip (len), presenceDb.skip (len));

        auto sub = block.getSubBlock ((size_t) pos, (size_t) len);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        lowShelf.process (ctx);
        midPeak.process (ctx);
        highShelf.process (ctx);
        presencePeak.process (ctx);
    }
}

//==============================================================================
// Bypass pela DAW: o sinal passa limpo, mas atrasado pela mesma latência, para continuar
// alinhado com as outras faixas (a DAW compensa a latência do plugin mesmo em bypass).
void AuraBassAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int procChannels = juce::jmin (numIn, numOut, (int) dryBuffer.getNumChannels());
    if (numSamples == 0 || procChannels <= 0 || getLatencySamples() == 0)
        return;

    for (int ch = 0; ch < procChannels; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            bypassDelay.pushSample (ch, d[i]);
            d[i] = bypassDelay.popSample (ch);
        }
    }
    for (int ch = procChannels; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

//==============================================================================
void AuraBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("cabIR", getCabIR(), nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("program", currentProgram.load(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AuraBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    const auto ir = tree.getProperty ("cabIR", "builtin:0").toString();
    setUiScale ((float) tree.getProperty ("uiScale", 1.0f));
    currentProgram.store (juce::jlimit (0, numPresets - 1, (int) tree.getProperty ("program", 0)));
    for (auto id : { "cabIR", "uiScale", "program", "version" })
        tree.removeProperty (id, nullptr);

    apvts.replaceState (tree);
    // O afinador nunca volta ligado numa sessão (o plugin abriria mudo)
    if (auto* t = apvts.getParameter (ParamIDs::tuner))
        t->setValueNotifyingHost (0.0f);
    setCabIR (ir);
}

juce::AudioProcessorEditor* AuraBassAudioProcessor::createEditor()
{
    return new AuraBassAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraBassAudioProcessor();
}
