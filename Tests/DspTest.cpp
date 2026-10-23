// Teste offline do DSP: roda o processador com senoides e checa níveis, THD,
// estabilidade numérica e o alinhamento do Blend (sem phasing).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
    constexpr double sr = 48000.0;
    constexpr int blockSize = 512;

    void setParam (GritDIAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Processa 1 s de senoide e devolve a saída (descartando o primeiro 0,5 s)
    std::vector<float> run (GritDIAudioProcessor& p, float freq, float amp, std::vector<float>* inputOut = nullptr)
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

    std::printf ("Drive x nível/THD (senoide 100 Hz, -12 dBFS, cab desligado, EQ plano)\n");
    for (float drive : { 0.0f, 2.5f, 5.0f, 7.5f, 10.0f })
    {
        GritDIAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::drive, drive);
        auto out = run (p, 100.0f, 0.25f);
        const double h1 = harmonic (out, from, 100), h2 = harmonic (out, from, 200), h3 = harmonic (out, from, 300),
                     h4 = harmonic (out, from, 400), h5 = harmonic (out, from, 500);
        const double thd = std::sqrt (h2*h2 + h3*h3 + h4*h4 + h5*h5) / h1 * 100.0;
        std::printf ("  drive %4.1f  saída %6.1f dBFS  THD %5.1f%%  H2 %5.1f dB  H3 %5.1f dB\n",
                     drive, juce::Decibels::gainToDecibels (rms (out, from) * std::sqrt (2.0)), thd,
                     juce::Decibels::gainToDecibels (h2 / h1), juce::Decibels::gainToDecibels (h3 / h1));
        check (finite (out), "sem NaN/Inf");
    }

    std::printf ("Blend 0%% = sinal limpo alinhado (sem phasing)\n");
    {
        GritDIAudioProcessor p;
        setParam (p, ParamIDs::cab, 0.0f);
        setParam (p, ParamIDs::presence, 0.0f);
        setParam (p, ParamIDs::blend, 0.0f);
        setParam (p, ParamIDs::drive, 10.0f);
        std::vector<float> in;
        auto out = run (p, 440.0f, 0.25f, &in);
        const int lat = p.getLatencySamples();
        // Referência: entrada passada pelo mesmo HPF de 25 Hz do plugin
        juce::dsp::IIR::Filter<float> hpf;
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 25.0f, 0.707f);
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
        GritDIAudioProcessor a, b;
        setParam (a, ParamIDs::cab, 0.0f); setParam (b, ParamIDs::cab, 1.0f);
        const double off = rms (run (a, 8000.0f, 0.1f), from), on = rms (run (b, 8000.0f, 0.1f), from);
        std::printf ("  8 kHz: sem cab %.1f dB, com cab %.1f dB\n",
                     juce::Decibels::gainToDecibels (off), juce::Decibels::gainToDecibels (on));
        check (on < off * 0.25, "cab corta pelo menos 12 dB em 8 kHz");
    }

    std::printf ("Mono\n");
    {
        GritDIAudioProcessor p;
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

    // Salva uma imagem da interface (útil para conferir o layout sem abrir um host)
    if (auto* outPath = std::getenv ("GRIT_SNAPSHOT"))
    {
        GritDIAudioProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
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
