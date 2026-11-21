// VERGANI PEDALBOARD — montagem da cadeia de áudio.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
//
// Cada pedal é um bloco com entrada, saída e bypass, para trocar de preset
// sem reconstruir o grafo (e sem estalo). Ordem fixa:
//   overdrive → distorção → detune → chorus → delay

'use strict';

const RAMPA = 0.02;   // segundos — tempo das trocas, para não estalar

function db2lin(db) { return Math.pow(10, db / 20); }

// Pico das curvas de saturação: −6 dBFS, para o pedal ligado não bater no
// limitador final. O knob LEVEL sobe a partir daí.
const PICO_CURVA = 0.5;

/** Overdrive: saturação macia e levemente assimétrica (ganho 1x a 25x). */
function curvaOverdrive(drive, n = 2048) {
  const g = 1 + (Math.max(0, Math.min(100, drive)) / 100) * 24;
  const c = new Float32Array(n);
  let pico = 0;
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    // o lado negativo satura um pouco mais cedo: harmônico par, sem deslocar o centro
    c[i] = x >= 0 ? Math.tanh(g * x) : Math.tanh(g * 0.8 * x);
    pico = Math.max(pico, Math.abs(c[i]));
  }
  for (let i = 0; i < n; i++) c[i] = (c[i] / pico) * PICO_CURVA;
  return c;
}

/** Distorção: ganho alto (8x a 200x) e corte duro, mais forte no lado positivo. */
function curvaDistorcao(drive, n = 2048) {
  const g = 8 + (Math.max(0, Math.min(100, drive)) / 100) * 192;
  const c = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    const y = x >= 0 ? Math.tanh(g * x) : Math.tanh(g * 0.72 * x);
    c[i] = y * PICO_CURVA;
  }
  return c;
}

// ---------------------------------------------------------------------------

function blocoBase(ctx) {
  return { ctx, entrada: ctx.createGain(), saida: ctx.createGain(), ligado: false };
}

const desconecta = (de, para) => { try { de.disconnect(para); } catch (e) { /* já estava solto */ } };

// BYPASS VERDADEIRO: com o pedal desligado, o sinal vai da entrada direto para
// a saída, por um único nó de ganho 1, e o pedal fica totalmente desconectado
// do caminho (não processa nada, nem em paralelo). Ligado, o contrário.
function finaliza(b, cabeca, cauda) {
  b.cabeca = cabeca; b.cauda = cauda;
  b.bypass = (ligado) => {
    if (ligado) {
      desconecta(b.entrada, b.saida);
      b.entrada.connect(cabeca);
      cauda.connect(b.saida);
    } else {
      desconecta(b.entrada, cabeca);
      desconecta(cauda, b.saida);
      b.entrada.connect(b.saida);
    }
    b.ligado = !!ligado;
  };
  b.bypass(false);
  return b;
}

// ---- Pedais ---------------------------------------------------------------

function fazOverdrive(ctx) {
  const b = blocoBase(ctx);
  // Só os agudos entram forte no clip; o grave passa quase limpo, como num
  // pedal de médio: dá ataque sem embolar a corda grave.
  const hpf = ctx.createBiquadFilter(); hpf.type = 'highpass'; hpf.frequency.value = 280; hpf.Q.value = 0.6;
  const shaper = ctx.createWaveShaper(); shaper.oversample = '4x';
  shaper.curve = curvaOverdrive(35);
  const graves = ctx.createBiquadFilter(); graves.type = 'lowpass'; graves.frequency.value = 280; graves.Q.value = 0.6;
  const gGraves = ctx.createGain(); gGraves.gain.value = 0.5;
  const soma = ctx.createGain();
  const tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 3500; tone.Q.value = 0.6;
  const dc = ctx.createBiquadFilter(); dc.type = 'highpass'; dc.frequency.value = 30; dc.Q.value = 0.7;
  const nivel = ctx.createGain();
  hpf.connect(shaper); shaper.connect(soma);
  graves.connect(gGraves); gGraves.connect(soma);
  soma.connect(dc); dc.connect(tone); tone.connect(nivel);
  const cabeca = ctx.createGain();
  cabeca.connect(hpf); cabeca.connect(graves);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'drive') shaper.curve = curvaOverdrive(v);
    if (id === 'tone') tone.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'nivel') nivel.gain.setTargetAtTime(db2lin(v), t, RAMPA);
  };
  return finaliza(b, cabeca, nivel);
}

function fazDistorcao(ctx) {
  const b = blocoBase(ctx);
  const hpf = ctx.createBiquadFilter(); hpf.type = 'highpass'; hpf.frequency.value = 120; hpf.Q.value = 0.7;
  const shaper = ctx.createWaveShaper(); shaper.oversample = '4x';
  shaper.curve = curvaDistorcao(60);
  const dc = ctx.createBiquadFilter(); dc.type = 'highpass'; dc.frequency.value = 40; dc.Q.value = 0.7;
  const tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 3000; tone.Q.value = 0.7;
  const nivel = ctx.createGain();
  hpf.connect(shaper); shaper.connect(dc); dc.connect(tone); tone.connect(nivel);
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'drive') shaper.curve = curvaDistorcao(v);
    if (id === 'tone') tone.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'nivel') nivel.gain.setTargetAtTime(db2lin(v), t, RAMPA);
  };
  return finaliza(b, hpf, nivel);
}

function fazDetune(ctx) {
  const b = blocoBase(ctx);
  const n = new AudioWorkletNode(ctx, 'detune', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'cents') n.parameters.get('cents').setValueAtTime(v, t);
    if (id === 'mix') n.parameters.get('mix').setTargetAtTime(v / 100, t, RAMPA);
  };
  return finaliza(b, n, n);
}

function fazChorus(ctx) {
  const b = blocoBase(ctx);
  const entrada = ctx.createGain();
  // Duas vozes com a modulação em oposição: o som abre sem virar vibrato.
  const a1 = ctx.createDelay(0.05), a2 = ctx.createDelay(0.05);
  a1.delayTime.value = 0.014; a2.delayTime.value = 0.019;
  const lfo = ctx.createOscillator(); lfo.type = 'sine'; lfo.frequency.value = 0.8;
  const p1 = ctx.createGain(), p2 = ctx.createGain();
  p1.gain.value = 0.0025; p2.gain.value = -0.0025;
  lfo.connect(p1); lfo.connect(p2);
  p1.connect(a1.delayTime); p2.connect(a2.delayTime); lfo.start();
  const suaviza = ctx.createBiquadFilter(); suaviza.type = 'lowpass'; suaviza.frequency.value = 7000;
  const molhado = ctx.createGain(), seco = ctx.createGain(), soma = ctx.createGain();
  entrada.connect(a1); entrada.connect(a2);
  a1.connect(suaviza); a2.connect(suaviza); suaviza.connect(molhado);
  entrada.connect(seco);
  molhado.connect(soma); seco.connect(soma);
  molhado.gain.value = 0.5; seco.gain.value = 1;
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'rate') lfo.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'depth') {
      const d = 0.0002 + (v / 100) * 0.0055;
      p1.gain.setTargetAtTime(d, t, RAMPA); p2.gain.setTargetAtTime(-d, t, RAMPA);
    }
    if (id === 'mix') molhado.gain.setTargetAtTime((v / 100) * 0.9, t, RAMPA);
  };
  return finaliza(b, entrada, soma);
}

function fazDelay(ctx) {
  const b = blocoBase(ctx);
  const entrada = ctx.createGain();
  const atraso = ctx.createDelay(2);
  atraso.delayTime.value = 0.38;
  // Cada repetição passa por um filtro, então fica mais escura que a anterior.
  const tone = ctx.createBiquadFilter(); tone.type = 'lowpass'; tone.frequency.value = 3500; tone.Q.value = 0.5;
  const fb = ctx.createGain(); fb.gain.value = 0.35;
  const molhado = ctx.createGain(), seco = ctx.createGain(), soma = ctx.createGain();
  entrada.connect(atraso); atraso.connect(tone); tone.connect(fb); fb.connect(atraso);
  tone.connect(molhado); entrada.connect(seco);
  molhado.connect(soma); seco.connect(soma);
  molhado.gain.value = 0.3; seco.gain.value = 1;
  // Valores pedidos pelo usuário; a cauda do delay é silenciada ao desligar,
  // para repetições antigas não vazarem quando o pedal for religado.
  let fbv = 0.35, mixv = 0.3, tDesliga = -10;
  b.set = (id, v) => {
    const t = ctx.currentTime;
    if (id === 'time') atraso.delayTime.setTargetAtTime(v / 1000, t, 0.06);
    if (id === 'feedback') { fbv = Math.min(0.92, v / 100); if (b.ligado) fb.gain.setTargetAtTime(fbv, t, RAMPA); }
    if (id === 'tone') tone.frequency.setTargetAtTime(v, t, RAMPA);
    if (id === 'mix') { mixv = v / 100; if (b.ligado) molhado.gain.setTargetAtTime(mixv, t, RAMPA); }
  };
  finaliza(b, entrada, soma);
  const bypassBase = b.bypass;
  b.bypass = (ligado) => {
    const t = ctx.currentTime;
    if (ligado && !b.ligado) {
      const quando = Math.max(t, tDesliga + 1.1);    // o delay mais longo tem 1 s
      fb.gain.cancelScheduledValues(t); molhado.gain.cancelScheduledValues(t);
      fb.gain.setValueAtTime(fbv, quando); molhado.gain.setValueAtTime(mixv, quando);
    } else if (!ligado && b.ligado) {
      tDesliga = t;
      fb.gain.cancelScheduledValues(t); molhado.gain.cancelScheduledValues(t);
      fb.gain.setValueAtTime(0, t); molhado.gain.setValueAtTime(0, t);
    }
    bypassBase(ligado);
  };
  return b;
}

/** Afinador: ligado, a saída fica muda (mute) e o analisador lê a entrada. */
function fazAfinador(ctx) {
  const b = blocoBase(ctx);
  const mudo = ctx.createGain(); mudo.gain.value = 0;
  const an = ctx.createAnalyser(); an.fftSize = 4096;
  b.entrada.connect(an);               // só escuta; não faz parte do caminho do som
  b.analisador = an;
  b.set = () => {};
  return finaliza(b, mudo, mudo);
}

// ---------------------------------------------------------------------------

const FABRICAS = {
  tuner: fazAfinador, overdrive: fazOverdrive, distortion: fazDistorcao, detune: fazDetune,
  chorus: fazChorus, delay: fazDelay,
};

/** Monta a cadeia inteira na ordem fixa e devolve os blocos por id. */
function montaCadeia(ctx, ordem) {
  const blocos = {};
  const entrada = ctx.createGain();
  let anterior = entrada;
  for (const id of ordem) {
    const b = FABRICAS[id](ctx);
    anterior.connect(b.entrada);
    anterior = b.saida;
    blocos[id] = b;
  }
  // Proteção de saída: limitador rápido e, depois dele, um teto que não deixa
  // nenhum pico passar de 0 dBFS, por mais drive e feedback que se coloque.
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

if (typeof module !== 'undefined') module.exports = { montaCadeia, curvaOverdrive, curvaDistorcao, db2lin, PICO_CURVA };
