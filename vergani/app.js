// VERGANI PEDALBOARD — interface e ligação com o motor de áudio.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive

'use strict';

const VERSAO_SITE = '2d6f9fe';   // trocado na publicação por Tools/versiona-site.py
const AUTORIA = [
  'VERGANI PEDALBOARD',
  'Copyright (c) 2026 Thiago Galvão',
  'Todos os direitos reservados — Leis 9.609/1998 e 9.610/1998',
  'github.com/ledixplugins/Drive',
];
const AURA_PROV = 'AURA-PROV/1 obra=VERGANI-PEDALBOARD autor=Thiago-Galvao ano=2026 '
  + 'lei=9609/1998+9610/1998 origem=github.com/ledixplugins/Drive direitos=reservados';
try { window.AURA_PROV = AURA_PROV; } catch (e) { /* sem window */ }

const CHAVE = 'vergani-pedalboard-v1';

const $ = (s) => document.querySelector(s);
const boardEl = $('#board'), presetsEl = $('#presets'), menuEl = $('#menu');
const avisoEl = $('#aviso'), medidorEl = $('#medidor i'), cadeiaEl = $('#cadeiaTexto');

let ctx = null, rig = null, fonte = null, analisador = null, dados = null;

const estado = {
  preset: 'limpo',
  pedais: {},          // id -> { on, params: {} }
  volume: 0,           // dB
  entradaId: '',       // dispositivo de entrada escolhido
  saidaId: '',         // dispositivo de saída escolhido
  entradaNome: '',     // só para mostrar qual entrada está em uso
};

// ---- Estado inicial a partir dos padrões ----------------------------------

function estadoPadrao() {
  const p = {};
  for (const id of ORDEM_PEDAIS) {
    const params = {};
    for (const k of PEDAIS[id].knobs) params[k.id] = valorPadrao(id, k);
    p[id] = { on: false, params };
  }
  return p;
}

function valorPadrao(id, k) {
  const v = (PADROES[id] || {})[k.id];
  return v !== undefined ? v : (k.min + k.max) / 2;
}

// ---- Presets --------------------------------------------------------------

function achaPreset(id) { return PRESETS.find((p) => p.id === id) || PRESETS[0]; }

function aplicaPreset(id, { manterAjustes = false } = {}) {
  const pr = achaPreset(id);
  estado.preset = pr.id;

  for (const pid of ORDEM_PEDAIS) {
    estado.pedais[pid].on = pr.cadeia.includes(pid);
    if (manterAjustes) continue;
    const extra = (pr.ajustes || {})[pid] || {};
    for (const k of PEDAIS[pid].knobs) {
      estado.pedais[pid].params[k.id] = extra[k.id] !== undefined ? extra[k.id] : valorPadrao(pid, k);
    }
  }

  mandaTudo();
  desenha();
  salva();
}

function mandaTudo() {
  if (!rig) return;
  for (const pid of ORDEM_PEDAIS) {
    const b = rig.blocos[pid], st = estado.pedais[pid];
    b.bypass(st.on);
    for (const [k, v] of Object.entries(st.params)) b.set(k, v);
  }
  aplicaVolume(0.02);
}

/** Volume geral; com o afinador ligado a saída fica totalmente muda (inclusive caudas). */
function aplicaVolume(tau) {
  if (!rig) return;
  const g = estado.pedais.tuner.on ? 0 : Math.pow(10, estado.volume / 20);
  rig.volume.gain.setTargetAtTime(g, ctx.currentTime, tau);
}

// ---- Desenho --------------------------------------------------------------

function knobSvg(frac) {
  const a0 = -135, a1 = 135, c = 28;
  const ang = a0 + (a1 - a0) * frac;
  const r = Math.PI / 180;
  const pt = (graus, raio) => [c + raio * Math.sin(graus * r), c - raio * Math.cos(graus * r)];
  // 11 pontos de escala; os que o ponteiro já passou acendem na cor do pedal
  let marcas = '';
  for (let i = 0; i <= 10; i++) {
    const g = a0 + (a1 - a0) * (i / 10), acesa = i / 10 <= frac + 1e-6;
    const [x1, y1] = pt(g, 24.2), [x2, y2] = pt(g, 27.2);
    marcas += `<line x1="${x1.toFixed(2)}" y1="${y1.toFixed(2)}" x2="${x2.toFixed(2)}" y2="${y2.toFixed(2)}" `
      + (acesa ? 'style="stroke:var(--luz)" stroke-width="1.7"' : 'stroke="rgba(255,255,255,.28)" stroke-width="1.2"')
      + ' stroke-linecap="round"/>';
  }
  const arco = (de, ate) => {
    const [x1, y1] = pt(de, 22.3), [x2, y2] = pt(ate, 22.3);
    return `M ${x1.toFixed(2)} ${y1.toFixed(2)} A 22.3 22.3 0 ${Math.abs(ate - de) > 180 ? 1 : 0} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
  };
  return `<div class="k" aria-hidden="true">
    <svg viewBox="0 0 56 56">${marcas}
      <path d="${arco(a0, a1)}" fill="none" stroke="rgba(0,0,0,.55)" stroke-width="2" stroke-linecap="round"/>
      <path d="${arco(a0, Math.max(a0 + 0.01, ang))}" fill="none" style="stroke:var(--luz)" stroke-width="1.6" stroke-linecap="round" opacity=".9"/></svg>
    <i class="sombra"></i><i class="anel"></i><i class="tampa"></i><i class="pont" style="transform:rotate(${ang.toFixed(1)}deg)"></i></div>`;
}

function formata(k, v) {
  if (k.un === 'Hz') return v >= 1000 ? (v / 1000).toFixed(2) + ' k'
                    : v < 10 ? v.toFixed(2) + ' Hz' : Math.round(v) + ' Hz';
  if (k.un === '%') return Math.round(v) + '%';
  if (k.un === 'ms') return Math.round(v) + ' ms';
  if (k.un === 'ct') return (v > 0 ? '+' : '') + Math.round(v) + ' ct';
  if (k.un === 'dB') return (v > 0 ? '+' : '') + v.toFixed(1);
  return Math.round(v).toString();
}

function desenha() {
  // presets
  presetsEl.innerHTML = '';
  PRESETS.forEach((p, i) => {
    const b = document.createElement('button');
    b.className = 'preset';
    b.dataset.preset = p.id;
    b.style.setProperty('--cor', p.cor);
    b.setAttribute('aria-current', String(p.id === estado.preset));
    b.innerHTML = `<b>${i + 1} · ${p.nome}</b><span>${p.sub}</span>`;
    b.onclick = () => aplicaPreset(p.id);
    presetsEl.appendChild(b);
  });

  // pedais
  boardEl.innerHTML = '';
  for (const pid of ORDEM_PEDAIS) {
    const def = PEDAIS[pid], st = estado.pedais[pid];
    const el = document.createElement('section');
    el.className = 'pedal' + (st.on ? '' : ' off') + (pid === 'tuner' ? ' afinador' : '');
    el.style.setProperty('--cor', def.cor);
    el.style.setProperty('--luz', def.luz);
    el.dataset.pedal = pid;

    const knobs = def.knobs.map((k) => {
      const v = st.params[k.id];
      const frac = (v - k.min) / (k.max - k.min);
      return `<div class="knob" data-knob="${k.id}">${knobSvg(Math.max(0, Math.min(1, frac)))}
        <div class="rot">${k.rot}</div><div class="val">${formata(k, v)}</div></div>`;
    }).join('');

    el.innerHTML = `<i class="jack e"></i><i class="jack d"></i><h2>${def.nome}</h2><div class="tipo">${def.sub}</div>
      <div class="knobs">${knobs}</div>
      ${pid === 'tuner' ? '<div class="afina"><div class="nota">--</div><div class="hzv">&nbsp;</div><div class="escala"><i class="agulha"></i></div></div>' : ''}
      <div class="rodapePedal"><i class="led"></i>
        <button class="foot" aria-label="Ligar ou desligar ${def.nome}"></button></div>`;
    boardEl.appendChild(el);
  }

  cadeiaEl.textContent = 'Ordem fixa: ' + ORDEM_PEDAIS.map((c) => PEDAIS[c].nome).join(' → ')
    + ' · true bypass';
}

// ---- Interação ------------------------------------------------------------

function alternaPedal(pid) {
  estado.pedais[pid].on = !estado.pedais[pid].on;
  rig && rig.blocos[pid].bypass(estado.pedais[pid].on);
  const el = boardEl.querySelector(`[data-pedal="${pid}"]`);
  if (el) el.classList.toggle('off', !estado.pedais[pid].on);
  if (pid === 'tuner') { afina.resetar = true; mostraAfinador(false); aplicaVolume(0.01); }
  salva();
}

let arrasto = null, ultimoToque = { t: 0, alvo: null };

boardEl.addEventListener('pointerdown', (ev) => {
  const foot = ev.target.closest('.foot');
  if (foot) {
    alternaPedal(foot.closest('.pedal').dataset.pedal);
    return;
  }
  const knobEl = ev.target.closest('.knob');
  if (!knobEl) return;
  const pid = knobEl.closest('.pedal').dataset.pedal;
  const kid = knobEl.dataset.knob;
  const def = PEDAIS[pid].knobs.find((k) => k.id === kid);

  const agora = performance.now();
  if (ultimoToque.alvo === pid + kid && agora - ultimoToque.t < 400) {
    poeValor(pid, kid, valorPadrao(pid, def));
    ultimoToque = { t: 0, alvo: null };
    return;
  }
  ultimoToque = { t: agora, alvo: pid + kid };

  knobEl.setPointerCapture(ev.pointerId);
  arrasto = { pid, kid, def, y0: ev.clientY, v0: estado.pedais[pid].params[kid], el: knobEl };
  ev.preventDefault();
});

boardEl.addEventListener('pointermove', (ev) => {
  if (!arrasto) return;
  const { def } = arrasto;
  const faixa = def.max - def.min;
  const sens = ev.shiftKey ? 900 : 190;
  let v = arrasto.v0 + ((arrasto.y0 - ev.clientY) / sens) * faixa;
  v = Math.max(def.min, Math.min(def.max, Math.round(v / def.passo) * def.passo));
  poeValor(arrasto.pid, arrasto.kid, v, arrasto.el);
});

const solta = () => { if (arrasto) { salva(); arrasto = null; } };
boardEl.addEventListener('pointerup', solta);
boardEl.addEventListener('pointercancel', solta);

function poeValor(pid, kid, v, el) {
  estado.pedais[pid].params[kid] = v;
  rig && rig.blocos[pid].set(kid, v);
  const def = PEDAIS[pid].knobs.find((k) => k.id === kid);
  const alvo = el || boardEl.querySelector(`[data-pedal="${pid}"] [data-knob="${kid}"]`);
  if (alvo && def) {
    const frac = Math.max(0, Math.min(1, (v - def.min) / (def.max - def.min)));
    alvo.querySelector('.k').outerHTML = knobSvg(frac);
    alvo.querySelector('.val').textContent = formata(def, v);
  }
}

window.addEventListener('keydown', (ev) => {
  if (ev.key >= '1' && ev.key <= '8') {
    const p = PRESETS[+ev.key - 1];
    if (p) aplicaPreset(p.id);
  }
  if (ev.key === 'Escape') fechaMenu();
  if ((ev.key === 't' || ev.key === 'T') && !ev.metaKey && !ev.ctrlKey && !menuEl.contains(document.activeElement)) {
    alternaPedal('tuner');
  }
});

// ---- Menu -----------------------------------------------------------------

function fechaMenu() { menuEl.hidden = true; menuEl.innerHTML = ''; }

/** Lista de entradas e saídas, aplicando a escolha na hora. */
async function montaDispositivos() {
  const marca = document.createElement('div');
  marca.className = 'dica';
  marca.textContent = 'Procurando dispositivos...';
  menuEl.appendChild(marca);

  let lista = [];
  try { lista = await navigator.mediaDevices.enumerateDevices(); } catch (e) { /* sem permissão */ }
  if (menuEl.hidden) return;
  marca.remove();

  const seletor = (tipo, atual, rotulo, aoTrocar) => {
    const sel = document.createElement('select');
    const padrao = document.createElement('option');
    padrao.value = ''; padrao.textContent = 'Padrão do sistema';
    sel.appendChild(padrao);
    lista.filter((d) => d.kind === tipo).forEach((d, i) => {
      const o = document.createElement('option');
      o.value = d.deviceId;
      o.textContent = d.label || `${rotulo} ${i + 1}`;
      if (d.deviceId === atual) o.selected = true;
      sel.appendChild(o);
    });
    sel.onchange = () => { aoTrocar(sel.value); fechaMenu(); };
    return sel;
  };

  const ref = menuEl.querySelector('#ancoraPresets');
  const poe = (el) => ref ? menuEl.insertBefore(el, ref) : menuEl.appendChild(el);

  poe(seletor('audioinput', estado.entradaId, 'Entrada', trocaEntrada));
  if (estado.entradaNome) {
    const e = document.createElement('div'); e.className = 'dica';
    e.textContent = 'Em uso: ' + estado.entradaNome; poe(e);
  }
  if (ctx && typeof ctx.setSinkId === 'function') {
    poe(seletor('audiooutput', estado.saidaId, 'Saída', trocaSaida));
  } else {
    const e = document.createElement('div'); e.className = 'dica';
    e.textContent = ctx ? 'Este navegador usa a saída padrão do sistema.' : 'Ligue o áudio para escolher os dispositivos.';
    poe(e);
  }
}

function abreMenu() {
  menuEl.innerHTML = '';
  const h = (t) => { const e = document.createElement('h3'); e.textContent = t; menuEl.appendChild(e); };
  const b = (t, fn) => { const e = document.createElement('button'); e.textContent = t; e.onclick = () => { fn(); fechaMenu(); }; menuEl.appendChild(e); };
  const d = (t) => { const e = document.createElement('div'); e.className = 'dica'; e.textContent = t; menuEl.appendChild(e); };

  h('Volume geral');
  for (const v of [-12, -6, 0, 6]) b(`${v > 0 ? '+' : ''}${v} dB${estado.volume === v ? '  ✓' : ''}`, () => {
    estado.volume = v;
    aplicaVolume(0.05);
    salva();
  });

  menuEl.appendChild(document.createElement('hr'));
  h('Entrada e saída');
  montaDispositivos();

  const ancora = document.createElement('hr');
  ancora.id = 'ancoraPresets';
  menuEl.appendChild(ancora);
  h('Presets');
  b('Voltar o preset aos valores originais', () => aplicaPreset(estado.preset));
  b('Ligar todos os pedais', () => { ORDEM_PEDAIS.forEach((p) => { estado.pedais[p].on = p !== 'tuner'; }); mandaTudo(); desenha(); salva(); });
  b('Desligar todos os pedais', () => { ORDEM_PEDAIS.forEach((p) => { estado.pedais[p].on = false; }); mandaTudo(); desenha(); salva(); });

  menuEl.appendChild(document.createElement('hr'));
  h('Dicas');
  d('Arraste o knob para cima ou para baixo');
  d('Shift + arrastar: ajuste fino');
  d('Toque duplo no knob: valor original');
  d('Teclas 1 a 8: trocar de preset');
  d('Tecla T: liga e desliga o afinador (deixa o som mudo)');
  d('Todos os pedais são true bypass: desligado, o sinal passa direto');

  menuEl.appendChild(document.createElement('hr'));
  h('Sobre');
  d('Versão ' + (VERSAO_SITE.startsWith('AURA_') ? 'de desenvolvimento' : VERSAO_SITE));
  AUTORIA.forEach(d);

  menuEl.hidden = false;
  const r = menuEl.getBoundingClientRect();
  menuEl.style.left = Math.max(8, window.innerWidth - r.width - 12) + 'px';
  menuEl.style.top = '56px';
}

$('#botaoMenu').onclick = (ev) => { ev.stopPropagation(); menuEl.hidden ? abreMenu() : fechaMenu(); };
document.addEventListener('pointerdown', (ev) => {
  if (!menuEl.hidden && !menuEl.contains(ev.target) && ev.target !== $('#botaoMenu')) fechaMenu();
});

// ---- Estado salvo ---------------------------------------------------------

function salva() {
  try { localStorage.setItem(CHAVE, JSON.stringify(estado)); } catch (e) { /* sem storage */ }
}

function carrega() {
  try {
    const s = JSON.parse(localStorage.getItem(CHAVE) || 'null');
    if (!s || !s.pedais) return false;
    estado.preset = s.preset || estado.preset;
    estado.volume = typeof s.volume === 'number' ? s.volume : 0;
    if (typeof s.entradaId === 'string') estado.entradaId = s.entradaId;
    if (typeof s.saidaId === 'string') estado.saidaId = s.saidaId;
    for (const pid of ORDEM_PEDAIS) {
      if (!s.pedais[pid]) continue;
      estado.pedais[pid].on = pid === 'tuner' ? false : !!s.pedais[pid].on;
      Object.assign(estado.pedais[pid].params, s.pedais[pid].params || {});
    }
    return true;
  } catch (e) { return false; }
}

function mostra(txt, ms = 3200) {
  avisoEl.textContent = txt; avisoEl.hidden = false;
  clearTimeout(mostra._t);
  mostra._t = setTimeout(() => { avisoEl.hidden = true; }, ms);
}

// ---- Áudio ----------------------------------------------------------------

function pareceMicrofoneInterno(label) {
  return /micro|microphone|interno|internal|built-?in|embutid|padr[ãa]o|default/i.test(label || '');
}

/** Passo 1: pede a permissão (sem usar o áudio) e lista as entradas. */
async function escolheEntrada() {
  const nota = $('#notaInicio'), caixa = $('#escolhaEntrada'), sel = $('#entradaSelect');
  nota.textContent = 'Pedindo acesso ao áudio...';
  try {
    const permissao = await navigator.mediaDevices.getUserMedia({ audio: true });
    permissao.getTracks().forEach((t) => t.stop());

    const entradas = (await navigator.mediaDevices.enumerateDevices())
      .filter((d) => d.kind === 'audioinput');
    if (!entradas.length) {
      nota.textContent = 'Nenhuma entrada encontrada. Ligue a interface e toque em Ligar de novo.';
      $('#botaoLigar').disabled = false;
      return;
    }

    const salva = entradas.find((d) => d.deviceId === estado.entradaId);
    if (salva) { await liga(salva.deviceId); return; }

    sel.innerHTML = '';
    const vazio = document.createElement('option');
    vazio.value = ''; vazio.textContent = 'Escolha a entrada…';
    sel.appendChild(vazio);
    entradas.forEach((d, i) => {
      const o = document.createElement('option');
      o.value = d.deviceId;
      o.textContent = d.label || `Entrada ${i + 1}`;
      sel.appendChild(o);
    });
    const externas = entradas.filter((d) => !pareceMicrofoneInterno(d.label));
    if (externas.length === 1) sel.value = externas[0].deviceId;

    $('#entrarButton').disabled = !sel.value;
    $('#botaoLigar').hidden = true;
    caixa.hidden = false;
    nota.textContent = estado.entradaId
      ? 'A entrada usada antes não está mais ligada. Escolha outra.'
      : 'Escolha por onde a guitarra entra.';
  } catch (e) {
    $('#botaoLigar').disabled = false;
    nota.textContent = (e && e.name) === 'NotAllowedError'
      ? 'Acesso negado. Libere o microfone nas permissões do site e tente de novo.'
      : 'Não consegui acessar o áudio: ' + ((e && e.message) || e);
  }
}

/** Passo 2: abre a entrada escolhida e monta a cadeia. */
async function liga(deviceId) {
  const entrada = await navigator.mediaDevices.getUserMedia(restricoes(deviceId));
  estado.entradaId = deviceId || '';

  ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
  await ctx.audioWorklet.addModule('gt-processor.js');

  fonte = ctx.createMediaStreamSource(entrada);
  rig = montaCadeia(ctx, ORDEM_PEDAIS);
  fonte.connect(rig.entrada);
  rig.saida.connect(ctx.destination);

  analisador = ctx.createAnalyser();
  analisador.fftSize = 1024;
  dados = new Float32Array(analisador.fftSize);
  fonte.connect(analisador);

  if (ctx.state === 'suspended') await ctx.resume();

  aplicaPreset(estado.preset, { manterAjustes: true });
  mandaTudo();

  estado.entradaNome = nomeDaEntrada(entrada);
  await aplicaSaida(estado.saidaId);

  salva();
  $('#escolhaEntrada').hidden = true;
  $('#inicio').hidden = true;
  const atraso = Math.round((ctx.baseLatency + (ctx.outputLatency || 0)) * 1000);
  mostra(`Entrando por: ${estado.entradaNome || 'entrada padrão'} · ${atraso} ms`, 5200);
  medeEntrada();
}

function restricoes(id) {
  return {
    audio: {
      echoCancellation: false, noiseSuppression: false, autoGainControl: false,
      channelCount: 1, latency: 0,
      ...(id ? { deviceId: { exact: id } } : {}),
    },
  };
}

function nomeDaEntrada(stream) {
  const t = stream && stream.getAudioTracks && stream.getAudioTracks()[0];
  return t ? (t.label || '') : '';
}

/** Troca a entrada sem recarregar: abre a nova e religa o grafo. */
async function trocaEntrada(id) {
  if (!ctx) { estado.entradaId = id; salva(); return; }
  try {
    const novo = await navigator.mediaDevices.getUserMedia(restricoes(id));
    const velho = fonte && fonte.mediaStream;
    try { fonte.disconnect(); } catch (e) { /* já solto */ }
    fonte = ctx.createMediaStreamSource(novo);
    fonte.connect(rig.entrada);
    if (analisador) fonte.connect(analisador);
    if (velho) velho.getTracks().forEach((t) => t.stop());
    estado.entradaId = id;
    estado.entradaNome = nomeDaEntrada(novo);
    salva();
    mostra('Entrando por: ' + (estado.entradaNome || 'entrada padrão'));
  } catch (e) {
    mostra('Não consegui abrir essa entrada: ' + ((e && e.message) || e), 6000);
  }
}

async function aplicaSaida(id) {
  if (!ctx || !id || typeof ctx.setSinkId !== 'function') return !id;
  try { await ctx.setSinkId(id); return true; } catch (e) { return false; }
}

async function trocaSaida(id) {
  estado.saidaId = id;
  salva();
  const ok = await aplicaSaida(id);
  if (!ok && id) mostra('Este navegador não deixa escolher a saída. Troque o padrão no sistema.', 6000);
  else if (ok) mostra('Saída trocada.');
}

function medeEntrada() {
  if (!analisador) return;
  analisador.getFloatTimeDomainData(dados);
  let pico = 0;
  for (let i = 0; i < dados.length; i++) pico = Math.max(pico, Math.abs(dados[i]));
  medidorEl.style.width = Math.min(100, pico * 140) + '%';
  requestAnimationFrame(medeEntrada);
}

$('#botaoLigar').onclick = () => { $('#botaoLigar').disabled = true; escolheEntrada(); };
$('#entradaSelect').onchange = () => { $('#entrarButton').disabled = !$('#entradaSelect').value; };
$('#entrarButton').onclick = async () => {
  $('#entrarButton').disabled = true;
  $('#notaInicio').textContent = 'Abrindo a entrada...';
  try {
    await liga($('#entradaSelect').value);
  } catch (e) {
    $('#entrarButton').disabled = false;
    $('#notaInicio').textContent = ['NotFoundError', 'OverconstrainedError'].includes(e && e.name)
      ? 'Essa entrada não está mais disponível. Escolha outra na lista.'
      : 'Não consegui abrir essa entrada: ' + ((e && e.message) || e);
  }
};

// ---- Afinador -------------------------------------------------------------
// Lê a entrada (antes de qualquer pedal) e mostra a nota e os cents. Com o
// afinador ligado a saída fica muda; desligado, ele não está no caminho do som.

const afina = {
  det: window.AuraTuner ? new window.AuraTuner.TunerDetector() : null,
  buf: null, nota: null, cents: 0, hz: 0, ultimo: 0, resetar: true,
};

function mostraAfinador(valido) {
  const el = boardEl.querySelector('.afina');
  if (!el) return;
  const nota = el.querySelector('.nota'), hz = el.querySelector('.hzv'), ag = el.querySelector('.agulha');
  if (valido && afina.nota !== null) {
    const T = window.AuraTuner;
    nota.textContent = T.noteName(afina.nota) + T.noteOctave(afina.nota);
    hz.textContent = afina.hz.toFixed(1) + ' Hz · ' + (afina.cents > 0 ? '+' : '') + Math.round(afina.cents) + ' ct';
    ag.style.left = (50 + Math.max(-50, Math.min(50, afina.cents))) + '%';
    el.classList.toggle('ok', Math.abs(afina.cents) <= 5);
  } else {
    nota.textContent = '--'; hz.innerHTML = '&nbsp;';
    ag.style.left = '50%'; el.classList.remove('ok');
  }
}

function cicloAfinador() {
  if (!ctx || !rig || !afina.det || !estado.pedais.tuner.on) return;
  const an = rig.blocos.tuner.analisador;
  if (afina.resetar) { afina.det.reset(); afina.nota = null; afina.resetar = false; }
  if (!afina.buf || afina.buf.length !== an.fftSize) afina.buf = new Float32Array(an.fftSize);
  an.getFloatTimeDomainData(afina.buf);
  const f = Math.max(1, Math.round(ctx.sampleRate / 12000));       // ~12 kHz, como o detector espera
  const n = Math.floor(afina.buf.length / f), bloco = new Float32Array(n);
  for (let i = 0; i < n; i++) { let s = 0; for (let j = 0; j < f; j++) s += afina.buf[i * f + j]; bloco[i] = s / f; }
  afina.det.push(bloco, ctx.sampleRate / f);
  const r = afina.det.analyse();
  const agora = performance.now();
  if (r.valid) {
    const m = 69 + 12 * Math.log2(r.hz / estado.pedais.tuner.params.ref);
    const nota = Math.round(m), cents = (m - nota) * 100;
    afina.cents = (afina.nota === nota && agora - afina.ultimo < 800) ? afina.cents + 0.4 * (cents - afina.cents) : cents;
    afina.nota = nota; afina.hz = r.hz; afina.ultimo = agora;
  }
  mostraAfinador(agora - afina.ultimo < 700);
}
setInterval(cicloAfinador, 90);

// ---- Início ---------------------------------------------------------------

estado.pedais = estadoPadrao();
if (!carrega()) aplicaPreset(estado.preset);
else desenha();

if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker.register('sw.js').catch(() => { /* offline indisponível */ });
  });
}

// Para os testes automatizados
window.__pb = {
  estado,
  aplicaPreset,
  poeValor,
  get audio() { return ctx; },
  get rig() { return rig; },
  alternaPedal,
};
