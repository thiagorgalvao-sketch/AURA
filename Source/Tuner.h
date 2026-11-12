#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Afinador: detecção de altura para baixo (Si grave de 5 cordas, 30,9 Hz, até ~650 Hz).
//
// Thread de áudio: push() soma os canais, filtra (passa-baixas em 1,2 kHz), reduz a taxa
// para ~12 kHz e escreve numa fila sem travas. Nada é alocado.
// Thread da interface: analyse() lê a fila, mantém uma janela de ~170 ms e estima a
// fundamental pelo método YIN (diferença normalizada + interpolação parabólica).
//==============================================================================
class Tuner
{
public:
    struct Reading
    {
        bool valid = false;
        float hz = 0.0f;
        int midiNote = 0;        // nota mais próxima (Lá 440 = 69)
        float cents = 0.0f;      // desvio em relação à nota mais próxima (-50..+50)
        float levelDb = -100.0f;
    };

    static constexpr int windowSize = 2048;
    static constexpr float minHz = 27.0f, maxHz = 650.0f;
    static constexpr float referenceA4 = 440.0f;

    Tuner() { fifoData.resize ((size_t) fifo.getTotalSize()); window.assign (windowSize, 0.0f); diff.resize (windowSize); }

    // Fora da thread de áudio (prepareToPlay)
    void prepare (double sampleRate)
    {
        const juce::SpinLock::ScopedLockType sl (analysisLock);
        decimation = juce::jmax (1, juce::roundToInt (sampleRate / 12000.0));
        decimatedRate.store (sampleRate / decimation);
        const auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, juce::jmin (1200.0, sampleRate * 0.2), 0.707f);
        for (auto& f : lowpass) { f.coefficients = c; f.reset(); }
        phase = 0;
        fifo.reset();
        std::fill (window.begin(), window.end(), 0.0f);
        filled = 0;
    }

    // Thread de áudio
    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0) return;
        const float norm = 1.0f / (float) numChannels;
        for (int i = 0; i < numSamples; ++i)
        {
            float x = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch) x += channels[ch][i];
            x = lowpass[1].processSample (lowpass[0].processSample (x * norm));
            if (++phase >= decimation)
            {
                phase = 0;
                const auto scope = fifo.write (1);
                if (scope.blockSize1 > 0) fifoData[(size_t) scope.startIndex1] = x;
            }
        }
        for (auto& f : lowpass) f.snapToZero();
    }

    // Thread da interface. Esvazia a fila mesmo sem analisar, para o dado não ficar velho.
    Reading analyse()
    {
        Reading r;
        const juce::SpinLock::ScopedTryLockType sl (analysisLock);
        if (! sl.isLocked()) return r;

        // Puxa o que chegou para a janela deslizante
        {
            const auto scope = fifo.read (fifo.getNumReady());
            auto append = [this] (int start, int n)
            {
                if (n <= 0) return;
                if (n >= windowSize) { start += n - windowSize; n = windowSize; }
                std::move (window.begin() + n, window.end(), window.begin());
                std::copy_n (fifoData.begin() + start, n, window.end() - n);
                filled = juce::jmin (windowSize, filled + n);
            };
            append (scope.startIndex1, scope.blockSize1);
            append (scope.startIndex2, scope.blockSize2);
        }
        if (filled < windowSize) return r;

        const double rate = decimatedRate.load();
        const int tauMax = juce::jmin (windowSize / 2, (int) (rate / minHz));
        const int tauMin = juce::jmax (2, (int) (rate / maxHz));
        const int len = windowSize - tauMax;
        const float* x = window.data();

        double energy = 0.0;
        for (int j = 0; j < len; ++j) energy += (double) x[j] * x[j];
        r.levelDb = (float) juce::Decibels::gainToDecibels (std::sqrt (energy / len), -100.0);
        if (r.levelDb < -60.0f) return r;

        // YIN: diferença e diferença normalizada pela média cumulativa
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            const float* y = x + tau;
            for (int j = 0; j < len; ++j) { const float e = x[j] - y[j]; d += e * e; }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        // Primeiro vale abaixo do limiar (evita erros de oitava); senão, o menor vale
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < 0.12f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            best = tauMin;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (diff[(size_t) tau] < diff[(size_t) best]) best = tau;
            if (diff[(size_t) best] > 0.3f) return r;      // sem altura definida (ruído, acorde)
        }
        if (best <= tauMin || best >= tauMax - 1) return r;

        // Interpolação parabólica do vale
        const float a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
        const float denom = a - 2.0f * b + c;
        const float shift = std::abs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f;
        const double period = (double) best + juce::jlimit (-1.0f, 1.0f, shift);

        r.hz = (float) (rate / period);
        const double note = 69.0 + 12.0 * std::log2 ((double) r.hz / referenceA4);
        r.midiNote = juce::roundToInt (note);
        r.cents = (float) ((note - r.midiNote) * 100.0);
        r.valid = true;
        return r;
    }

    static juce::String noteName (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((midiNote % 12) + 12) % 12];
    }
    static int noteOctave (int midiNote) { return (int) std::floor (midiNote / 12.0) - 1; }

private:
    // Áudio
    juce::dsp::IIR::Filter<float> lowpass[2];
    int decimation = 4, phase = 0;
    juce::AbstractFifo fifo { 8192 };
    std::vector<float> fifoData;
    std::atomic<double> decimatedRate { 12000.0 };

    // Interface
    juce::SpinLock analysisLock;
    std::vector<float> window, diff;
    int filled = 0;
};
