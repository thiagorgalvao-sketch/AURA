// AURA Bass — testes da versão web
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// Teste do motor de áudio da versão web (Node): node Tests/web-dsp-test.js
// Verifica os mesmos comportamentos do teste do plugin, com os mesmos números de referência.
const { AuraDsp, LATENCY } = require('../web/aura-processor.js');
const { TunerDetector, noteName, noteOctave } = require('../web/tuner.js');

const SR = 48000, BLOCK = 128;
let failures = 0;
const check = (ok, what) => { console.log(`  [${ok ? ' OK ' : 'FALHA'}] ${what}`); if (!ok) failures++; };
const db = (x) => 20 * Math.log10(Math.max(1e-12, x));

function run(dsp, freq, amp, seconds = 1, hook = null) {
  const total = Math.round(SR * seconds), out = new Float32Array(total), input = new Float32Array(total);
  const inBlock = new Float32Array(BLOCK), outBlock = new Float32Array(BLOCK);
  for (let pos = 0; pos < total; pos += BLOCK) {
    if (hook) hook(pos, dsp);
    for (let i = 0; i < BLOCK; i++) inBlock[i] = amp * Math.sin((2 * Math.PI * freq * (pos + i)) / SR);
    dsp.process([inBlock], outBlock);
    const n = Math.min(BLOCK, total - pos);
    out.set(outBlock.subarray(0, n), pos);
    input.set(inBlock.subarray(0, n), pos);
  }
  return { out, input };
}
const rms = (v, from) => { let s = 0; for (let i = from; i < v.length; i++) s += v[i] * v[i]; return Math.sqrt(s / (v.length - from)); };
function harmonic(v, from, f) {
  let re = 0, im = 0;
  for (let i = from; i < v.length; i++) { const ph = (2 * Math.PI * f * i) / SR; re += v[i] * Math.cos(ph); im += v[i] * Math.sin(ph); }
  return (2 * Math.hypot(re, im)) / (v.length - from);
}
const maxJump = (v, a, b) => { let m = 0; for (let i = Math.max(a, 1); i < Math.min(b, v.length); i++) m = Math.max(m, Math.abs(v[i] - v[i - 1])); return m; };
const from = SR / 2;
const flat = (extra = {}) => { const d = new AuraDsp(SR); d.setParams({ presence: 0, ...extra }); d.toneDb = [0, 0, 0, 0]; d.updateTone(); return d; };

console.log('Drive (dB) x THD (senoide 100 Hz, -12 dBFS, EQ plano)  [plugin: 3,6 / 9,3 / 22,5 / 34,5 / 38,3 %]');
const thds = [];
for (const drive of [-15, -7.5, 0, 7.5, 15]) {
  const d = flat({ drive }); d.driveGain = Math.pow(10, (drive + 20) / 20); d.autoGain = d.autoTarget = 1; d.lastDrive = drive;
  const { out } = run(d, 100, 0.25);
  const h1 = harmonic(out, from, 100), hs = [200, 300, 400, 500].map((f) => harmonic(out, from, f));
  const thd = (Math.hypot(...hs) / h1) * 100;
  thds.push(thd);
  console.log(`  drive ${drive >= 0 ? '+' : ''}${drive.toFixed(1)} dB  THD ${thd.toFixed(1)}%`);
  check(out.every(Number.isFinite), 'sem NaN/Inf');
}
const ref = [3.6, 9.3, 22.5, 34.5, 38.3];   // valores medidos no plugin (Tests/DspTest.cpp)
check(thds.every((t, i) => Math.abs(t - ref[i]) < 0.6), 'distorção igual à do plugin (±0,6 ponto)');

console.log('Blend 0% = sinal limpo alinhado');
{
  const d = flat({ blend: 0, drive: 15 }); d.blendMix = 0;
  const { out, input } = run(d, 440, 0.25);
  // referência: entrada pelo mesmo HPF de 18 Hz
  const { Coefs } = require('../web/aura-processor.js');
  const c = Coefs.highPass(SR, 18, 0.707); let z1 = 0, z2 = 0;
  const refSig = Float32Array.from(input, (x) => { const y = c[0] * x + z1; z1 = c[1] * x - c[3] * y + z2; z2 = c[2] * x - c[4] * y; return y; });
  let err = 0;
  for (let i = from; i < out.length; i++) err = Math.max(err, Math.abs(out[i] - refSig[i - LATENCY]));
  console.log(`  latência ${LATENCY} amostras, erro ${err.toExponential(2)}`);
  check(err < 1e-4, 'Blend 0% reproduz a entrada sem desalinhamento');
}

console.log('Blend 50% sem cancelamento (limpo e saturado em fase)');
{
  const a = flat({ blend: 50, drive: -15 }), b = flat({ blend: 0 }); b.blendMix = 0;
  const lvA = rms(run(a, 80, 0.25).out, from), lvB = rms(run(b, 80, 0.25).out, from);
  console.log(`  80 Hz: 50% ${db(lvA).toFixed(1)} dB, limpo ${db(lvB).toFixed(1)} dB`);
  check(Math.abs(db(lvA / lvB)) < 1.5, 'sem perda de graves ao misturar');
}

console.log('Compensação de volume: DRIVE ligado x desligado');
for (const drive of [-15, 0, 15]) {
  const on = flat({ drive }), off = flat({ drive, driveOn: false }); off.blendMix = 0;
  const diff = db(rms(run(on, 100, 0.25).out, from) / rms(run(off, 100, 0.25).out, from));
  console.log(`  drive ${drive >= 0 ? '+' : ''}${drive.toFixed(1)} dB: diferença ${diff >= 0 ? '+' : ''}${diff.toFixed(1)} dB`);
  check(Math.abs(diff) < 1.5, 'volume parecido ligado/desligado');
}

console.log('EQ: Low +12 dB em 50 Hz');
{
  const a = flat({ driveOn: false }), b = flat({ driveOn: false, low: 12 }); a.blendMix = b.blendMix = 0;
  const g = db(rms(run(b, 50, 0.1).out, from) / rms(run(a, 50, 0.1).out, from));
  console.log(`  ${g.toFixed(1)} dB  [plugin: +10,1 dB]`);
  check(Math.abs(g - 10.1) < 0.3, 'mesmo ganho do plugin');
}

console.log('Trocas sem estalo (DRIVE e afinador)');
{
  const d = flat();
  const { out } = run(d, 100, 0.25, 2, (pos, dsp) => {
    if (pos >= 0.5 * SR) dsp.setParams({ driveOn: false });
    if (pos >= 0.9 * SR) dsp.setParams({ driveOn: true });
    if (pos >= 1.3 * SR) dsp.setParams({ tuner: true });
    if (pos >= 1.6 * SR) dsp.setParams({ tuner: false });
  });
  const steady = maxJump(out, 0.2 * SR, 0.45 * SR);
  const sw = Math.max(...[0.5, 0.9, 1.3, 1.6].map((t) => maxJump(out, t * SR - 50, t * SR + 4000)));
  console.log(`  maior salto: estável ${steady.toFixed(4)}, nas trocas ${sw.toFixed(4)}`);
  check(sw <= steady * 1.2, 'sem descontinuidade');
}

console.log('Afinador ligado: saída muda e detecção pela entrada');
for (const [name, hz, midi] of [['B0', 30.868, 23], ['E1', 41.203, 28], ['A1', 55, 33], ['G2', 97.999, 43]]) {
  const d = new AuraDsp(SR); d.setParams({ tuner: true });
  const det = new TunerDetector();
  d.onTunerData = (buf) => det.push(buf, d.tunerRate);
  const { out } = run(d, hz, 0.25, 1);
  const r = det.analyse();
  const err = r.valid ? 1200 * Math.log2(r.hz / hz) : 999;
  let peak = 0; for (let i = 4800; i < out.length; i++) peak = Math.max(peak, Math.abs(out[i]));
  console.log(`  ${name}: ${hz} Hz -> ${r.hz.toFixed(3)} Hz ${noteName(r.midiNote)}${noteOctave(r.midiNote)} erro ${err.toFixed(2)} cents, pico na saída ${peak.toExponential(1)}`);
  check(r.valid && r.midiNote === midi && Math.abs(err) < 1, 'nota certa, erro < 1 cent');
  check(peak < 1e-5, 'saída muda');
}

console.log('Layout da web igual ao do plugin (Source/Layout.h)');
{
  const fs = require('fs');
  const L = require('../web/layout.js');
  const h = fs.readFileSync(__dirname + '/../Source/Layout.h', 'utf8');
  const num = (name) => {
    const m = h.match(new RegExp('\\b' + name + '\\s*=\\s*(-?[\\d.]+)f?'));
    return m ? parseFloat(m[1]) : null;
  };
  const scalars = ['knobHitRadius', 'knobMinAngleDeg', 'knobMaxAngleDeg', 'pointerHalf', 'valueY', 'valueFontSize',
                   'arcCentreDy', 'arcLitRadius', 'arcMaxDeg', 'driveSwitchX', 'driveSwitchY', 'driveSwitchDownHalf',
                   'driveLedX', 'driveLedY', 'driveLedHalf', 'tunerSwitchX', 'tunerSwitchY', 'tunerLedX', 'tunerLedY',
                   'cabLedX', 'cabLedY', 'cabToggleX', 'cabToggleY', 'ledHalf', 'toggleHalf',
                   'meterMinDb', 'meterClipX', 'meterLabelY'];
  const bad = scalars.filter((k) => num(k) === null || Math.abs(num(k) - L[k]) > 1e-4);
  const size = h.match(/width\s*=\s*(\d+);[\s\S]*?height\s*=\s*(\d+);/);
  const sizeOk = size && +size[1] === L.width && +size[2] === L.height;
  const rects = ['driveHit', 'tunerHit', 'tunerDisplay', 'menuButton', 'cabHit', 'meter', 'meterHit'].filter((k) => {
    const m = h.match(new RegExp('Rect ' + k + '\\s*\\{\\s*(-?[\\d.]+)f,\\s*(-?[\\d.]+)f,\\s*(-?[\\d.]+)f,\\s*(-?[\\d.]+)f'));
    if (!m) return true;
    const r = L[k];
    return Math.abs(+m[1] - r.x) > 1e-4 || Math.abs(+m[2] - r.y) > 1e-4 || Math.abs(+m[3] - r.w) > 1e-4 || Math.abs(+m[4] - r.h) > 1e-4;
  });
  const knobs = [...h.matchAll(/\{\s*"(\w+)",\s*(-?[\d.]+)f,\s*(-?[\d.]+)f,\s*(-?[\d.]+)f\s*\}/g)];
  const knobsOk = knobs.length === L.knobs.length && knobs.every((m, i) => {
    const k = L.knobs[i];
    return m[1] === k.paramId && Math.abs(+m[2] - k.cx) < 1e-4 && Math.abs(+m[3] - k.cy) < 1e-4 && Math.abs(+m[4] - k.valueX) < 1e-4;
  });
  console.log(`  ${scalars.length} medidas, ${rects.length ? 'divergências: ' + rects.join(',') : '5 retângulos'}, ${knobs.length} knobs`);
  check(sizeOk, 'tamanho do painel');
  check(bad.length === 0, 'medidas iguais' + (bad.length ? ': ' + bad.join(',') : ''));
  check(rects.length === 0, 'áreas clicáveis iguais');
  check(knobsOk, 'posições dos knobs iguais');
}

console.log('Faixas e valores padrão iguais aos do plugin (PluginProcessor.cpp)');
{
  const fs = require('fs');
  const L = require('../web/layout.js');
  const cpp = fs.readFileSync(__dirname + '/../Source/PluginProcessor.cpp', 'utf8');
  const eq = cpp.match(/auto eq = \[\] \{ return NormalisableRange<float> \((-?[\d.]+)f, (-?[\d.]+)f, (-?[\d.]+)f\)/);
  const n = '(-?[\\d.]+)f';
  const faixa = (id) => {
    let m = cpp.match(new RegExp('ParamIDs::' + id + '[\\s\\S]{0,160}?NormalisableRange<float> \\(' + n + ', ' + n + ', ' + n + '\\), ' + n));
    if (m) return { min: +m[1], max: +m[2], step: +m[3], def: +m[4] };
    m = cpp.match(new RegExp('ParamIDs::' + id + '[\\s\\S]{0,120}?eq\\(\\), ' + n));
    if (m && eq) return { min: +eq[1], max: +eq[2], step: +eq[3], def: +m[1] };
    return null;
  };
  const difs = Object.keys(L.params).filter((id) => {
    const c = faixa(id), w = L.params[id];
    return !c || ['min', 'max', 'step', 'def'].some((k) => Math.abs(c[k] - w[k]) > 1e-4);
  });
  console.log(`  ${Object.keys(L.params).length} controles conferidos` + (difs.length ? `, divergências: ${difs.join(',')}` : ''));
  check(difs.length === 0, 'faixa, passo e valor padrão iguais aos do plugin');
}

console.log('Silêncio e canais');
{
  const d = new AuraDsp(SR), out = new Float32Array(BLOCK);
  d.process([], out);
  check(out.every((x) => x === 0), 'sem entrada conectada = silêncio');
  const l = new Float32Array(BLOCK).fill(0.1), r = new Float32Array(BLOCK).fill(-0.1);
  d.setParams({ channel: 2, driveOn: false }); d.blendMix = 0;
  for (let i = 0; i < 400; i++) d.process([l, r], out);
  check(Math.abs(out[BLOCK - 1]) < 1e-3, 'canal 1+2 soma os dois lados');
}

console.log(failures === 0 ? '\nTodos os testes passaram.' : `\n${failures} teste(s) falharam.`);
process.exit(failures === 0 ? 0 : 1);
