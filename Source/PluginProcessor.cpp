#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    using namespace Tone;

    constexpr float driveRefDb    = 20.0f;   // Drive 0 dB = +20 dB antes do clipper (faixa ±15 dB → +5 a +35 dB)
    constexpr float clipBias      = 0.15f;   // assimetria -> harmônicos pares
    constexpr int   osFactorLog2  = 2;       // 2^2 = 4x oversampling
    constexpr float wetTrim       = 0.7f;    // -3 dB: deixa Drive máximo perto de 0 dBFS

    float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }

    struct Preset
    {
        const char* name;
        float level, drive, low, mid, high, presence, blend;
        bool cab, driveOn;
    };

    // Presets de fábrica (o primeiro é o padrão)
    constexpr Preset presets[] = {
        { "AURA (padrão)",   0.0f,   0.0f, 0.0f,  0.0f, 0.0f, 3.0f, 100.0f, true, true  },
        { "Clean DI",        0.0f,   0.0f, 2.0f,  0.0f, 1.0f, 2.0f, 100.0f, true, false },
        { "Grit leve",       0.0f,  -8.0f, 1.0f,  2.0f, 0.0f, 3.0f,  60.0f, true, true  },
        { "Drive pesado",    0.0f,  10.0f, 3.0f,  4.0f, -1.0f, 4.0f, 85.0f, true, true  },
        { "Slap brilhante",  0.0f, -12.0f, 4.0f, -5.0f, 6.0f, 6.0f,  35.0f, true, true  },
    };
}

//==============================================================================
AuraBassAudioProcessor::AuraBassAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pLevel    = apvts.getRawParameterValue (ParamIDs::level);
    pDrive    = apvts.getRawParameterValue (ParamIDs::drive);
    pLow      = apvts.getRawParameterValue (ParamIDs::low);
    pMid      = apvts.getRawParameterValue (ParamIDs::mid);
    pHigh     = apvts.getRawParameterValue (ParamIDs::high);
    pPresence = apvts.getRawParameterValue (ParamIDs::presence);
    pBlend    = apvts.getRawParameterValue (ParamIDs::blend);
    pCab      = apvts.getRawParameterValue (ParamIDs::cab);
    pDriveOn  = apvts.getRawParameterValue (ParamIDs::driveOn);
    applyCabIR();
}

//==============================================================================
int AuraBassAudioProcessor::getNumPrograms() { return (int) std::size (presets); }

const juce::String AuraBassAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) std::size (presets)) ? juce::String::fromUTF8 (presets[index].name) : juce::String();
}

void AuraBassAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) std::size (presets)))
        return;
    currentProgram = index;
    const auto& pr = presets[index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
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
    return apvts.state.getProperty ("cabIR", "builtin:0").toString();
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
    apvts.state.setProperty ("cabIR", source, nullptr);
    applyCabIR();
}

void AuraBassAudioProcessor::applyCabIR()
{
    // Lê a IR (embutida ou arquivo), usa só o primeiro canal, limita a 1 s e ajusta o ganho para
    // 0 dB na região de 100-800 Hz: ligar o CAB não muda o volume, qualquer que seja a IR.
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
    if (reader == nullptr)
    {
        const bool second = ! src.startsWith ("file:") && src.getTrailingIntValue() == 1;
        auto stream = std::make_unique<juce::MemoryInputStream> (second ? BinaryData::cab_1x15_wav : BinaryData::cab_4x10_wav,
                                                                 (size_t) (second ? BinaryData::cab_1x15_wavSize : BinaryData::cab_4x10_wavSize), false);
        reader.reset (fm.createReaderFor (std::move (stream)));
    }
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    const int len = (int) std::min (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 1.0));
    juce::AudioBuffer<float> ir (1, len);
    reader->read (&ir, 0, len, 0, true, false);

    double sumDb = 0.0;
    const double freqs[] = { 100.0, 200.0, 400.0, 800.0 };
    for (double f : freqs)
    {
        double re = 0.0, im = 0.0;
        const double w = juce::MathConstants<double>::twoPi * f / reader->sampleRate;
        const float* d = ir.getReadPointer (0);
        for (int n = 0; n < len; ++n) { re += d[n] * std::cos (w * n); im -= d[n] * std::sin (w * n); }
        sumDb += juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im), -100.0);
    }
    const double avgDb = sumDb / (double) std::size (freqs);
    if (avgDb > -90.0)
        ir.applyGain ((float) juce::Decibels::decibelsToGain (-avgDb));

    using C = juce::dsp::Convolution;
    cabConvolution.loadImpulseResponse (std::move (ir), reader->sampleRate, C::Stereo::no, C::Trim::yes, C::Normalise::no);
}

float AuraBassAudioProcessor::getUiScale() const
{
    return juce::jlimit (1.0f, 1.5f, (float) apvts.state.getProperty ("uiScale", 1.0f));
}

void AuraBassAudioProcessor::setUiScale (float scale)
{
    apvts.state.setProperty ("uiScale", scale, nullptr);
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

    // Textos dos parâmetros (também usados pela DAW): dB com uma casa decimal, % inteiro
    auto dbAttr = AudioParameterFloatAttributes()
                      .withLabel ("dB")
                      .withStringFromValueFunction ([] (float v, int) { return String (std::abs (v) < 0.05f ? 0.0f : v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto eq = [] (int) { return NormalisableRange<float> (-12.0f, 12.0f, 0.1f); };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::level, 1 }, "Level",
                     NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::drive, 1 }, "Drive",
                     NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::low, 1 }, "Low",
                     eq (0), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mid, 1 }, "Mid",
                     eq (0), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::high, 1 }, "High",
                     eq (0), 0.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::presence, 1 }, "Presence",
                     NormalisableRange<float> (-7.0f, 7.0f, 0.1f), 3.0f, dbAttr));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::blend, 1 }, "Blend",
                     NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::cab, 1 }, "Cab", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::driveOn, 1 }, "Drive On", true));

    return { p.begin(), p.end() };
}

bool AuraBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
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

    *inputHpf.state    = Coefs::makeHighPass (sr, 25.0f, 0.707f);
    *preEmphasis.state = Coefs::makePeakFilter (sr, 800.0f, 0.7f, dbToGain (6.0f));  // médios entram mais quentes no clipper
    *postDcBlock.state = Coefs::makeHighPass (sr, 20.0f, 0.707f);
    *postLpf.state     = Coefs::makeLowPass  (sr, 7000.0f, 0.707f);                   // corta o "fizz" digital
}

void AuraBassAudioProcessor::updateToneFilters (bool force)
{
    const auto sr = currentSampleRate;
    const float low = pLow->load(), mid = pMid->load(), high = pHigh->load(), pres = pPresence->load();

    if (force || ! juce::exactlyEqual (low, lastLow))
        *lowShelf.state = Coefs::makeLowShelf (sr, lowFreq, 0.707f, dbToGain (lastLow = low));
    if (force || ! juce::exactlyEqual (mid, lastMid))
        *midPeak.state = Coefs::makePeakFilter (sr, midFreq, midQ, dbToGain (lastMid = mid));
    if (force || ! juce::exactlyEqual (high, lastHigh))
        *highShelf.state = Coefs::makeHighShelf (sr, highFreq, 0.707f, dbToGain (lastHigh = high));
    if (force || ! juce::exactlyEqual (pres, lastPresence))
        *presencePeak.state = Coefs::makePeakFilter (sr, presenceFreq, presenceQ, dbToGain (lastPresence = pres));
}

void AuraBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto numChannels = (juce::uint32) juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numChannels };

    setupFixedFilters();
    updateToneFilters (true);

    for (auto* f : { &inputHpf, &preEmphasis, &postDcBlock, &postLpf,
                     &lowShelf, &midPeak, &highShelf, &presencePeak })
    {
        f->prepare (spec);
        f->reset();
    }

    // IR carregada antes do prepare: já fica ativa no primeiro bloco
    applyCabIR();
    cabConvolution.prepare (spec);
    cabConvolution.reset();

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const int latency = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (juce::jmax (latency + 1, 64));
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();

    dryBuffer.setSize ((int) numChannels, samplesPerBlock);
    preCabBuffer.setSize ((int) numChannels, samplesPerBlock);

    const double osRate = sampleRate * (1 << osFactorLog2);
    driveGain.reset (osRate, 0.05);
    driveGain.setCurrentAndTargetValue (dbToGain (pDrive->load() + driveRefDb));
    blendMix.reset (sampleRate, 0.05);
    blendMix.setCurrentAndTargetValue (effectiveBlend());
    cabMix.reset (sampleRate, 0.03);
    cabMix.setCurrentAndTargetValue (pCab->load() > 0.5f ? 1.0f : 0.0f);
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (dbToGain (pLevel->load()));
    autoGain.reset (sampleRate, 0.05);
    lastCompDrive = pDrive->load();
    autoGain.setCurrentAndTargetValue (levelCompensation (lastCompDrive));
}

//==============================================================================
void AuraBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (numOut, buffer.getNumChannels());
    if (numSamples == 0 || numChannels == 0)
        return;

    // Garantia caso o host mande um bloco maior que o anunciado
    if (numSamples > dryBuffer.getNumSamples() || numChannels > dryBuffer.getNumChannels())
    {
        dryBuffer.setSize (numChannels, numSamples, false, false, true);
        preCabBuffer.setSize (numChannels, numSamples, false, false, true);
        oversampling->initProcessing ((size_t) numSamples);
    }

    updateToneFilters (false);
    driveGain.setTargetValue (dbToGain (pDrive->load() + driveRefDb));
    blendMix.setTargetValue (effectiveBlend());
    cabMix.setTargetValue (pCab->load() > 0.5f ? 1.0f : 0.0f);
    outputGain.setTargetValue (dbToGain (pLevel->load()));
    if (const float d = pDrive->load(); ! juce::exactlyEqual (d, lastCompDrive))
    {
        lastCompDrive = d;
        autoGain.setTargetValue (levelCompensation (d));
    }

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

    // 3) Saturação em 4x
    preEmphasis.process (ctx);
    auto up = oversampling->processSamplesUp (block);
    const size_t upSamples = up.getNumSamples();
    for (size_t i = 0; i < upSamples; ++i)
    {
        const float g = driveGain.getNextValue();
        const float makeup = wetTrim / std::tanh (g);   // ~1/g com pouco drive, ~1 com muito
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            up.setSample ((int) ch, (int) i, shape (up.getSample ((int) ch, (int) i) * g) * makeup);
    }
    oversampling->processSamplesDown (block);

    postDcBlock.process (ctx);
    postLpf.process (ctx);

    // 4) Blend paralelo (o saturado já com o volume compensado)
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

    // 5) Tone stack
    lowShelf.process (ctx);
    midPeak.process (ctx);
    highShelf.process (ctx);
    presencePeak.process (ctx);

    // 6) Cab (crossfade suave ao ligar/desligar)
    for (int ch = 0; ch < numChannels; ++ch)
        preCabBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    cabConvolution.process (ctx);

    // 7) Mix do cab + Level
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

//==============================================================================
void AuraBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuraBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            applyCabIR();
        }
}

juce::AudioProcessorEditor* AuraBassAudioProcessor::createEditor()
{
    return new AuraBassAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraBassAudioProcessor();
}
