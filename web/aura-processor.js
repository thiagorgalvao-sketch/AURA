// AURA Bass — processamento de áudio da versão web
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// AURA Bass — motor de áudio da versão web (AudioWorklet).
// Porta do processamento do plugin (Source/PluginProcessor.cpp):
//   entrada -> HPF 18 Hz -> pré-ênfase -> [4x oversampling] Drive -> soft clip assimétrico
//   -> remove DC + LPF 7 kHz -> compensação de volume -> Blend com o limpo alinhado
//   -> Low / Mid / High / Presence -> Level
// O CAB (convolução) fica num ConvolverNode nativo, depois deste processador.
// O mesmo arquivo roda no Node para os testes (Tests/web-dsp-test.js).

const DRIVE_REF_DB = 20;      // Drive 0 dB = +20 dB antes do clipper
const CLIP_BIAS = 0.15;       // assimetria -> harmônicos pares
const WET_TRIM = 0.7;
const TANH_BIAS = Math.tanh(CLIP_BIAS);
const OS = 4;                 // oversampling
const FIR_TAPS = 65;          // filtro do oversampling (atraso total = 16 amostras)
const LATENCY = 16;

const dbToGain = (db) => Math.pow(10, db / 20);
const shape = (x) => Math.tanh(x + CLIP_BIAS) - TANH_BIAS;

// Ganho que deixa o som saturado no mesmo volume do limpo (referência: senoide de -12 dBFS)
function levelCompensation(driveDb) {
  const g = dbToGain(driveDb + DRIVE_REF_DB);
  const makeup = WET_TRIM / Math.tanh(g);
  const n = 256;
  const y = new Float64Array(n);
  let mean = 0, sumIn = 0;
  for (let i = 0; i < n; i++) {
    const x = 0.25 * Math.sin((2 * Math.PI * i) / 64);
    y[i] = shape(x * g) * makeup;
    mean += y[i];
    sumIn += x * x;
  }
  mean /= n;
  let sumOut = 0;
  for (let i = 0; i < n; i++) sumOut += (y[i] - mean) * (y[i] - mean);
  return Math.sqrt(sumIn / Math.max(1e-12, sumOut));
}

// ---- Biquads (mesmas fórmulas do JUCE) -------------------------------------
class Biquad {
  constructor() { this.b0 = 1; this.b1 = 0; this.b2 = 0; this.a1 = 0; this.a2 = 0; this.z1 = 0; this.z2 = 0; }
  set(c) { [this.b0, this.b1, this.b2, this.a1, this.a2] = c; }
  reset() { this.z1 = 0; this.z2 = 0; }
  process(x) {
    const y = this.b0 * x + this.z1;
    this.z1 = this.b1 * x - this.a1 * y + this.z2;
    this.z2 = this.b2 * x - this.a2 * y;
    return y;
  }
}
const norm = (b0, b1, b2, a0, a1, a2) => [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0];
const Coefs = {
  highPass(sr, f, q) {
    const n = Math.tan((Math.PI * f) / sr), n2 = n * n, c1 = 1 / (1 + n / q + n2);
    return [c1, -2 * c1, c1, c1 * 2 * (n2 - 1), c1 * (1 - n / q + n2)];
  },
  lowPass(sr, f, q) {
    const n = 1 / Math.tan((Math.PI * f) / sr), n2 = n * n, c1 = 1 / (1 + n / q + n2);
    return [c1, 2 * c1, c1, c1 * 2 * (1 - n2), c1 * (1 - n / q + n2)];
  },
  peak(sr, f, q, gain) {
    const A = Math.sqrt(Math.max(gain, 1e-9)), w = (2 * Math.PI * f) / sr;
    const alpha = Math.sin(w) / (2 * q), c2 = -2 * Math.cos(w);
    return norm(1 + alpha * A, c2, 1 - alpha * A, 1 + alpha / A, c2, 1 - alpha / A);
  },
  lowShelf(sr, f, q, gain) {
    const A = Math.sqrt(Math.max(gain, 1e-9)), am = A - 1, ap = A + 1, w = (2 * Math.PI * f) / sr;
    const co = Math.cos(w), beta = (Math.sin(w) * Math.sqrt(A)) / q, amc = am * co;
    return norm(A * (ap - amc + beta), A * 2 * (am - ap * co), A * (ap - amc - beta),
                ap + amc + beta, -2 * (am + ap * co), ap + amc - beta);
  },
  highShelf(sr, f, q, gain) {
    const A = Math.sqrt(Math.max(gain, 1e-9)), am = A - 1, ap = A + 1, w = (2 * Math.PI * f) / sr;
    const co = Math.cos(w), beta = (Math.sin(w) * Math.sqrt(A)) / q, amc = am * co;
    return norm(A * (ap + amc + beta), A * -2 * (am + ap * co), A * (ap + amc - beta),
                ap - amc + beta, 2 * (am - ap * co), ap - amc - beta);
  },
};

// Passa-baixas do oversampling: sinc com janela de Kaiser, corte em 0,45 da taxa base
function designFir() {
  const h = new Float64Array(FIR_TAPS), fc = 0.45 / OS, mid = (FIR_TAPS - 1) / 2, beta = 9;
  const i0 = (x) => { let s = 1, t = 1; for (let k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
  let sum = 0;
  for (let j = 0; j < FIR_TAPS; j++) {
    const m = j - mid;
    const sinc = m === 0 ? 2 * fc : Math.sin(2 * Math.PI * fc * m) / (Math.PI * m);
    const w = i0(beta * Math.sqrt(1 - (m / mid) * (m / mid))) / i0(beta);
    h[j] = sinc * w;
    sum += h[j];
  }
  for (let j = 0; j < FIR_TAPS; j++) h[j] /= sum;
  return h;
}

// ---- Processamento ---------------------------------------------------------
class AuraDsp {
  constructor(sr) {
    this.sr = sr;
    this.p = { level: 0, drive: 0, low: 0, mid: 0, high: 0, presence: 3, blend: 100, driveOn: true, tuner: false, channel: 0 };
    this.fir = designFir();
    this.xHist = new Float64Array(32); this.xPos = 0;       // entrada do upsampler
    this.uRing = new Float64Array(128); this.uPos = 0;      // sinal em 4x
    this.dryRing = new Float64Array(LATENCY); this.dryPos = 0;

    this.inHpf = new Biquad(); this.inHpf.set(Coefs.highPass(sr, 18, 0.707));
    this.preEmph = new Biquad(); this.preEmph.set(Coefs.peak(sr, 800, 0.7, dbToGain(6)));
    this.dcBlock = new Biquad(); this.dcBlock.set(Coefs.highPass(sr, 20, 0.707));
    this.postLpf = new Biquad(); this.postLpf.set(Coefs.lowPass(sr, Math.min(7000, sr * 0.45), 0.707));
    this.tone = [new Biquad(), new Biquad(), new Biquad(), new Biquad()];
    this.toneDb = [0, 0, 0, 3];
    this.updateTone();

    this.kSmooth = 1 - Math.exp(-1 / (0.012 * sr));          // ~50 ms até assentar
    this.kMute = 1 - Math.exp(-1 / (0.005 * sr));
    this.driveGain = dbToGain(DRIVE_REF_DB);
    this.blendMix = 1; this.outGain = 1; this.muteGain = 1;
    this.autoGain = levelCompensation(0); this.autoTarget = this.autoGain; this.lastDrive = 0;
    this.wetIdle = false;

    // Afinador: passa-baixas + decimação para ~12 kHz
    this.tunerLp = [new Biquad(), new Biquad()];
    const lp = Coefs.lowPass(sr, Math.min(1200, sr * 0.2), 0.707);
    this.tunerLp.forEach((f) => f.set(lp));
    this.decimation = Math.max(1, Math.round(sr / 12000));
    this.decPhase = 0;
    this.tunerBuf = new Float32Array(256); this.tunerFill = 0;
    this.onTunerData = null;
  }

  get tunerRate() { return this.sr / this.decimation; }

  setParams(params) { Object.assign(this.p, params); }

  updateTone() {
    const sr = this.sr, nyq = sr * 0.45, t = this.toneDb;
    this.tone[0].set(Coefs.lowShelf(sr, 80, 0.707, dbToGain(t[0])));
    this.tone[1].set(Coefs.peak(sr, 600, 0.8, dbToGain(t[1])));
    this.tone[2].set(Coefs.highShelf(sr, Math.min(2500, nyq), 0.707, dbToGain(t[2])));
    this.tone[3].set(Coefs.peak(sr, Math.min(5000, nyq), 1.0, dbToGain(t[3])));
  }

  // inputs: array de canais (Float32Array) ou vazio; out: Float32Array mono
  process(inputs, out) {
    const p = this.p, n = out.length, fir = this.fir;
    const c0 = inputs[0], c1 = inputs[1] || inputs[0];

    // EQ: ganhos suavizados, coeficientes recalculados por bloco enquanto mudam
    const target = [p.low, p.mid, p.high, p.presence];
    let moving = false;
    for (let i = 0; i < 4; i++) {
      const d = target[i] - this.toneDb[i];
      if (d !== 0) { moving = true; this.toneDb[i] = Math.abs(d) < 0.02 ? target[i] : this.toneDb[i] + d * 0.25; }
    }
    if (moving) this.updateTone();

    if (p.drive !== this.lastDrive) { this.lastDrive = p.drive; this.autoTarget = levelCompensation(p.drive); }
    const driveTarget = dbToGain(p.drive + DRIVE_REF_DB);
    const blendTarget = p.driveOn ? p.blend / 100 : 0;
    const outTarget = dbToGain(p.level);
    const muteTarget = p.tuner ? 0 : 1;
    const k = this.kSmooth;

    for (let i = 0; i < n; i++) {
      let x = 0;
      if (c0) x = p.channel === 1 ? c1[i] : p.channel === 2 ? 0.5 * (c0[i] + c1[i]) : c0[i];

      if (p.tuner) {
        const t = this.tunerLp[1].process(this.tunerLp[0].process(x));
        if (++this.decPhase >= this.decimation) {
          this.decPhase = 0;
          this.tunerBuf[this.tunerFill++] = t;
          if (this.tunerFill === this.tunerBuf.length) {
            if (this.onTunerData) this.onTunerData(this.tunerBuf.slice());
            this.tunerFill = 0;
          }
        }
      }

      x = this.inHpf.process(x);

      // limpo, atrasado pela latência do oversampling (o Blend não dá phasing)
      const dry = this.dryRing[this.dryPos];
      this.dryRing[this.dryPos] = x;
      this.dryPos = (this.dryPos + 1) % LATENCY;

      this.driveGain += (driveTarget - this.driveGain) * k;
      this.blendMix += (blendTarget - this.blendMix) * k;
      this.outGain += (outTarget - this.outGain) * k;
      this.autoGain += (this.autoTarget - this.autoGain) * k;
      this.muteGain += (muteTarget - this.muteGain) * this.kMute;

      let y;
      if (blendTarget === 0 && this.blendMix < 1e-4) {
        // DRIVE desligado: a saturação não é processada
        this.blendMix = 0;
        this.wetIdle = true;
        y = dry;
      } else {
        if (this.wetIdle) {
          this.xHist.fill(0); this.uRing.fill(0);
          this.preEmph.reset(); this.dcBlock.reset(); this.postLpf.reset();
          this.wetIdle = false;
        }
        const g = this.driveGain, makeup = WET_TRIM / Math.tanh(g);
        this.xPos = (this.xPos + 1) & 31;
        this.xHist[this.xPos] = this.preEmph.process(x);

        let v = 0;
        for (let ph = 0; ph < OS; ph++) {
          let u = 0;
          for (let j = ph, kk = 0; j < FIR_TAPS; j += OS, kk++) u += fir[j] * this.xHist[(this.xPos - kk) & 31];
          this.uPos = (this.uPos + 1) & 127;
          this.uRing[this.uPos] = shape(u * OS * g) * makeup;
          if (ph === 0) {
            // decimação: usa a amostra de fase 0 (atraso total exato de 16 amostras)
            for (let j = 0; j < FIR_TAPS; j++) v += fir[j] * this.uRing[(this.uPos - j) & 127];
          }
        }
        const wet = this.postLpf.process(this.dcBlock.process(v));
        const m = this.blendMix;
        y = dry * (1 - m) + wet * this.autoGain * m;
      }

      y = this.tone[3].process(this.tone[2].process(this.tone[1].process(this.tone[0].process(y))));
      out[i] = y * this.outGain * this.muteGain;
    }
  }
}

// ---- AudioWorklet ----------------------------------------------------------
if (typeof registerProcessor !== 'undefined') {
  class AuraProcessor extends AudioWorkletProcessor {
    constructor() {
      super();
      this.dsp = new AuraDsp(sampleRate);
      this.dsp.onTunerData = (buf) => this.port.postMessage({ tuner: buf, rate: this.dsp.tunerRate }, [buf.buffer]);
      this.port.onmessage = (e) => { if (e.data.params) this.dsp.setParams(e.data.params); };
    }
    process(inputs, outputs) {
      const out = outputs[0];
      if (!out || !out[0]) return true;
      this.dsp.process(inputs[0] || [], out[0]);
      for (let ch = 1; ch < out.length; ch++) out[ch].set(out[0]);
      return true;
    }
  }
  registerProcessor('aura-bass', AuraProcessor);
}

if (typeof module !== 'undefined') module.exports = { AuraDsp, levelCompensation, shape, LATENCY, Coefs };
