// WORSHIP BASS ESSENTIALS — processadores de áudio que o Web Audio não tem prontos:
// a oitava abaixo (divisor analógico) e o noise gate.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA

'use strict';

// Oitava abaixo no estilo dos pedais analógicos: um comparador com histerese
// acompanha o sinal, um divisor gera uma onda quadrada com metade da
// frequência e um envelope copia a dinâmica do que está sendo tocado.
class SubOitava extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'mix',   defaultValue: 0.5, minValue: 0, maxValue: 1, automationRate: 'k-rate' },
      { name: 'nivel', defaultValue: 1.0, minValue: 0, maxValue: 4, automationRate: 'k-rate' },
    ];
  }

  constructor() {
    super();
    const sr = sampleRate;
    this.lpTrack = 0;                     // filtro de rastreio (~250 Hz)
    this.aTrack = Math.exp(-2 * Math.PI * 250 / sr);
    this.comp = 1;                        // estado do comparador
    this.flip = 1;                        // estado do divisor
    this.env = 0;
    this.aAtk = Math.exp(-1 / (0.004 * sr));
    this.aRel = Math.exp(-1 / (0.090 * sr));
    this.lp1 = 0; this.lp2 = 0;
    this.aOut = Math.exp(-2 * Math.PI * 220 / sr);
    this.dc = 0;
  }

  process(inputs, outputs, params) {
    const inp = inputs[0], out = outputs[0];
    if (!out || out.length === 0) return true;
    const x = (inp && inp[0]) ? inp[0] : null;
    const mix = params.mix[0], nivel = params.nivel[0];
    const n = out[0].length;

    for (let i = 0; i < n; i++) {
      const s = x ? x[i] : 0;

      // rastreio: só as fundamentais interessam para contar os ciclos
      this.lpTrack = this.aTrack * this.lpTrack + (1 - this.aTrack) * s;
      const t = this.lpTrack;

      // envelope do sinal original
      const mag = Math.abs(s);
      const a = mag > this.env ? this.aAtk : this.aRel;
      this.env = a * this.env + (1 - a) * mag;

      // comparador com histerese proporcional ao envelope (evita tremer no silêncio)
      const h = Math.max(0.004, this.env * 0.12);
      if (this.comp <= 0 && t > h) { this.comp = 1; this.flip = -this.flip; }
      else if (this.comp > 0 && t < -h) { this.comp = -1; }

      // quadrada na metade da frequência, com a dinâmica do baixo
      let sub = this.flip * this.env * 1.6;

      // abafa: o que interessa do sub é o fundo, não a quadrada crua
      this.lp1 = this.aOut * this.lp1 + (1 - this.aOut) * sub;
      this.lp2 = this.aOut * this.lp2 + (1 - this.aOut) * this.lp1;
      sub = this.lp2;

      // tira qualquer componente contínua que o divisor possa deixar
      this.dc = 0.9995 * this.dc + 0.0005 * sub;
      sub -= this.dc;

      // fecha quando não há nota (−60 dBFS)
      if (this.env < 0.001) sub = 0;

      const y = s * (1 - mix) + Math.tanh(sub * nivel) * mix;
      for (let c = 0; c < out.length; c++) out[c][i] = y;
    }
    return true;
  }
}

// Noise gate simples, com ataque rápido e release ajustável.
class Gate extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [
      { name: 'thresh',  defaultValue: -65, minValue: -100, maxValue: 0,   automationRate: 'k-rate' },
      { name: 'release', defaultValue: 27,  minValue: 2,    maxValue: 1000, automationRate: 'k-rate' },
    ];
  }

  constructor() {
    super();
    this.env = 0;
    this.g = 1;
    this.aAtk = Math.exp(-1 / (0.001 * sampleRate));
    this.aDet = Math.exp(-1 / (0.005 * sampleRate));
  }

  process(inputs, outputs, params) {
    const inp = inputs[0], out = outputs[0];
    if (!out || out.length === 0) return true;
    const x = (inp && inp[0]) ? inp[0] : null;
    const lim = Math.pow(10, params.thresh[0] / 20);
    const aRel = Math.exp(-1 / (Math.max(2, params.release[0]) / 1000 * sampleRate));
    const n = out[0].length;

    for (let i = 0; i < n; i++) {
      const s = x ? x[i] : 0;
      const mag = Math.abs(s);
      this.env = mag > this.env ? this.aDet * this.env + (1 - this.aDet) * mag
                                : this.aDet * this.env;
      const alvo = this.env > lim ? 1 : 0;
      const a = alvo > this.g ? this.aAtk : aRel;
      this.g = a * this.g + (1 - a) * alvo;
      const y = s * this.g;
      for (let c = 0; c < out.length; c++) out[c][i] = y;
    }
    return true;
  }
}

registerProcessor('sub-oitava', SubOitava);
registerProcessor('noise-gate', Gate);
