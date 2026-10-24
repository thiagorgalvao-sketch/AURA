#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using namespace Tone;

    constexpr float driveRefDb    = 20.0f;   // Drive 0 dB = +20 dB antes do clipper (faixa ±15 dB → +5 a +35 dB)
    constexpr float clipBias      = 0.15f;   // assimetria -> harmônicos pares
    constexpr int   osFactorLog2  = 2;       // 2^2 = 4x oversampling
    constexpr float wetTrim       = 0.7f;    // -3 dB: deixa Drive máximo perto de 0 dBFS

    float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }
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

    // Simulação simples de caixa 4x10/8x10. Para mais realismo, troque por uma IR (juce::dsp::Convolution).
    *cabHpf.state      = Coefs::makeHighPass   (sr, 45.0f, 0.707f);
    *cabLowBump.state  = Coefs::makePeakFilter (sr, 110.0f, 1.2f, dbToGain (3.0f));
    *cabMidScoop.state = Coefs::makePeakFilter (sr, 1800.0f, 1.0f, dbToGain (-3.0f));
    *cabLpf1.state     = Coefs::makeLowPass    (sr, 4500.0f, 0.54f);
    *cabLpf2.state     = Coefs::makeLowPass    (sr, 4500.0f, 1.31f);  // par Butterworth -> 24 dB/oct
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
                     &cabHpf, &cabLowBump, &cabMidScoop, &cabLpf1, &cabLpf2,
                     &lowShelf, &midPeak, &highShelf, &presencePeak })
    {
        f->prepare (spec);
        f->reset();
    }

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

    // 4) Blend paralelo
    for (int i = 0; i < numSamples; ++i)
    {
        const float m = blendMix.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = dryBuffer.getSample (ch, i) * (1.0f - m) + d[i] * m;
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

    cabHpf.process (ctx);
    cabLowBump.process (ctx);
    cabMidScoop.process (ctx);
    cabLpf1.process (ctx);
    cabLpf2.process (ctx);

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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AuraBassAudioProcessor::createEditor()
{
    return new AuraBassAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuraBassAudioProcessor();
}
