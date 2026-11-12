// AURA Bass — testes do processamento
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// Teste offline do DSP: roda o processador com senoides e checa níveis, THD,
// estabilidade numérica e o alinhamento do Blend (sem phasing).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;
    constexpr float inputHpfHz = 18.0f;   // igual ao do plugin

    // Senoide em blocos de tamanho variável; devolve o canal 0 (e o 1, se pedido)
    std::vector<float> runBlocks (AuraBassAudioProcessor& p, float freq, float amp, int prepareSize,
                                  const std::vector<int>& sizes, int totalSamples, std::vector<float>* right = nullptr,
                                  int numChannels = 2)
    {
        p.prepareToPlay (sr, prepareSize);
        std::vector<float> out;
        juce::MidiBuffer midi;
        int pos = 0, k = 0;
        while (pos < totalSamples)
        {
            const int n = juce::jmin (sizes[(size_t) (k++ % (int) sizes.size())], totalSamples - pos);
            juce::AudioBuffer<float> buf (numChannels, n);
            buf.clear();
            for (int i = 0; i < n; ++i)
                buf.setSample (0, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (pos + i) / sr));
            if (numChannels > 1 && p.getTotalNumInputChannels() > 1)
                buf.copyFrom (1, 0, buf, 0, 0, n);
            p.processBlock (buf, midi);
            for (int i = 0; i < n; ++i)
            {
                out.push_back (buf.getSample (0, i));
                if (right != nullptr && numChannels > 1) right->push_back (buf.getSample (1, i));
            }
            pos += n;
        }
        return out;
    }

    double maxJump (const std::vector<float>& v, size_t a, size_t b)
    {
        double m = 0;
        for (size_t i = std::max (a, (size_t) 1); i < std::min (b, v.size()); ++i)
            m = std::max (m, (double) std::abs (v[i] - v[i - 1]));
        return m;
    }

    void setParam (AuraBassAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Processa 1 s de senoide e devolve a saída (descartando o primeiro 0,5 s)
    std::vector<float> run (AuraBassAudioProcessor& p, float freq, float amp, std::vector<float>* inputOut = nullptr)
    {
        p.prepareToPlay (sr, blockSize);
        const int total = (int) sr;
        std::vector<float> out, in;
        juce::AudioBuffer<float> buf (2, blockSize);
        juce::MidiBuffer midi;
        for (int start = 0; start < total; start += blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (start + i) / sr);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
                in.push_back (x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i)
                out.push_back (buf.getSample (0, i));
        }
        if (inputOut) *inputOut = in;
        return out;
    }

    double rms (const std::vector<float>& v, size_t from)
    {
        double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
        return std::sqrt (s / double (v.size() - from));
    }

    // Amplitude de um harmônico via correlação (Goertzel simplificado)
    double harmonic (const std::vector<float>& v, size_t from, double f)
    {
        double re = 0, im = 0; const size_t n = v.size() - from;
        for (size_t i = 0; i < n; ++i)
        {
            const double ph = 2.0 * juce::MathConstants<double>::pi * f * double (i + from) / sr;
            re += v[i + from] * std::cos (ph); im += v[i + from] * std::sin (ph);
        }
        return 2.0 * std::sqrt (re * re + im * im) / double (n);
    }

    bool finite (const std::vector<float>& v)
    {
        for (auto x : v) if (! std::isfinite (x)) return false;
        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " OK " : "FALHA", what); if (! ok) ++failures; };

    const size_t from = (size_t) (sr / 2);

    std::printf ("Drive (dB) x nível/THD (senoide 100 Hz, -12 dBFS, cab desligado, EQ plano)\n");
    for (float drive : { -15.0f, -7.5f, 0.0f, 7.5f, 15.0f })
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::drive, drive);
        auto out = run (p, 100.0f, 0.25f);
        const double h1 = harmonic (out, from, 100), h2 = harmonic (out, from, 200), h3 = harmonic (out, from, 300),
                     h4 = harmonic (out, from, 400), h5 = harmonic (out, from, 500);
        const double thd = std::sqrt (h2*h2 + h3*h3 + h4*h4 + h5*h5) / h1 * 100.0;
        std::printf ("  drive %+5.1f dB  saída %6.1f dBFS  THD %5.1f%%  H2 %5.1f dB  H3 %5.1f dB\n",
                     drive, juce::Decibels::gainToDecibels (rms (out, from) * std::sqrt (2.0)), thd,
                     juce::Decibels::gainToDecibels (h2 / h1), juce::Decibels::gainToDecibels (h3 / h1));
        check (finite (out), "sem NaN/Inf");
    }

    std::printf ("Blend 0%% = sinal limpo alinhado (sem phasing)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::blend, 0.0f);
        setParam (p, ParamIDs::drive, 15.0f);
        std::vector<float> in;
        auto out = run (p, 440.0f, 0.25f, &in);
        const int lat = p.getLatencySamples();
        // Referência: entrada passada pelo mesmo HPF de entrada do plugin
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, inputHpfHz, 0.707f);
        for (auto& x : in) x = hpf.processSample (x);
        double err = 0, errShift = 0;
        for (size_t i = from; i < out.size(); ++i)
        {
            err      = std::max (err,      (double) std::abs (out[i] - in[i - (size_t) lat]));
            errShift = std::max (errShift, (double) std::abs (out[i] - in[i - (size_t) lat - 1]));
        }
        std::printf ("  (com 1 amostra de desalinhamento o erro seria %.4f)\n", errShift);
        std::printf ("  latência %d amostras, erro máx vs. entrada atrasada: %.4f\n", lat, err);
        check (err < 1.0e-4, "Blend 0% reproduz a entrada sem desalinhamento");
    }

    std::printf ("Cab ligado atenua agudos\n");
    {
        AuraBassAudioProcessor a, b;
        setParam (a, ParamIDs::cab, 0.0f); setParam (b, ParamIDs::cab, 1.0f);
        const double off = rms (run (a, 8000.0f, 0.1f), from), on = rms (run (b, 8000.0f, 0.1f), from);
        std::printf ("  8 kHz: sem cab %.1f dB, com cab %.1f dB\n",
                     juce::Decibels::gainToDecibels (off), juce::Decibels::gainToDecibels (on));
        check (on < off * 0.25, "cab corta pelo menos 12 dB em 8 kHz");
    }

    std::printf ("Botão DRIVE desligado = só EQ/Cab (sinal limpo alinhado)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::blend, 100.0f);
        setParam (p, ParamIDs::drive, 15.0f);
        setParam (p, ParamIDs::driveOn, 0.0f);
        std::vector<float> in;
        auto out = run (p, 100.0f, 0.25f, &in);
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, inputHpfHz, 0.707f);
        for (auto& x : in) x = hpf.processSample (x);
        const int lat = p.getLatencySamples();
        double err = 0;
        for (size_t i = from; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        const double h1 = harmonic (out, from, 100), h3 = harmonic (out, from, 300);
        std::printf ("  erro vs. limpo %.6f, H3 %.1f dB\n", err, juce::Decibels::gainToDecibels (h3 / h1 + 1e-12));
        check (err < 1.0e-4, "DRIVE off ignora Drive +15 dB e Blend 100%");
        check (finite (out), "sem NaN/Inf");
    }

    std::printf ("Botão DRIVE desligado mantém o EQ ativo\n");
    {
        AuraBassAudioProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, ParamIDs::cab, 0.0f); setParam (*p, ParamIDs::presence, 0.0f); setParam (*p, ParamIDs::driveOn, 0.0f); }
        setParam (b, ParamIDs::low, 12.0f);
        const double flat = rms (run (a, 50.0f, 0.1f), from), boosted = rms (run (b, 50.0f, 0.1f), from);
        std::printf ("  50 Hz com Low +12: %+.1f dB\n", juce::Decibels::gainToDecibels (boosted / flat));
        check (boosted > flat * 2.0, "Low continua atuando com o DRIVE desligado");
    }

    std::printf ("Troca de DRIVE no meio do som sem estalo\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 200; ++blk)
        {
            if (blk == 100) setParam (p, ParamIDs::driveOn, 0.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        double maxStep = 0, maxStepAround = 0;
        for (size_t i = 1; i < out.size(); ++i)
        {
            const double d = std::abs (out[i] - out[i - 1]);
            if (i > 20000 && i < 50000) maxStep = std::max (maxStep, d);
            if (i > 51000 && i < 53000) maxStepAround = std::max (maxStepAround, d);
        }
        std::printf ("  maior salto entre amostras: antes %.4f, na troca %.4f\n", maxStep, maxStepAround);
        check (maxStepAround <= maxStep * 1.05, "sem descontinuidade na troca");
    }

    std::printf ("Compensação de volume: DRIVE ligado x desligado (senoide 100 Hz, -12 dBFS)\n");
    for (float drive : { -15.0f, 0.0f, 15.0f })
    {
        AuraBassAudioProcessor on, off;
        for (auto* p : { &on, &off })
        {
            setParam (*p, ParamIDs::cab, 0.0f);
            setParam (*p, ParamIDs::presence, 0.0f);
            setParam (*p, ParamIDs::blend, 100.0f);
            setParam (*p, ParamIDs::drive, drive);
        }
        setParam (off, ParamIDs::driveOn, 0.0f);
        const double a = rms (run (on, 100.0f, 0.25f), from), b = rms (run (off, 100.0f, 0.25f), from);
        const double diff = juce::Decibels::gainToDecibels (a / b);
        std::printf ("  drive %+5.1f dB: diferença %+.1f dB\n", drive, diff);
        check (std::abs (diff) < 1.5, "volume parecido ligado/desligado");
    }

    std::printf ("CAB por resposta de impulso\n");
    {
        AuraBassAudioProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, ParamIDs::presence, 0.0f); setParam (*p, ParamIDs::driveOn, 0.0f); }
        b.setCabIR ("builtin:1");
        const double low4 = rms (run (a, 100.0f, 0.1f), from), hi4 = rms (run (a, 5000.0f, 0.1f), from);
        const double low15 = rms (run (b, 100.0f, 0.1f), from), hi15 = rms (run (b, 5000.0f, 0.1f), from);
        std::printf ("  4x10: 5 kHz %.1f dB abaixo de 100 Hz | 1x15: %.1f dB abaixo\n",
                     juce::Decibels::gainToDecibels (low4 / hi4), juce::Decibels::gainToDecibels (low15 / hi15));
        check (hi15 < hi4, "1x15 mais escura que 4x10");
        {
            AuraBassAudioProcessor c;
            setParam (c, ParamIDs::presence, 0.0f); setParam (c, ParamIDs::driveOn, 0.0f); setParam (c, ParamIDs::cab, 0.0f);
            for (float f : { 80.0f, 300.0f, 1000.0f })
            {
                AuraBassAudioProcessor on; setParam (on, ParamIDs::presence, 0.0f); setParam (on, ParamIDs::driveOn, 0.0f);
                AuraBassAudioProcessor off; setParam (off, ParamIDs::presence, 0.0f); setParam (off, ParamIDs::driveOn, 0.0f); setParam (off, ParamIDs::cab, 0.0f);
                std::printf ("  CAB 4x10 ligado vs desligado em %4.0f Hz: %+.1f dB\n", f,
                             juce::Decibels::gainToDecibels (rms (run (on, f, 0.1f), from) / rms (run (off, f, 0.1f), from)));
            }
        }
        check (b.getCabIRDisplayName() == "1x15", "nome da IR");
        b.setCabIR ("file:/nao/existe.wav");
        check (finite (run (b, 100.0f, 0.1f)), "arquivo inexistente volta para a IR embutida");
    }

    std::printf ("Presets de fábrica\n");
    {
        AuraBassAudioProcessor p;
        check (p.getNumPrograms() == 5, "5 presets");
        p.setCurrentProgram (3);
        const float d = p.apvts.getRawParameterValue (ParamIDs::drive)->load();
        std::printf ("  '%s' -> drive %.1f dB\n", p.getProgramName (3).toRawUTF8(), d);
        check (std::abs (d - 10.0f) < 0.05f, "preset aplica os valores");
        p.setCurrentProgram (1);
        check (p.apvts.getRawParameterValue (ParamIDs::driveOn)->load() < 0.5f, "Clean DI desliga o DRIVE");
    }

    std::printf ("Estado (IR e escala) salvo e restaurado\n");
    {
        AuraBassAudioProcessor a, b;
        a.setCabIR ("builtin:1");
        a.setUiScale (1.25f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCabIR() == "builtin:1" && std::abs (b.getUiScale() - 1.25f) < 0.001f, "IR e escala restauradas");
    }

    std::printf ("Mono\n");
    {
        AuraBassAudioProcessor p;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (p.setBusesLayout (mono), "layout mono aceito");
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (1, blockSize); juce::MidiBuffer midi;
        for (int i = 0; i < blockSize; ++i) buf.setSample (0, i, 0.5f * (float) std::sin (i * 0.05));
        p.processBlock (buf, midi);
        check (std::isfinite (buf.getSample (0, blockSize - 1)), "processa mono");
    }

    std::printf ("Blocos maiores que o anunciado (prepare 256, blocos de 4096 e tamanhos irregulares)\n");
    {
        AuraBassAudioProcessor a, b;
        const auto ref = runBlocks (a, 110.0f, 0.25f, 256, { 256 }, 48000);
        const auto big = runBlocks (b, 110.0f, 0.25f, 256, { 4096, 37, 1000, 256, 3 }, 48000);
        double err = 0;
        for (size_t i = 0; i < ref.size(); ++i) err = std::max (err, (double) std::abs (ref[i] - big[i]));
        std::printf ("  diferença máxima: %.2e\n", err);
        check (err < 1.0e-5, "mesmo resultado com qualquer tamanho de bloco");
        check (finite (big), "sem NaN/Inf");
    }

    std::printf ("Bypass da DAW mantém o alinhamento (atraso = latência)\n");
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (sr, blockSize);
        const int lat = p.getLatencySamples();
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> in, out;
        for (int blk = 0; blk < 8; ++blk)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.3f * (float) std::sin (0.013 * (blk * blockSize + i));
                buf.setSample (0, i, x); buf.setSample (1, i, x); in.push_back (x);
            }
            p.processBlockBypassed (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        double err = 0;
        for (size_t i = (size_t) lat; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        std::printf ("  latência %d amostras, erro %.2e\n", lat, err);
        check (lat > 0 && err < 1.0e-6, "bypass atrasa exatamente a latência");
    }

    std::printf ("Cauda informada à DAW\n");
    {
        AuraBassAudioProcessor p;
        std::printf ("  tail = %.3f s\n", p.getTailLengthSeconds());
        check (p.getTailLengthSeconds() > 0.02, "tail cobre a IR do CAB");
    }

    std::printf ("Silêncio na entrada = silêncio na saída\n");
    {
        AuraBassAudioProcessor p;
        const auto out = runBlocks (p, 100.0f, 0.0f, blockSize, { blockSize }, 48000);
        double peak = 0; for (auto x : out) peak = std::max (peak, (double) std::abs (x));
        std::printf ("  pico na saída: %.2e\n", peak);
        check (peak < 1.0e-6, "sem ruído/DC com entrada em silêncio");
    }

    std::printf ("Outras taxas de amostragem\n");
    for (double rate : { 44100.0, 96000.0, 192000.0 })
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (rate, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        bool ok = true;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int i = 0; i < 512; ++i) { const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * 512 + i) / rate); buf.setSample (0, i, x); buf.setSample (1, i, x); }
            p.processBlock (buf, midi);
            for (int i = 0; i < 512; ++i) ok = ok && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) < 4.0f;
        }
        std::printf ("  %.0f Hz: latência %d\n", rate, p.getLatencySamples());
        check (ok, "processa sem NaN/Inf nem explosão");
    }

    std::printf ("Entrada mono, saída estéreo\n");
    {
        AuraBassAudioProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::stereo());
        check (p.setBusesLayout (l), "layout mono→estéreo aceito");
        std::vector<float> right;
        const auto left = runBlocks (p, 100.0f, 0.25f, blockSize, { blockSize }, 24000, &right);
        double diff = 0, peak = 0;
        for (size_t i = 0; i < left.size(); ++i) { diff = std::max (diff, (double) std::abs (left[i] - right[i])); peak = std::max (peak, (double) std::abs (right[i])); }
        check (diff < 1.0e-7 && peak > 0.05, "os dois lados saem iguais");
    }

    std::printf ("Religar o DRIVE e o CAB depois de um tempo desligados, sem estalo\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::driveOn, 0.0f);
        setParam (p, ParamIDs::cab, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 240; ++blk)
        {
            if (blk == 100) setParam (p, ParamIDs::driveOn, 1.0f);
            if (blk == 160) setParam (p, ParamIDs::cab, 1.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 20000, 50000);
        const double driveOn = maxJump (out, 100 * blockSize - 100, 100 * blockSize + 3000);
        const double cabOn = maxJump (out, 160 * blockSize - 100, 160 * blockSize + 3000);
        const double after = maxJump (out, 200 * blockSize, 240 * blockSize);
        std::printf ("  maior salto: estável %.4f, DRIVE on %.4f, CAB on %.4f, depois %.4f\n", steady, driveOn, cabOn, after);
        check (driveOn <= std::max (steady, after) * 1.15, "DRIVE volta sem estalo");
        check (cabOn <= std::max (steady, after) * 1.15, "CAB volta sem estalo");
    }

    std::printf ("EQ girando rápido não gera degraus (zipper)\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::driveOn, 0.0f);
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 120; ++blk)
        {
            if (blk >= 40 && blk < 80) setParam (p, ParamIDs::low, (blk % 2 == 0) ? 12.0f : -12.0f);   // salta 24 dB a cada bloco
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 10000, 20000);
        const double moving = maxJump (out, 40 * blockSize, 80 * blockSize);
        std::printf ("  maior salto: parado %.4f, girando %.4f\n", steady, moving);
        check (moving < steady * 4.0 && finite (out), "variação suave do EQ");
    }

    std::printf ("Preset atual salvo na sessão\n");
    {
        AuraBassAudioProcessor a, b;
        a.setCurrentProgram (2);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCurrentProgram() == 2, "preset restaurado");
        check (std::abs (b.apvts.getRawParameterValue (ParamIDs::drive)->load() + 8.0f) < 0.05f, "valores do preset restaurados");
    }

    std::printf ("Afinador: precisão da detecção\n");
    {
        struct Case { const char* name; double hz; double fundamental, second, third; double rate; };
        const Case cases[] = {
            { "B0 (Si grave, 5 cordas)", 30.868, 1.0, 0.0, 0.0, 48000.0 },
            { "E1 (Mi)",                 41.203, 1.0, 0.0, 0.0, 48000.0 },
            { "A1 (Lá)",                 55.0,   1.0, 0.0, 0.0, 44100.0 },
            { "D2 (Ré)",                 73.416, 1.0, 0.0, 0.0, 96000.0 },
            { "G2 (Sol)",                97.999, 1.0, 0.0, 0.0, 48000.0 },
            { "E1 com harmônicos fortes", 41.203, 0.3, 1.0, 0.6, 48000.0 },
            { "A1 +12 cents",            55.0 * std::pow (2.0, 12.0 / 1200.0), 0.5, 0.8, 0.4, 48000.0 },
            { "G4 (casa 24)",            391.995, 1.0, 0.3, 0.0, 48000.0 },
        };
        for (const auto& c : cases)
        {
            Tuner t;
            t.prepare (c.rate);
            std::vector<float> block (480);
            const float* chans[1] = { block.data() };
            Tuner::Reading r;
            for (int blk = 0; blk < 60; ++blk)
            {
                for (int i = 0; i < 480; ++i)
                {
                    const double ph = 2.0 * juce::MathConstants<double>::pi * c.hz * (blk * 480 + i) / c.rate;
                    block[(size_t) i] = (float) (0.2 * (c.fundamental * std::sin (ph) + c.second * std::sin (2 * ph + 0.7) + c.third * std::sin (3 * ph + 1.9)));
                }
                t.push (chans, 1, 480);
                r = t.analyse();
            }
            const double errCents = r.valid ? 1200.0 * std::log2 ((double) r.hz / c.hz) : 999.0;
            std::printf ("  %-26s %8.3f Hz -> %8.3f Hz  %s%d  erro %+.2f cents\n", c.name, c.hz, (double) r.hz,
                         Tuner::noteName (r.midiNote).toRawUTF8(), Tuner::noteOctave (r.midiNote), errCents);
            check (r.valid && std::abs (errCents) < 1.0, "erro menor que 1 cent");
        }

        Tuner t;
        t.prepare (48000.0);
        std::vector<float> block (480);
        const float* chans[1] = { block.data() };
        juce::Random rnd (3);
        Tuner::Reading r;
        for (int blk = 0; blk < 60; ++blk)
        {
            for (auto& x : block) x = 0.2f * (rnd.nextFloat() * 2.0f - 1.0f);
            t.push (chans, 1, 480);
            r = t.analyse();
        }
        check (! r.valid, "ruído não vira nota");
        for (int blk = 0; blk < 60; ++blk)
        {
            std::fill (block.begin(), block.end(), 0.0f);
            t.push (chans, 1, 480);
            r = t.analyse();
        }
        check (! r.valid, "silêncio não vira nota");
    }

    std::printf ("Afinador ligado: saída muda, detector recebe a entrada\n");
    {
        AuraBassAudioProcessor p;
        setParam (p, ParamIDs::tuner, 1.0f);
        const auto out = runBlocks (p, 41.203f, 0.25f, blockSize, { blockSize }, 48000);
        double peak = 0; for (size_t i = 4800; i < out.size(); ++i) peak = std::max (peak, (double) std::abs (out[i]));
        const auto r = p.getTuner().analyse();
        std::printf ("  pico na saída %.2e, nota %s%d (%.2f Hz)\n", peak, Tuner::noteName (r.midiNote).toRawUTF8(), Tuner::noteOctave (r.midiNote), (double) r.hz);
        check (peak < 1.0e-6, "saída muda");
        check (r.valid && r.midiNote == 28 && std::abs (r.cents) < 1.0f, "detecta E1 pela entrada do plugin");

        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        AuraBassAudioProcessor q;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.apvts.getRawParameterValue (ParamIDs::tuner)->load() < 0.5f, "sessão nunca reabre com o afinador ligado");
    }

    std::printf ("Ligar/desligar o afinador sem estalo\n");
    {
        AuraBassAudioProcessor p;
        p.prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> buf (2, blockSize); juce::MidiBuffer midi;
        std::vector<float> out;
        for (int blk = 0; blk < 150; ++blk)
        {
            if (blk == 60) setParam (p, ParamIDs::tuner, 1.0f);
            if (blk == 100) setParam (p, ParamIDs::tuner, 0.0f);
            for (int i = 0; i < blockSize; ++i)
            {
                const float x = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (blk * blockSize + i) / sr);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < blockSize; ++i) out.push_back (buf.getSample (0, i));
        }
        const double steady = maxJump (out, 10000, 30000);
        const double sw = std::max (maxJump (out, 60 * blockSize - 50, 60 * blockSize + 3000), maxJump (out, 100 * blockSize - 50, 100 * blockSize + 3000));
        std::printf ("  maior salto: estável %.4f, nas trocas %.4f\n", steady, sw);
        check (sw <= steady * 1.15, "fade sem descontinuidade");
    }

    // Salva uma imagem da interface (útil para conferir o layout sem abrir um host)
    if (auto* outPath = std::getenv ("AURA_SNAPSHOT"))
    {
        AuraBassAudioProcessor p;
        if (auto* hzText = std::getenv ("AURA_SNAPSHOT_TUNER"))
        {
            // Afinador ligado, com uma nota tocando (frequência em Hz)
            setParam (p, ParamIDs::tuner, 1.0f);
            runBlocks (p, juce::String (hzText).getFloatValue(), 0.25f, blockSize, { blockSize }, 48000);
        }
        if (std::getenv ("AURA_SNAPSHOT_OFF") != nullptr)
        {
            setParam (p, ParamIDs::driveOn, 0.0f);
            setParam (p, ParamIDs::cab, 0.0f);
            setParam (p, ParamIDs::drive, 9.5f);
            setParam (p, ParamIDs::low, -12.0f);
        }
        if (auto* sc = std::getenv ("AURA_SNAPSHOT_SCALE"))
            p.setUiScale (juce::String (sc).getFloatValue());
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        std::printf ("Editor %dx%d\n", ed->getWidth(), ed->getHeight());
        if (std::getenv ("AURA_SNAPSHOT_TUNER") != nullptr)
            for (int i = 0; i < 12; ++i)     // deixa o timer da interface ler o detector
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File file (outPath);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("Snapshot salvo em %s\n", outPath);
    }

    std::printf (failures == 0 ? "\nTodos os testes passaram.\n" : "\n%d teste(s) falharam.\n", failures);
    return failures == 0 ? 0 : 1;
}
