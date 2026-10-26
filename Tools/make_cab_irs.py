#!/usr/bin/env python3
"""
Gera as respostas de impulso (IR) embutidas do CAB a partir de um modelo de caixa de baixo:
corte de subgrave e ressonância do gabinete, colorido dos médios, picos/vales de quebra do cone,
queda íngreme dos agudos e pequenas reflexões internas. A resposta é convertida para fase mínima
(como a de um falante microfonado de perto).

Saída: Resources/ir/cab_4x10.wav e Resources/ir/cab_1x15.wav (48 kHz, 24 bits, mono)
Uso:   python3 Tools/make_cab_irs.py
"""
import os
import struct
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Resources", "ir")
SR = 48000
NFFT = 16384
LENGTH = 2048            # ~43 ms


def db(x):
    return 10 ** (x / 20.0)


def peak(f, f0, gain_db, q):
    # curva de sino em dB (aproximação de um filtro peaking)
    bw = f0 / q
    return gain_db / (1.0 + ((f - f0) / (bw * 0.5)) ** 2)


def highpass(f, fc, order):
    return 1.0 / np.sqrt(1.0 + (fc / np.maximum(f, 1.0)) ** (2 * order))


def lowpass(f, fc, order):
    return 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order))


def ripple(f, seed, amount_db, start=900.0):
    rng = np.random.default_rng(seed)
    logf = np.log2(np.maximum(f, 20.0))
    knots = np.linspace(np.log2(20.0), np.log2(SR / 2), 48)
    vals = rng.normal(0.0, 1.0, knots.size)
    r = np.interp(logf, knots, vals)
    return r * amount_db * np.clip((f - start) / start, 0.0, 1.0)


def min_phase_ir(mag):
    """Magnitude (0..Nyquist) -> resposta ao impulso de fase mínima (método do cepstro)."""
    full = np.concatenate([mag, mag[-2:0:-1]])
    logmag = np.log(np.maximum(full, 1e-6))
    cep = np.fft.ifft(logmag).real
    n = full.size
    w = np.zeros(n)
    w[0] = 1.0
    w[1:n // 2] = 2.0
    w[n // 2] = 1.0
    spec = np.exp(np.fft.fft(cep * w))
    return np.fft.ifft(spec).real


def build(kind):
    f = np.linspace(0, SR / 2, NFFT // 2 + 1)
    if kind == "4x10":
        g = 20 * np.log10(highpass(f, 58.0, 2) * lowpass(f, 4600.0, 6) + 1e-9)
        g += peak(f, 95.0, 4.0, 1.4)          # ressonância do gabinete
        g += peak(f, 420.0, -2.5, 1.1)        # leve "escavado" nos médios-graves
        g += peak(f, 1250.0, 1.5, 1.5)
        g += peak(f, 2300.0, 3.0, 2.2)        # presença do cone de 10"
        g += peak(f, 3300.0, 2.0, 4.0)
        g += peak(f, 4300.0, -6.0, 5.0)       # vale de quebra do cone
        g += ripple(f, 410, 0.9)
        reflections = [(0.00038, -13.0), (0.0011, -19.0), (0.0024, -26.0)]
    else:
        g = 20 * np.log10(highpass(f, 42.0, 2) * lowpass(f, 2900.0, 6) + 1e-9)
        g += peak(f, 72.0, 5.0, 1.3)
        g += peak(f, 240.0, 2.0, 1.0)
        g += peak(f, 700.0, -4.0, 1.2)
        g += peak(f, 1600.0, 2.0, 2.5)
        g += peak(f, 2500.0, -4.0, 4.0)
        g += ripple(f, 1510, 0.8, 600.0)
        reflections = [(0.00052, -12.0), (0.0016, -18.0), (0.0031, -27.0)]

    ir = min_phase_ir(db(g))[:LENGTH * 2]

    # reflexões internas curtas (gabinete/microfone), com agudos atenuados
    out = ir.copy()
    for delay, level in reflections:
        d = int(round(delay * SR))
        refl = np.convolve(ir, np.ones(6) / 6.0, mode="full")[: ir.size]
        out[d:] += refl[: ir.size - d] * db(level)

    out = out[:LENGTH]
    fade = np.ones(LENGTH)
    fl = LENGTH // 3
    fade[-fl:] = 0.5 * (1 + np.cos(np.linspace(0, np.pi, fl)))
    out *= fade
    out /= np.max(np.abs(out)) * 1.02
    return out


def write_wav24(path, data):
    pcm = np.clip(np.round(data * 8388607.0), -8388608, 8388607).astype(np.int32)
    raw = bytearray()
    for v in pcm:
        raw += struct.pack("<i", int(v))[:3]
    with open(path, "wb") as fh:
        fh.write(b"RIFF" + struct.pack("<I", 36 + len(raw)) + b"WAVE")
        fh.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, SR, SR * 3, 3, 24))
        fh.write(b"data" + struct.pack("<I", len(raw)) + raw)


def main():
    os.makedirs(OUT, exist_ok=True)
    for kind in ("4x10", "1x15"):
        ir = build(kind)
        write_wav24(os.path.join(OUT, "cab_%s.wav" % kind), ir)
        spec = 20 * np.log10(np.abs(np.fft.rfft(ir, 8192)) + 1e-9)
        fr = np.fft.rfftfreq(8192, 1 / SR)
        ref = spec[np.argmin(np.abs(fr - 500))]
        rep = ", ".join("%dHz %+.1f" % (hz, spec[np.argmin(np.abs(fr - hz))] - ref) for hz in (40, 80, 100, 250, 1000, 2500, 4000, 8000))
        print(kind, rep)


if __name__ == "__main__":
    main()
