// VERGANI PEDALBOARD — processador de áudio do detune (Web Audio não tem pronto).
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
//
// Detune por linha de atraso: a posição de leitura anda numa velocidade
// ligeiramente diferente da de escrita, o que muda a altura em "cents".
// Duas leituras defasadas em meio ciclo se revezam; a troca acontece numa
// janela curta no fim do ciclo, e no resto do tempo só uma leitura soa
// (isso evita o efeito de "pente" que o cruzamento permanente causaria).

'use strict';

const JANELA_MS = 40;      // tamanho do ciclo de leitura
const BORDA = 0.15;        // fração do ciclo gasta em cada troca

function suave(x) { return x <= 0 ? 0 : x >= 1 ? 1 : x * x * (3 - 2 * x); }

class DetuneCore {
  constructor(sr) {
    this.sr = sr;
    this.W = Math.round((JANELA_MS / 1000) * sr);
    this.N = 1;
    while (this.N < this.W * 2 + 8) this.N <<= 1;
    this.mask = this.N - 1;
    this.buf = new Float32Array(this.N);
    this.w = 0;
    this.fase = 0.5;       // começa com a leitura no meio do ciclo (sem troca)
  }

  /** x: entrada; out: saída; n: amostras. cents -50..50, mix 0..1. */
  process(x, cents, mix, out, n) {
    const { buf, mask, W } = this;
    const ratio = Math.pow(2, cents / 1200);
    const dFase = (1 - ratio) / W;
    let w = this.w, fase = this.fase;

    for (let i = 0; i < n; i++) {
      const s = x ? x[i] : 0;
      buf[w & mask] = s;

      const pa = fase;
      let pb = pa + 0.5; if (pb >= 1) pb -= 1;

      const ya = this.le(w, pa * W + 1);
      const yb = this.le(w, pb * W + 1);

      // peso da leitura A: 1 no meio do ciclo, cai a 0 nas pontas (onde ela "salta")
      const wa = suave(Math.min(pa, 1 - pa) / BORDA);
      const molhado = ya * wa + yb * (1 - wa);

      out[i] = s * (1 - mix) + molhado * mix;

      w++;
      fase += dFase;
      if (fase >= 1) fase -= 1; else if (fase < 0) fase += 1;
    }
    this.w = w & 0x3fffffff;
    this.fase = fase;
  }

  le(w, atraso) {
    const rp = w - atraso;
    const i0 = Math.floor(rp);
    const f = rp - i0;
    const a = this.buf[i0 & this.mask], b = this.buf[(i0 + 1) & this.mask];
    return a + (b - a) * f;
  }
}

if (typeof AudioWorkletProcessor !== 'undefined') {
  class Detune extends AudioWorkletProcessor {
    static get parameterDescriptors() {
      return [
        { name: 'cents', defaultValue: 14, minValue: -50, maxValue: 50, automationRate: 'k-rate' },
        { name: 'mix',   defaultValue: 0.5, minValue: 0, maxValue: 1,   automationRate: 'k-rate' },
      ];
    }
    constructor() {
      super();
      this.core = new DetuneCore(sampleRate);
    }
    process(inputs, outputs, params) {
      const out = outputs[0];
      if (!out || out.length === 0) return true;
      const x = (inputs[0] && inputs[0][0]) ? inputs[0][0] : null;
      const n = out[0].length;
      this.core.process(x, params.cents[0], params.mix[0], out[0], n);
      for (let c = 1; c < out.length; c++) out[c].set(out[0]);
      return true;
    }
  }
  registerProcessor('detune', Detune);
}

if (typeof module !== 'undefined') module.exports = { DetuneCore, JANELA_MS };
