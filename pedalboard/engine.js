// WORSHIP BASS ESSENTIALS — montagem da cadeia de áudio.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
//
// Cada pedal é um bloco com entrada, saída e um atalho de bypass, para trocar
// de preset sem reconstruir o grafo (e sem estalo).

'use strict';

const RAMPA = 0.02;   // segundos — tempo das trocas, para não estalar

function db2lin(db) { return Math.pow(10, db / 20); }

function curvaTanh(driveDb, assimetria = 0.12, n = 2048) {
  const c = new Float32Array(n);
  const g = db2lin(driveDb);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    const b = x >= 0 ? assimetria : -assimetria * 0.55;
    c[i] = Math.tanh(g * (x + b)) - Math.tanh(g * b);
  }
  // normaliza para o trecho não explodir de volume
  let pico = 0;
  for (let i = 0; i < n; i++) pico = Math.max(pico, Math.abs(c[i]));
  if (pico > 0) for (let i = 0; i < n; i++) c[i] /= pico;
  return c;
}

function curvaClip(driveDb, simetria, n = 2048) {
  // Clip Distortion: ganho alto e corte duro, com o ponto de corte deslocado
  const c = new Float32Array(n);
  const g = db2lin(driveDb);
  const s = Math.max(0.05, Math.min(1, simetria / 100));
  const tetoP = s, tetoN = 2 - s;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    let y = g * x * 0.1;
    y = y > 0 ? Math.min(y, tetoP) : Math.max(y, -tetoN);
    c[i] = Math.tanh(y * 1.2) / Math.tanh(1.2);
  }
  return c;
}

function curvaFuzz(drive, n = 2048) {
  const c = new Float32Array(n);
  const g = 1 + (drive / 100) * 60;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    const y = Math.tanh(g * x);
    c[i] = Math.sign(y) * Math.pow(Math.abs(y), 0.78);   // comprime e deixa áspero
  }
  return c;
}

// ---------------------------------------------------------------------------

function blocoBase(ctx) {
  const entrada = ctx.createGain();
  const saida = ctx.createGain();
  const wet = ctx.createGain();
  const dry = ctx.createGain();
  wet.gain.value = 1; dry.gain.value = 0;
  entrada.connect(dry); dry.connect(saida); wet.connect(saida);
  return { ctx, entrada, saida, wet, dry, ligado: true };
}

function finaliza(b, cabeca, cauda) {
  b.entrada.connect(cabeca);
  cauda.connect(b.wet);
  b.bypass = (ligado) => {
    b.ligado = ligado;
    const t = b.ctx.currentTime;
    b.wet.gain.setTargetAtTime(ligado ? 1 : 0, t, RAMPA / 3);
    b.dry.gain.setTargetAtTime(ligado ? 0 : 1, t, RAMPA / 3);
  };
  return b;
}

// ---- Pedais ---------------------------------------------------------------

function fazConsoleEq(ctx) {
  const b = blocoBase(ctx);
  const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 50; hp.Q.value = 0.7;
  const low = ctx.createBiquadFilter(); low.type = 'lowshelf'; low.frequency.value = 100;
  const lmid = ctx.createBiquadFilter(); lmid.type = 'peaking'; lmid.frequency.value = 300; lmid.Q.value = 0.8;
  const hmid = ctx.createBiquadFilter(); hmid.type = 'peaking'; hmid.frequency.value = 1500; hmid.Q.value = 0.7;
  const high = ctx.createBiquadFilter(); high.type = 'highshelf'; high.frequency.value = 6000;
  hp.connect(low); low.connect(lmid); lmid.connect(hmid); hmid.connect(high);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'hp') hp.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'low') low.gain.setTargetAtTime(v, t, RAMPA);
    if (id === 'lowMid') lmid.gain.setTargetAtTime(v, t, RAMPA);
    if (id === 'highMid') hmid.gain.setTargetAtTime(v, t, RAMPA);
    if (id === 'high') high.gain.setTargetAtTime(v, t, RAMPA);
  };
  return finaliza(b, hp, high);
}

function fazComp(ctx) {
  const b = blocoBase(ctx);
  const c = ctx.createDynamicsCompressor();
  c.knee.value = 6;
  const makeup = ctx.createGain();
  c.connect(makeup);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'thresh') c.threshold.setTargetAtTime(v, t, RAMPA);
    if (id === 'ratio') c.ratio.setTargetAtTime(v, t, RAMPA);
    if (id === 'attack') c.attack.setTargetAtTime(v / 1000, t, RAMPA);
    if (id === 'release') c.release.setTargetAtTime(v / 1000, t, RAMPA);
    if (id === 'makeup') makeup.gain.setTargetAtTime(db2lin(v), t, RAMPA);
  };
  b.reducao = () => c.reduction;
  return finaliza(b, c, makeup);
}

// EQ de canal: um passa-altas e cinco bandas, como o Channel EQ do Logic
function fazEq(ctx) {
  const b = blocoBase(ctx);
  const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = 20; hp.Q.value = 0.71;
  const bandas = [];
  let no = hp;
  for (let i = 0; i < 5; i++) {
    const f = ctx.createBiquadFilter();
    f.type = i === 0 ? 'lowshelf' : 'peaking';
    f.frequency.value = 200; f.gain.value = 0; f.Q.value = 1;
    no.connect(f); no = f; bandas.push(f);
  }
  b.aplicaEq = (cfg) => {
    const t = ctx.currentTime;
    hp.frequency.setTargetAtTime(cfg.hp.on ? cfg.hp.hz : 10, t, RAMPA);
    hp.Q.setTargetAtTime(cfg.hp.q, t, RAMPA);
    cfg.bands.forEach((bd, i) => {
      const f = bandas[i];
      f.type = bd.tipo;
      f.frequency.setTargetAtTime(bd.hz, t, RAMPA);
      f.gain.setTargetAtTime(bd.on ? bd.db : 0, t, RAMPA);
      f.Q.setTargetAtTime(bd.q, t, RAMPA);
    });
  };
  b.set = (id, v) => {
    const t = ctx.currentTime;
    const alvo = { b165: 1, b1030: 3, b1920: 4, bLow: 1, bMid: 3 }[id];
    if (id === 'hp') { hp.frequency.setTargetAtTime(v, t, RAMPA); return; }
    if (alvo !== undefined) bandas[alvo].gain.setTargetAtTime(v, t, RAMPA);
  };
  b.leBanda = (id) => {
    const alvo = { b165: 1, b1030: 3, b1920: 4, bLow: 1, bMid: 3 }[id];
    return alvo === undefined ? hp.frequency.value : bandas[alvo].gain.value;
  };
  return finaliza(b, hp, no);
}

function fazAmp(ctx, irBuffer) {
  const b = blocoBase(ctx);
  const pre = ctx.createGain();
  const shaper = ctx.createWaveShaper();
  shaper.oversample = '4x';
  shaper.curve = curvaTanh(6);
  const bass = ctx.createBiquadFilter(); bass.type = 'lowshelf'; bass.frequency.value = 100;
  const mid = ctx.createBiquadFilter(); mid.type = 'peaking'; mid.frequency.value = 700; mid.Q.value = 0.8;
  const treb = ctx.createBiquadFilter(); treb.type = 'highshelf'; treb.frequency.value = 3000;
  const master = ctx.createGain();
  pre.connect(shaper); shaper.connect(bass); bass.connect(mid); mid.connect(treb);

  let cauda = treb;
  if (irBuffer) {
    const cab = ctx.createConvolver(); cab.normalize = true; cab.buffer = irBuffer;
    treb.connect(cab); cab.connect(master); cauda = master;
  } else {
    treb.connect(master); cauda = master;
  }

  const k = (v) => (v / 10) * 24 - 12;        // 0..10 -> -12..+12 dB
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'gain') { pre.gain.setTargetAtTime(db2lin(v * 2.2), t, RAMPA); }
    if (id === 'drive') { shaper.curve = curvaTanh(2 + v * 2.5); }
    if (id === 'bass') bass.gain.setTargetAtTime(k(v), t, RAMPA);
    if (id === 'mid') mid.gain.setTargetAtTime(k(v) * 0.6, t, RAMPA);
    if (id === 'treble') treb.gain.setTargetAtTime(k(v), t, RAMPA);
    if (id === 'master') master.gain.setTargetAtTime(db2lin((v - 6) * 1.5), t, RAMPA);
  };
  return finaliza(b, pre, cauda);
}

function fazDrive(ctx) {
  const b = blocoBase(ctx);
  const shaper = ctx.createWaveShaper(); shaper.oversample = '4x';
  const tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 1500; tone.Q.value = 0.6;
  const hpf = ctx.createBiquadFilter(); hpf.type = 'highpass'; hpf.frequency.value = 80; hpf.Q.value = 0.7;
  const nivel = ctx.createGain();
  shaper.connect(tone); tone.connect(hpf); hpf.connect(nivel);
  let dr = 37, sim = 46;
  shaper.curve = curvaClip(dr, sim);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'drive') { dr = v; shaper.curve = curvaClip(dr, sim); }
    if (id === 'sym') { sim = v; shaper.curve = curvaClip(dr, sim); }
    if (id === 'tone') tone.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'nivel') nivel.gain.setTargetAtTime(db2lin(v), t, RAMPA);
  };
  return finaliza(b, shaper, nivel);
}

function fazFuzz(ctx) {
  const b = blocoBase(ctx);
  const entrada = ctx.createGain();
  const shaper = ctx.createWaveShaper(); shaper.oversample = '4x';
  shaper.curve = curvaFuzz(59);
  const tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 2300; tone.Q.value = 0.5;
  const hpf = ctx.createBiquadFilter(); hpf.type = 'highpass'; hpf.frequency.value = 110; hpf.Q.value = 0.7;
  const molhado = ctx.createGain(), seco = ctx.createGain();
  const nivel = ctx.createGain();
  entrada.connect(shaper); shaper.connect(tone); tone.connect(hpf); hpf.connect(molhado);
  entrada.connect(seco);
  molhado.connect(nivel); seco.connect(nivel);
  molhado.gain.value = 1; seco.gain.value = 0;
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'drive') shaper.curve = curvaFuzz(v);
    if (id === 'tone') tone.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'mix') { molhado.gain.setTargetAtTime(v / 100, t, RAMPA); seco.gain.setTargetAtTime(1 - v / 100, t, RAMPA); }
    if (id === 'nivel') nivel.gain.setTargetAtTime(db2lin(v), t, RAMPA);
  };
  return finaliza(b, entrada, nivel);
}

function fazSub(ctx) {
  const b = blocoBase(ctx);
  const n = new AudioWorkletNode(ctx, 'sub-oitava', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
  // Tira o infrassom: abaixo disso não se ouve, só gasta excursão do alto-falante.
  const hpf = ctx.createBiquadFilter(); hpf.type = 'highpass'; hpf.frequency.value = 22; hpf.Q.value = 0.7;
  n.connect(hpf);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'mix') n.parameters.get('mix').setTargetAtTime(v / 100, t, RAMPA);
    if (id === 'nivel') n.parameters.get('nivel').setTargetAtTime(db2lin(v), t, RAMPA);
  };
  return finaliza(b, n, hpf);
}

function fazGate(ctx) {
  const b = blocoBase(ctx);
  const n = new AudioWorkletNode(ctx, 'noise-gate', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'thresh') n.parameters.get('thresh').setValueAtTime(v, t);
    if (id === 'release') n.parameters.get('release').setValueAtTime(v, t);
  };
  return finaliza(b, n, n);
}

function fazChorus(ctx) {
  const b = blocoBase(ctx);
  const entrada = ctx.createGain();
  const atraso = ctx.createDelay(0.05);
  atraso.delayTime.value = 0.012;
  const lfo = ctx.createOscillator(); lfo.type = 'sine'; lfo.frequency.value = 0.33;
  const prof = ctx.createGain(); prof.gain.value = 0.0035;
  lfo.connect(prof); prof.connect(atraso.delayTime); lfo.start();
  const molhado = ctx.createGain(), seco = ctx.createGain(), soma = ctx.createGain();
  entrada.connect(atraso); atraso.connect(molhado); entrada.connect(seco);
  molhado.connect(soma); seco.connect(soma);
  molhado.gain.value = 0.64; seco.gain.value = 1;
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'rate') lfo.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'depth') prof.gain.setTargetAtTime(0.0002 + (v / 100) * 0.006, t, RAMPA);
    if (id === 'mix') molhado.gain.setTargetAtTime(v / 100, t, RAMPA);
  };
  return finaliza(b, entrada, soma);
}

// ---------------------------------------------------------------------------

const FABRICAS = {
  consoleEq: fazConsoleEq, comp: fazComp, eq: fazEq, amp: fazAmp,
  drive: fazDrive, fuzz: fazFuzz, eq2: fazEq, gate: fazGate,
  sub: fazSub, chorus: fazChorus,
};

/** Monta a cadeia inteira na ordem fixa e devolve os blocos por id. */
function montaCadeia(ctx, ordem, irBuffer) {
  const blocos = {};
  let anterior = null;
  const entrada = ctx.createGain();
  anterior = entrada;
  for (const id of ordem) {
    const b = FABRICAS[id](ctx, irBuffer);
    anterior.connect(b.entrada);
    anterior = b.saida;
    blocos[id] = b;
  }
  // Proteção de saída: limitador rápido e, depois dele, um teto que não deixa
  // nenhum pico passar de 0 dBFS, por mais drive que se coloque.
  const limite = ctx.createDynamicsCompressor();
  limite.threshold.value = -6; limite.ratio.value = 20;
  limite.attack.value = 0.001; limite.release.value = 0.08; limite.knee.value = 0;
  const volume = ctx.createGain();
  const teto = ctx.createWaveShaper();
  teto.oversample = '2x';
  const n = 4096, c = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    c[i] = Math.tanh(x * 1.35) / Math.tanh(1.35) * 0.985;
  }
  teto.curve = c;
  anterior.connect(limite); limite.connect(volume); volume.connect(teto);
  return { entrada, saida: teto, blocos, volume };
}

if (typeof module !== 'undefined') module.exports = { montaCadeia, curvaTanh, curvaClip, curvaFuzz, db2lin };
