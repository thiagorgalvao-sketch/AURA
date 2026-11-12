// AURA Bass — app da versão web
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// AURA Bass — app da versão web.
// Desenha o mesmo painel do plugin num canvas (coordenadas de layout.js) e monta a cadeia
// de áudio: microfone/interface -> AudioWorklet (drive, EQ, blend) -> CAB (convolução) -> saída.
'use strict';

// Registro de autoria — aparece no menu "Sobre" e fica no pacote do app.
// Remover ou alterar este aviso é vedado pela licença (ver LICENSE).
const AUTORIA = [
  'AURA Bass — overdrive e pré de baixo',
  'Copyright (c) 2026 Thiago Galvão',
  'Todos os direitos reservados — Leis 9.609/1998 e 9.610/1998',
  'github.com/thiagorgalvao-sketch/AURA',
];
const AURA_PROV = 'AURA-PROV/1 obra=AURA-Bass autor=Thiago-Galvao ano=2026 '
  + 'lei=9609/1998+9610/1998 origem=github.com/thiagorgalvao-sketch/AURA direitos=reservados';
try { window.AURA_PROV = AURA_PROV; } catch (e) { /* sem window */ }

const L = window.AuraLayout;
const TunerApi = window.AuraTuner;

const COLORS = {
  value: '#b7bcc4',
  small: '#8e939a',
  green: '#3ddc84',
  amber: '#ffb347',
  dim: '#5b626c',
  text: '#c9ced6',
};

const KNOB_TIPS = {
  level: 'Volume de saída',
  drive: 'Ganho antes da saturação',
  low: 'Graves (80 Hz)',
  mid: 'Médios (600 Hz)',
  high: 'Agudos (2,5 kHz)',
  presence: 'Presença e ataque (5 kHz)',
  blend: 'Mistura do som limpo com o saturado',
};

const IMAGES = {
  background: 'assets/background.jpg',
  arcsLit: 'assets/arcs_lit.png',
  pointer: 'assets/pointer.png',
  ledDriveOn: 'assets/led_drive_on.png',
  ledTunerOn: 'assets/led_tuner_on.png',
  ledCabOn: 'assets/led_cab_on.png',
  cabOff: 'assets/cab_off.png',
  footswitchDown: 'assets/footswitch_down.png',
  tunerDown: 'assets/tuner_down.png',
};

const BUILTIN_IRS = [
  { name: '4x10', url: 'assets/cab_4x10.wav' },
  { name: '1x15', url: 'assets/cab_1x15.wav' },
];

const STORAGE_KEY = 'aura-bass-state-v1';

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
const state = {
  params: { level: 0, drive: 0, low: 0, mid: 0, high: 0, presence: 3, blend: 100 },
  driveOn: true,
  cab: true,
  tuner: false,
  irIndex: 0,
  irName: '4x10',
  customIr: null,          // { name, buffer }
  channel: 0,              // 0 = esquerda/mono, 1 = direita, 2 = soma
  inputDeviceId: '',
  pressed: null,           // 'drive' | 'tuner' (footswitch afundado)
  running: false,
};

const tuner = {
  detector: TunerApi ? new TunerApi.TunerDetector() : null,
  hasNote: false,
  note: 0,
  cents: 0,
  hz: 0,
  candidate: -1000,
  candidateCount: 0,
  warmup: 0,
  lastValid: 0,
};

let audio = null;          // { ctx, stream, source, worklet, convolver, wetGain, dryGain, master }
let images = {};
let needsPaint = true;

const canvas = document.getElementById('panel');
const ctx = canvas.getContext('2d');
const valueInput = document.getElementById('valueInput');
const menuEl = document.getElementById('menu');
const startOverlay = document.getElementById('start');
const startButton = document.getElementById('startButton');
const startNote = document.getElementById('startNote');
const toastEl = document.getElementById('toast');

const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const rad = (deg) => (deg * Math.PI) / 180;
const paint = () => { needsPaint = true; };

let toastTimer = 0;
function toast(message, ms = 3200) {
  toastEl.textContent = message;
  toastEl.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { toastEl.hidden = true; }, ms);
}

// ---------------------------------------------------------------------------
// Estado salvo no aparelho
// ---------------------------------------------------------------------------
function saveState() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify({
      params: state.params, driveOn: state.driveOn, cab: state.cab,
      irIndex: state.irIndex, channel: state.channel, inputDeviceId: state.inputDeviceId,
    }));
  } catch (e) { /* modo privado: segue sem salvar */ }
}

function loadState() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) || 'null');
    if (!saved) return;
    for (const id of Object.keys(state.params)) {
      const v = saved.params && saved.params[id];
      if (typeof v === 'number' && Number.isFinite(v)) {
        const p = L.params[id];
        state.params[id] = clamp(v, p.min, p.max);
      }
    }
    if (typeof saved.driveOn === 'boolean') state.driveOn = saved.driveOn;
    if (typeof saved.cab === 'boolean') state.cab = saved.cab;
    if (saved.irIndex === 0 || saved.irIndex === 1) { state.irIndex = saved.irIndex; state.irName = BUILTIN_IRS[saved.irIndex].name; }
    if ([0, 1, 2].includes(saved.channel)) state.channel = saved.channel;
    if (typeof saved.inputDeviceId === 'string') state.inputDeviceId = saved.inputDeviceId;
  } catch (e) { /* ignora estado inválido */ }
}

// ---------------------------------------------------------------------------
// Áudio
// ---------------------------------------------------------------------------
function sendParams() {
  if (!audio) return;
  audio.worklet.port.postMessage({
    params: { ...state.params, driveOn: state.driveOn, tuner: state.tuner, channel: state.channel },
  });
}

function applyCabMix() {
  if (!audio) return;
  const t = audio.ctx.currentTime, on = state.cab ? 1 : 0;
  audio.wetGain.gain.setTargetAtTime(on, t, 0.012);
  audio.dryGain.gain.setTargetAtTime(1 - on, t, 0.012);
}

// Ajusta a IR para 0 dB entre 100 e 800 Hz (ligar o CAB muda o timbre, não o volume)
function normaliseIr(buffer, ctxRate) {
  const src = buffer.getChannelData(0);
  const len = Math.min(src.length, Math.round(buffer.sampleRate * 1.0));
  let sumDb = 0;
  const points = 16;
  for (let k = 0; k < points; k++) {
    const f = 100 * Math.pow(8, k / (points - 1));
    const w = (2 * Math.PI * f) / buffer.sampleRate;
    let re = 0, im = 0;
    for (let n = 0; n < len; n++) { re += src[n] * Math.cos(w * n); im -= src[n] * Math.sin(w * n); }
    sumDb += 20 * Math.log10(Math.max(1e-10, Math.hypot(re, im)));
  }
  const gain = Math.pow(10, -(sumDb / points) / 20);
  const out = new AudioBuffer({ length: len, sampleRate: buffer.sampleRate, numberOfChannels: 1 });
  const dst = out.getChannelData(0);
  for (let n = 0; n < len; n++) dst[n] = src[n] * gain;
  return out;
}

async function loadIr(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error('IR não encontrada: ' + url);
  const decoded = await audio.ctx.decodeAudioData(await res.arrayBuffer());
  return normaliseIr(decoded);
}

async function setIr(index, custom) {
  if (!audio) return;
  try {
    const buffer = custom ? custom.buffer : await loadIr(BUILTIN_IRS[index].url);
    audio.convolver.buffer = buffer;
    state.customIr = custom || null;
    state.irIndex = custom ? -1 : index;
    state.irName = custom ? custom.name : BUILTIN_IRS[index].name;
    saveState();
    paint();
  } catch (err) {
    toast('Não consegui carregar essa resposta de impulso.');
  }
}

async function start() {
  startButton.disabled = true;
  startNote.textContent = 'Pedindo acesso ao áudio...';
  try {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia)
      throw new Error('Este navegador não dá acesso à entrada de áudio.');

    const constraints = {
      audio: {
        echoCancellation: false, noiseSuppression: false, autoGainControl: false,
        channelCount: { ideal: 2 },
        ...(state.inputDeviceId ? { deviceId: { exact: state.inputDeviceId } } : {}),
      },
      video: false,
    };
    const stream = await navigator.mediaDevices.getUserMedia(constraints);

    const ctxAudio = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
    await ctxAudio.resume();
    await ctxAudio.audioWorklet.addModule('aura-processor.js');

    const source = ctxAudio.createMediaStreamSource(stream);
    const worklet = new AudioWorkletNode(ctxAudio, 'aura-bass', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
    const convolver = ctxAudio.createConvolver();
    convolver.normalize = false;
    const wetGain = ctxAudio.createGain();
    const dryGain = ctxAudio.createGain();
    const master = ctxAudio.createGain();

    wetGain.gain.value = state.cab ? 1 : 0;
    dryGain.gain.value = state.cab ? 0 : 1;

    source.connect(worklet);
    worklet.connect(convolver).connect(wetGain).connect(master);
    worklet.connect(dryGain).connect(master);
    master.connect(ctxAudio.destination);

    worklet.port.onmessage = (e) => {
      if (e.data.tuner && tuner.detector) {
        tuner.detector.push(e.data.tuner, e.data.rate);
        updateTuner(tuner.detector.analyse());
      }
    };

    audio = { ctx: ctxAudio, stream, source, worklet, convolver, wetGain, dryGain, master };
    sendParams();
    applyCabMix();
    await setIr(state.irIndex >= 0 ? state.irIndex : 0);

    state.running = true;
    startOverlay.classList.add('hidden');
    paint();

    const latency = Math.round(((ctxAudio.baseLatency || 0) + (ctxAudio.outputLatency || 0)) * 1000);
    if (latency > 0) toast(`Pronto. Latência de saída: cerca de ${latency} ms.`, 4200);

    ctxAudio.onstatechange = () => { if (ctxAudio.state === 'suspended') ctxAudio.resume().catch(() => {}); };
    stream.getAudioTracks().forEach((t) => {
      t.onended = () => toast('A entrada de áudio foi desconectada. Recarregue a página.', 6000);
    });
  } catch (err) {
    startButton.disabled = false;
    const name = err && err.name;
    startNote.textContent =
      name === 'NotAllowedError' ? 'Acesso negado. Libere o microfone nas permissões do site e toque em Ligar de novo.'
      : name === 'NotFoundError' ? 'Nenhuma entrada de áudio encontrada. Ligue a interface ou o adaptador e tente de novo.'
      : (err && err.message) || 'Não consegui iniciar o áudio.';
  }
}

// ---------------------------------------------------------------------------
// Afinador (mesma lógica de TunerDisplay no plugin)
// ---------------------------------------------------------------------------
function restartTuner() {
  tuner.hasNote = false;
  tuner.candidate = -1000;
  tuner.candidateCount = 0;
  tuner.warmup = 6;
  if (tuner.detector) tuner.detector.reset();
}

function updateTuner(r) {
  const now = performance.now() / 1000;
  if (tuner.warmup > 0) { tuner.warmup--; return; }

  if (r.valid) {
    tuner.lastValid = now;
    if (tuner.hasNote && r.midiNote === tuner.note) {
      tuner.cents += 0.35 * (r.cents - tuner.cents);
      tuner.hz += 0.35 * (r.hz - tuner.hz);
      tuner.candidateCount = 0;
    } else {
      tuner.candidateCount = r.midiNote === tuner.candidate ? tuner.candidateCount + 1 : 1;
      tuner.candidate = r.midiNote;
      if (!tuner.hasNote || tuner.candidateCount >= 2) {
        tuner.hasNote = true;
        tuner.note = r.midiNote;
        tuner.cents = r.cents;
        tuner.hz = r.hz;
      }
    }
  } else if (now - tuner.lastValid > 0.6) {
    tuner.hasNote = false;
  }
  paint();
}

// ---------------------------------------------------------------------------
// Desenho
// ---------------------------------------------------------------------------
// Em telas estreitas (celular em pé) o painel gira 90°, para ficar grande como um pedal
let view = { scale: 1, rotated: false };

function fitCanvas() {
  const pad = 16;
  const availW = window.innerWidth - pad;
  const availH = window.innerHeight - pad;
  const direct = Math.min(availW / L.width, availH / L.height);
  const turned = Math.min(availH / L.width, availW / L.height);
  const rotated = turned > direct * 1.25;
  const scale = Math.min(rotated ? turned : direct, 2.2);

  const dpr = Math.min(window.devicePixelRatio || 1, 3);
  canvas.width = Math.round(L.width * scale * dpr);
  canvas.height = Math.round(L.height * scale * dpr);
  canvas.style.width = Math.round(L.width * scale) + 'px';
  canvas.style.height = Math.round(L.height * scale) + 'px';
  canvas.style.transform = 'translate(-50%, -50%)' + (rotated ? ' rotate(90deg)' : '');
  canvas.dataset.scale = String(scale);
  canvas.dataset.rotated = String(rotated);
  view = { scale, rotated };
  ctx.setTransform(scale * dpr, 0, 0, scale * dpr, 0, 0);
  ctx.imageSmoothingQuality = 'high';
  hideValueInput();
  paint();
}

// Conversões entre a tela e as coordenadas do painel (considerando a rotação)
function canvasCentre() {
  const r = canvas.getBoundingClientRect();
  return { x: r.left + r.width / 2, y: r.top + r.height / 2 };
}

function panelToClient(x, y) {
  const c = canvasCentre();
  const ex = (x - L.width / 2) * view.scale;
  const ey = (y - L.height / 2) * view.scale;
  return view.rotated ? { x: c.x - ey, y: c.y + ex } : { x: c.x + ex, y: c.y + ey };
}

const valueFont = (size) => `${size}px AuraValue, system-ui, sans-serif`;
const titleFont = (size) => `${size}px AuraTitle, system-ui, sans-serif`;

function drawCentred(img, cx, cy, half) {
  if (img) ctx.drawImage(img, cx - half, cy - half, half * 2, half * 2);
}

function normOf(id) {
  const p = L.params[id];
  return (state.params[id] - p.min) / (p.max - p.min);
}

function drawPanel() {
  ctx.clearRect(0, 0, L.width, L.height);
  if (images.background) ctx.drawImage(images.background, 0, 0, L.width, L.height);

  // Arcos acesos até o valor (a partir do centro nos controles em dB, do início no Blend)
  if (!state.tuner && images.arcsLit) {
    for (const k of L.knobs) {
      if (!state.driveOn && (k.paramId === 'drive' || k.paramId === 'blend')) continue;
      const norm = normOf(k.paramId);
      const end = rad(-L.arcMaxDeg + norm * 2 * L.arcMaxDeg);
      const begin = k.paramId === 'blend' ? rad(-L.arcMaxDeg) : 0;
      if (Math.abs(end - begin) < 0.01) continue;
      const cy = k.cy + L.arcCentreDy;
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(k.cx, cy);
      ctx.arc(k.cx, cy, L.arcLitRadius, Math.min(begin, end) - Math.PI / 2, Math.max(begin, end) - Math.PI / 2);
      ctx.closePath();
      ctx.clip();
      ctx.drawImage(images.arcsLit, 0, 0, L.width, L.height);
      ctx.restore();
    }
  }

  // Indicadores dos knobs
  if (!state.tuner && images.pointer) {
    for (const k of L.knobs) {
      const angle = rad(L.knobMinAngleDeg + normOf(k.paramId) * (L.knobMaxAngleDeg - L.knobMinAngleDeg));
      ctx.save();
      ctx.translate(k.cx, k.cy);
      ctx.rotate(angle);
      ctx.drawImage(images.pointer, -L.pointerHalf, -L.pointerHalf, L.pointerHalf * 2, L.pointerHalf * 2);
      ctx.restore();
    }
  }

  // Footswitches e LEDs
  if (state.pressed === 'drive') drawCentred(images.footswitchDown, L.driveSwitchX, L.driveSwitchY, L.driveSwitchDownHalf);
  if (state.pressed === 'tuner') drawCentred(images.tunerDown, L.tunerSwitchX, L.tunerSwitchY, L.driveSwitchDownHalf);
  if (state.driveOn) drawCentred(images.ledDriveOn, L.driveLedX, L.driveLedY, L.driveLedHalf);
  if (state.tuner) drawCentred(images.ledTunerOn, L.tunerLedX, L.tunerLedY, L.driveLedHalf);

  if (state.cab) drawCentred(images.ledCabOn, L.cabLedX, L.cabLedY, L.ledHalf);
  else drawCentred(images.cabOff, L.cabToggleX, L.cabToggleY, L.toggleHalf);

  // Ícone de menu (três traços)
  const m = L.menuButton;
  ctx.fillStyle = COLORS.small;
  for (let i = 0; i < 3; i++) roundRect(m.x + (m.w - 15) / 2, m.y + (m.h - 11) / 2 + i * 4.6, 15, 1.8, 0.9, true);

  if (state.tuner) { drawTuner(); return; }

  // Knobs apagados quando o ENGAGE está desligado
  if (!state.driveOn) {
    for (const id of ['drive', 'blend']) {
      const k = L.knobs.find((x) => x.paramId === id);
      const r = 43;
      const grad = ctx.createRadialGradient(k.cx, k.cy + 2, 0, k.cx, k.cy + 2, r);
      grad.addColorStop(0, 'rgba(0,0,0,.42)');
      grad.addColorStop(0.8, 'rgba(0,0,0,.42)');
      grad.addColorStop(1, 'rgba(0,0,0,0)');
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.arc(k.cx, k.cy + 2, r, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  // Valores
  ctx.font = valueFont(L.valueFontSize);
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  for (const k of L.knobs) {
    const dimmed = !state.driveOn && (k.paramId === 'drive' || k.paramId === 'blend');
    ctx.fillStyle = dimmed ? 'rgba(142,147,154,.55)' : COLORS.value;
    ctx.fillText(L.formatValue(k.paramId, state.params[k.paramId]), k.valueX, L.valueY);
  }

  // Nome da IR do CAB, com a seta colada
  const name = state.irName.length > 7 ? state.irName.slice(0, 6) + '…' : state.irName;
  ctx.font = valueFont(L.valueFontSize);
  const tw = ctx.measureText(name).width;
  const total = tw + 4 + 7;
  const x0 = L.cabToggleX - total / 2;
  ctx.fillStyle = state.cab ? COLORS.value : COLORS.small;
  ctx.textAlign = 'left';
  ctx.fillText(name, x0, L.valueY);
  ctx.beginPath();
  const tx = x0 + tw + 7.5, ty = L.valueY + 0.5;
  ctx.moveTo(tx - 3.5, ty - 2);
  ctx.lineTo(tx + 3.5, ty - 2);
  ctx.lineTo(tx, ty + 2.5);
  ctx.fill();
}

function roundRect(x, y, w, h, r, fill) {
  ctx.beginPath();
  if (ctx.roundRect) ctx.roundRect(x, y, w, h, r);
  else {
    ctx.moveTo(x + r, y);
    ctx.arcTo(x + w, y, x + w, y + h, r);
    ctx.arcTo(x + w, y + h, x, y + h, r);
    ctx.arcTo(x, y + h, x, y, r);
    ctx.arcTo(x, y, x + w, y, r);
    ctx.closePath();
  }
  if (fill) ctx.fill(); else ctx.stroke();
}

function drawTuner() {
  const b = L.tunerDisplay;
  const inTune = tuner.hasNote && Math.abs(tuner.cents) <= 3;
  const accent = inTune ? COLORS.green : COLORS.amber;

  ctx.fillStyle = '#050608';
  roundRect(b.x, b.y, b.w, b.h, 9, true);
  const grad = ctx.createLinearGradient(0, b.y, 0, b.y + b.h);
  grad.addColorStop(0, '#11151a');
  grad.addColorStop(1, '#090b0e');
  ctx.fillStyle = grad;
  roundRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, 7.5, true);
  ctx.strokeStyle = 'rgba(255,255,255,.10)';
  ctx.lineWidth = 1;
  roundRect(b.x + 0.5, b.y + 0.5, b.w - 1, b.h - 1, 9, false);

  const smallSize = 10.5;
  ctx.textBaseline = 'middle';

  // Cabeçalho
  ctx.font = valueFont(smallSize);
  ctx.textAlign = 'left';
  ctx.fillStyle = COLORS.dim;
  ctx.fillText('T U N E R', b.x + 14, b.y + 12);
  const pillW = 44, pillX = b.x + b.w - 14 - pillW, pillY = b.y + 12;
  ctx.fillStyle = 'rgba(255,179,71,.16)';
  roundRect(pillX, pillY - 7.5, pillW, 15, 7.5, true);
  ctx.fillStyle = COLORS.amber;
  ctx.textAlign = 'center';
  ctx.fillText('MUTE', pillX + pillW / 2, pillY);
  ctx.fillStyle = COLORS.dim;
  ctx.textAlign = 'right';
  ctx.fillText('A4 = 440 Hz', pillX - 10, pillY);

  // Nota
  const cx = b.x + b.w / 2, noteY = b.y + 58;
  ctx.textAlign = 'center';
  if (tuner.hasNote) {
    const name = TunerApi.noteName(tuner.note);
    ctx.font = titleFont(64);
    const w = ctx.measureText(name).width;
    ctx.fillStyle = accent;
    ctx.fillText(name, cx, noteY);
    ctx.font = titleFont(20);
    ctx.globalAlpha = 0.75;
    ctx.textAlign = 'left';
    ctx.fillText(String(TunerApi.noteOctave(tuner.note)), cx + w / 2 + 4, noteY + 14);
    ctx.globalAlpha = 1;

    const arrow = (right, lit) => {
      const ax = cx + (right ? 1 : -1) * 104, ay = noteY + 2, s = right ? 1 : -1;
      ctx.fillStyle = lit ? COLORS.amber : 'rgba(91,98,108,.35)';
      ctx.beginPath();
      ctx.moveTo(ax + 9 * s, ay - 11);
      ctx.lineTo(ax + 9 * s, ay + 11);
      ctx.lineTo(ax - 9 * s, ay);
      ctx.fill();
    };
    arrow(false, !inTune && tuner.cents < 0);
    arrow(true, !inTune && tuner.cents > 0);
  } else {
    ctx.font = titleFont(48);
    ctx.fillStyle = 'rgba(91,98,108,.6)';
    ctx.fillText('- -', cx, noteY - 8);
    ctx.font = valueFont(smallSize);
    ctx.fillStyle = COLORS.dim;
    ctx.fillText('T O Q U E   U M A   C O R D A', cx, noteY + 30);
  }

  // Medidor de cents
  const segments = 41, segW = 9, gap = 4;
  const total = segments * segW + (segments - 1) * gap;
  const x0 = cx - total / 2, my = b.y + 108;
  const pos = tuner.hasNote ? clamp(Math.round(tuner.cents / 2.5) + 20, 0, segments - 1) : -1;
  for (let i = 0; i < segments; i++) {
    const centre = i === 20, major = i % 4 === 0;
    const h = centre ? 24 : major ? 16 : 12;
    let lit = false;
    if (tuner.hasNote) lit = inTune ? Math.abs(i - 20) <= 1 : i >= Math.min(20, pos) && i <= Math.max(20, pos);
    ctx.fillStyle = lit ? accent : centre ? COLORS.dim : 'rgba(91,98,108,.28)';
    roundRect(x0 + i * (segW + gap), my - h / 2, segW, h, 2, true);
  }

  // Rodapé
  const fy = b.y + 136;
  ctx.font = valueFont(smallSize);
  ctx.fillStyle = COLORS.dim;
  ctx.textAlign = 'left';
  ctx.fillText('-50', x0, fy);
  ctx.textAlign = 'right';
  ctx.fillText('+50', x0 + total, fy);
  if (tuner.hasNote) {
    const c = Math.round(tuner.cents);
    ctx.font = valueFont(13);
    ctx.fillStyle = COLORS.text;
    ctx.textAlign = 'center';
    ctx.fillText(`${c > 0 ? '+' : ''}${c} cents     ${tuner.hz.toFixed(1)} Hz`, cx, fy);
  }
}

function frame() {
  if (needsPaint) { needsPaint = false; drawPanel(); }
  requestAnimationFrame(frame);
}

// ---------------------------------------------------------------------------
// Interação
// ---------------------------------------------------------------------------
function toPanel(ev) {
  const c = canvasCentre();
  const sx = ev.clientX - c.x, sy = ev.clientY - c.y;
  const ex = view.rotated ? sy : sx;
  const ey = view.rotated ? -sx : sy;
  return { x: ex / view.scale + L.width / 2, y: ey / view.scale + L.height / 2 };
}

const inRect = (p, r) => p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;

function knobAt(p) {
  if (state.tuner) return null;
  for (const k of L.knobs) {
    if (Math.hypot(p.x - k.cx, p.y - k.cy) <= L.knobHitRadius) return k;
  }
  return null;
}

function valueAt(p) {
  if (state.tuner) return null;
  if (Math.abs(p.y - L.valueY) > 11) return null;
  for (const k of L.knobs) if (Math.abs(p.x - k.valueX) <= 38) return k;
  return null;
}

function cabNameAt(p) {
  return !state.tuner && Math.abs(p.y - L.valueY) <= 11 && Math.abs(p.x - L.cabToggleX) <= 32;
}

function setParam(id, value, store = true) {
  const p = L.params[id];
  const snapped = Math.round(clamp(value, p.min, p.max) / p.step) * p.step;
  state.params[id] = Math.round(snapped * 1000) / 1000;
  sendParams();
  paint();
  if (store) saveState();
}

function toggle(what) {
  if (what === 'drive') state.driveOn = !state.driveOn;
  else if (what === 'cab') { state.cab = !state.cab; applyCabMix(); }
  else if (what === 'tuner') {
    state.tuner = !state.tuner;
    if (state.tuner) restartTuner();
    hideValueInput();
  }
  sendParams();
  saveState();
  paint();
}

let drag = null;
let lastTap = { time: 0, id: null };

canvas.addEventListener('pointerdown', (ev) => {
  if (!menuEl.hidden) { closeMenu(); return; }
  hideValueInput();
  const p = toPanel(ev);

  if (inRect(p, L.menuButton)) { openMenu(ev); return; }

  const knob = knobAt(p);
  if (knob) {
    const now = performance.now();
    if (lastTap.id === knob.paramId && now - lastTap.time < 400) {
      setParam(knob.paramId, L.params[knob.paramId].def);
      lastTap = { time: 0, id: null };
      return;
    }
    lastTap = { time: now, id: knob.paramId };
    canvas.setPointerCapture(ev.pointerId);
    drag = { id: knob.paramId, startX: ev.clientX, startY: ev.clientY, startValue: state.params[knob.paramId], fine: ev.shiftKey };
    ev.preventDefault();
    return;
  }

  const value = valueAt(p);
  if (value) { showValueInput(value); return; }
  if (cabNameAt(p)) { openMenu(ev, 'cab'); return; }

  if (inRect(p, L.driveHit)) { state.pressed = 'drive'; paint(); return; }
  if (inRect(p, L.tunerHit)) { state.pressed = 'tuner'; paint(); return; }
  if (inRect(p, L.cabHit)) { toggle('cab'); return; }
});

canvas.addEventListener('pointermove', (ev) => {
  if (!drag) return;
  const p = L.params[drag.id];
  const sensitivity = (drag.fine || ev.shiftKey) ? 1100 : 220;
  // Como no plugin: arraste para cima ou para a direita aumenta o valor
  const dx = ev.clientX - drag.startX, dy = ev.clientY - drag.startY;
  const delta = view.rotated ? (dy + dx) : (dx - dy);
  setParam(drag.id, drag.startValue + (delta / sensitivity) * (p.max - p.min), false);
});

function endDrag() {
  if (drag) { saveState(); drag = null; }
  if (state.pressed) {
    const which = state.pressed;
    state.pressed = null;
    toggle(which);
  }
}
canvas.addEventListener('pointerup', endDrag);
canvas.addEventListener('pointercancel', () => { drag = null; state.pressed = null; paint(); });

canvas.addEventListener('wheel', (ev) => {
  const knob = knobAt(toPanel(ev));
  if (!knob) return;
  ev.preventDefault();
  const p = L.params[knob.paramId];
  const step = ev.shiftKey ? p.step : (p.max - p.min) / 50;
  setParam(knob.paramId, state.params[knob.paramId] + (ev.deltaY < 0 ? step : -step));
}, { passive: false });

canvas.addEventListener('contextmenu', (ev) => { ev.preventDefault(); openMenu(ev); });

// Teclado: acessibilidade mínima
window.addEventListener('keydown', (ev) => {
  if (ev.key === 'Escape') { closeMenu(); hideValueInput(); }
  if (document.activeElement === valueInput) return;
  if (ev.key === 't' || ev.key === 'T') toggle('tuner');
  if (ev.key === 'e' || ev.key === 'E') toggle('drive');
  if (ev.key === 'c' || ev.key === 'C') toggle('cab');
});

// ---- Digitar um valor ------------------------------------------------------
let editing = null;

function showValueInput(knob) {
  const scale = view.scale;
  const pos = panelToClient(knob.valueX, L.valueY);
  const w = Math.max(78 * scale, 64), h = Math.max(20 * scale, 28);
  editing = knob.paramId;
  valueInput.hidden = false;
  valueInput.style.left = `${pos.x - w / 2}px`;
  valueInput.style.top = `${pos.y - h / 2}px`;
  valueInput.style.width = `${w}px`;
  valueInput.style.height = `${h}px`;
  valueInput.style.transform = view.rotated ? 'rotate(90deg)' : '';
  valueInput.style.fontSize = `${Math.max(L.valueFontSize * scale, 15)}px`;
  valueInput.value = String(state.params[knob.paramId]);
  valueInput.select();
  valueInput.focus();
}

function hideValueInput() {
  if (!editing) return;
  editing = null;
  valueInput.hidden = true;
  valueInput.blur();
}

valueInput.addEventListener('keydown', (ev) => {
  if (ev.key === 'Enter') commitValueInput();
  if (ev.key === 'Escape') hideValueInput();
});
valueInput.addEventListener('blur', () => commitValueInput());

function commitValueInput() {
  if (!editing) return;
  const text = valueInput.value.replace(',', '.').replace(/[^0-9.+-]/g, '');
  const v = parseFloat(text);
  if (Number.isFinite(v)) setParam(editing, v);
  hideValueInput();
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------
function closeMenu() { menuEl.hidden = true; menuEl.innerHTML = ''; }

function menuItem(label, onClick, checked) {
  const b = document.createElement('button');
  b.textContent = label;
  if (checked !== undefined) b.setAttribute('aria-checked', String(checked));
  b.onclick = () => { closeMenu(); onClick(); };
  return b;
}

function menuHeader(text) {
  const h = document.createElement('h2');
  h.textContent = text;
  return h;
}

function applyPreset(preset) {
  for (const id of Object.keys(state.params)) if (id in preset) state.params[id] = preset[id];
  state.driveOn = preset.driveOn;
  state.cab = preset.cab;
  applyCabMix();
  sendParams();
  saveState();
  paint();
  toast(`Preset: ${preset.name}`, 2000);
}

async function openMenu(ev, section) {
  menuEl.innerHTML = '';

  if (section === 'cab' || !section) {
    menuEl.appendChild(menuHeader(section === 'cab' ? 'Caixa (resposta de impulso)' : 'AURA Bass'));
  }

  if (section === 'cab') {
    BUILTIN_IRS.forEach((ir, i) => menuEl.appendChild(menuItem(`${ir.name} (embutida)`, () => setIr(i), state.irIndex === i && !state.customIr)));
    menuEl.appendChild(menuItem('Carregar IR (.wav)…', pickIrFile, state.customIr ? true : undefined));
  } else {
    menuEl.appendChild(menuHeader('Presets'));
    L.presets.forEach((p) => menuEl.appendChild(menuItem(p.name, () => applyPreset(p))));

    menuEl.appendChild(document.createElement('hr'));
    menuEl.appendChild(menuHeader('Entrada de áudio'));
    const select = document.createElement('select');
    select.innerHTML = '<option value="">Padrão do sistema</option>';
    try {
      const devices = await navigator.mediaDevices.enumerateDevices();
      devices.filter((d) => d.kind === 'audioinput').forEach((d, i) => {
        const o = document.createElement('option');
        o.value = d.deviceId;
        o.textContent = d.label || `Entrada ${i + 1}`;
        if (d.deviceId === state.inputDeviceId) o.selected = true;
        select.appendChild(o);
      });
    } catch (e) { /* sem permissão ainda */ }
    select.onchange = () => {
      state.inputDeviceId = select.value;
      saveState();
      closeMenu();
      toast('Recarregue a página para usar a nova entrada.', 4000);
    };
    menuEl.appendChild(select);

    const channels = ['Canal 1 (esquerda)', 'Canal 2 (direita)', 'Somar os dois'];
    channels.forEach((label, i) => menuEl.appendChild(menuItem(label, () => {
      state.channel = i; sendParams(); saveState();
    }, state.channel === i)));

    menuEl.appendChild(document.createElement('hr'));
    menuEl.appendChild(menuHeader('Dicas'));
    for (const tip of [
      'Arraste o knob para cima ou para baixo',
      'Shift + arrastar: ajuste fino',
      'Toque duplo no knob: valor padrão',
      'Toque no valor: digitar',
      'Teclas E, C e T: ENGAGE, CAB e TUNER',
    ]) {
      const d = document.createElement('div');
      d.className = 'tip';
      d.textContent = tip;
      menuEl.appendChild(d);
    }
    const version = document.createElement('div');
    version.className = 'tip';
    version.textContent = 'Versão web · mesmo processamento do plugin';
    menuEl.appendChild(version);

    menuEl.appendChild(document.createElement('hr'));
    menuEl.appendChild(menuHeader('Sobre'));
    for (const line of AUTORIA) {
      const d = document.createElement('div');
      d.className = 'tip';
      d.textContent = line;
      menuEl.appendChild(d);
    }
  }

  menuEl.hidden = false;
  const r = menuEl.getBoundingClientRect();
  const x = clamp((ev.clientX || window.innerWidth / 2) - r.width / 2, 10, window.innerWidth - r.width - 10);
  const y = clamp((ev.clientY || 60) + 12, 10, Math.max(10, window.innerHeight - r.height - 10));
  menuEl.style.left = `${x}px`;
  menuEl.style.top = `${y}px`;
}

document.addEventListener('pointerdown', (ev) => {
  if (!menuEl.hidden && !menuEl.contains(ev.target) && ev.target !== canvas) closeMenu();
});

function pickIrFile() {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = '.wav,.aif,.aiff,audio/*';
  input.onchange = async () => {
    const file = input.files && input.files[0];
    if (!file || !audio) return;
    try {
      const decoded = await audio.ctx.decodeAudioData(await file.arrayBuffer());
      await setIr(-1, { name: file.name.replace(/\.[^.]+$/, ''), buffer: normaliseIr(decoded) });
      toast('Resposta de impulso carregada.', 2500);
    } catch (e) {
      toast('Não consegui ler esse arquivo de áudio.');
    }
  };
  input.click();
}

// ---------------------------------------------------------------------------
// Início
// ---------------------------------------------------------------------------
function loadImages() {
  return Promise.all(Object.entries(IMAGES).map(([key, url]) => new Promise((resolve) => {
    const img = new Image();
    img.onload = () => { images[key] = img; resolve(); };
    img.onerror = () => resolve();
    img.src = url;
  })));
}

async function init() {
  loadState();
  fitCanvas();
  window.addEventListener('resize', fitCanvas);
  window.addEventListener('orientationchange', () => setTimeout(fitCanvas, 250));

  await loadImages();
  if (document.fonts && document.fonts.ready) { try { await document.fonts.ready; } catch (e) { /* segue */ } }
  paint();
  requestAnimationFrame(frame);

  if (!window.isSecureContext) startNote.textContent = 'O navegador só libera a entrada de áudio em páginas seguras (https).';
  startButton.addEventListener('click', start);

  if ('serviceWorker' in navigator) {
    try { await navigator.serviceWorker.register('sw.js'); } catch (e) { /* offline não disponível */ }
  }
}

// Exposto para os testes automatizados (Tests/web-ui-test.py)
window.__aura = { state, tuner, toggle, setParam, applyPreset, get audio() { return audio; } };

init();
