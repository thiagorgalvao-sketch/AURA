// AURA Bass — afinador da versão web. Porta de Source/Tuner.h (método YIN).
// Recebe do AudioWorklet blocos já filtrados e decimados (~12 kHz).
(function (root) {
  const WINDOW = 2048, MIN_HZ = 27, MAX_HZ = 650, A4 = 440;
  const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

  class TunerDetector {
    constructor() { this.window = new Float32Array(WINDOW); this.diff = new Float32Array(WINDOW); this.filled = 0; this.rate = 12000; }
    reset() { this.window.fill(0); this.filled = 0; }

    push(chunk, rate) {
      this.rate = rate;
      const n = Math.min(chunk.length, WINDOW);
      this.window.copyWithin(0, n);
      this.window.set(chunk.subarray(chunk.length - n), WINDOW - n);
      this.filled = Math.min(WINDOW, this.filled + n);
    }

    analyse() {
      const r = { valid: false, hz: 0, midiNote: 0, cents: 0, levelDb: -100 };
      if (this.filled < WINDOW) return r;
      const x = this.window, d = this.diff, rate = this.rate;
      const tauMax = Math.min(WINDOW / 2, Math.floor(rate / MIN_HZ));
      const tauMin = Math.max(2, Math.floor(rate / MAX_HZ));
      const len = WINDOW - tauMax;

      let energy = 0;
      for (let j = 0; j < len; j++) energy += x[j] * x[j];
      r.levelDb = 20 * Math.log10(Math.max(1e-5, Math.sqrt(energy / len)));
      if (r.levelDb < -60) return r;

      d[0] = 1;
      let running = 0;
      for (let tau = 1; tau <= tauMax; tau++) {
        let s = 0;
        for (let j = 0; j < len; j++) { const e = x[j] - x[j + tau]; s += e * e; }
        running += s;
        d[tau] = running > 0 ? (s * tau) / running : 1;
      }

      // primeiro vale abaixo do limiar (evita erro de oitava); senão, o menor vale
      let best = -1;
      for (let tau = tauMin; tau < tauMax; tau++) {
        if (d[tau] < 0.12) {
          while (tau + 1 < tauMax && d[tau + 1] < d[tau]) tau++;
          best = tau;
          break;
        }
      }
      if (best < 0) {
        best = tauMin;
        for (let tau = tauMin; tau < tauMax; tau++) if (d[tau] < d[best]) best = tau;
        if (d[best] > 0.3) return r;
      }
      if (best <= tauMin || best >= tauMax - 1) return r;

      const a = d[best - 1], b = d[best], c = d[best + 1], den = a - 2 * b + c;
      const shift = Math.abs(den) > 1e-9 ? Math.max(-1, Math.min(1, (0.5 * (a - c)) / den)) : 0;
      r.hz = rate / (best + shift);
      const note = 69 + 12 * Math.log2(r.hz / A4);
      r.midiNote = Math.round(note);
      r.cents = (note - r.midiNote) * 100;
      r.valid = true;
      return r;
    }
  }

  const api = {
    TunerDetector,
    noteName: (m) => NAMES[((m % 12) + 12) % 12],
    noteOctave: (m) => Math.floor(m / 12) - 1,
  };
  if (typeof module !== 'undefined') module.exports = api; else root.AuraTuner = api;
})(typeof self !== 'undefined' ? self : this);
