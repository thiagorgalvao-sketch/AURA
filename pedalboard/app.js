// WORSHIP BASS ESSENTIALS — interface e ligação com o motor de áudio.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA

'use strict';

const AUTORIA = [
  'WORSHIP BASS ESSENTIALS',
  'Copyright (c) 2026 Thiago Galvão',
  'Todos os direitos reservados — Leis 9.609/1998 e 9.610/1998',
  'github.com/thiagorgalvao-sketch/AURA',
];
const AURA_PROV = 'AURA-PROV/1 obra=WORSHIP-BASS-ESSENTIALS autor=Thiago-Galvao ano=2026 '
  + 'lei=9609/1998+9610/1998 origem=github.com/thiagorgalvao-sketch/AURA direitos=reservados';
try { window.AURA_PROV = AURA_PROV; } catch (e) { /* sem window */ }

const CHAVE = 'wbe-pedalboard-v1';

const $ = (s) => document.querySelector(s);
const boardEl = $('#board'), presetsEl = $('#presets'), menuEl = $('#menu');
const avisoEl = $('#aviso'), medidorEl = $('#medidor i'), cadeiaEl = $('#cadeiaTexto');

let ctx = null, rig = null, fonte = null, analisador = null, dados = null;

const estado = {
  preset: 'colorindo',
  pedais: {},          // id -> { on, params: {} }
  volume: 0,           // dB
  entradaId: '',       // dispositivo de entrada escolhido
  saidaId: '',         // dispositivo de saída escolhido
  entradaNome: '',     // só para mostrar qual entrada está em uso
};

// ---- Estado inicial a partir dos padrões lidos do MainStage ---------------

function estadoPadrao() {
  const p = {};
  for (const id of ORDEM_PEDAIS) {
    const base = PADROES[id] || {};
    const params = {};
    for (const k of (PEDAIS[id].knobs || [])) {
      params[k.id] = base[k.id] !== undefined ? base[k.id] : valorNeutro(id, k);
    }
    p[id] = { on: false, params };
  }
  return p;
}

function valorNeutro(id, k) {
  if (id === 'eq') return { b165: 2.5, b1030: -3.0, b1920: 3.3 }[k.id] ?? 0;
  if (id === 'eq2') return { hp: 20, bLow: 0, bMid: 0 }[k.id] ?? 0;
  if (k.id === 'mix') return 50;
  return (k.min + k.max) / 2;
}

// ---- Presets --------------------------------------------------------------

function achaPreset(id) { return PRESETS.find((p) => p.id === id) || PRESETS[0]; }

function aplicaPreset(id, { manterAjustes = false } = {}) {
  const pr = achaPreset(id);
  estado.preset = id;

  for (const pid of ORDEM_PEDAIS) {
    const ligado = pr.cadeia.includes(pid);
    estado.pedais[pid].on = ligado;

    if (!manterAjustes) {
      const base = PADROES[pid] || {};
      const extra = (pr.ajustes || {})[pid] || {};
      for (const k of PEDAIS[pid].knobs || []) {
        const v = extra[k.id] !== undefined ? extra[k.id]
                : base[k.id] !== undefined ? base[k.id]
                : valorNeutro(pid, k);
        estado.pedais[pid].params[k.id] = v;
      }
    }
  }

  // os dois EQs recebem a configuração de bandas do preset
  if (rig) {
    rig.blocos.eq.aplicaEq(pr.eq || EQ_BASE);
    rig.blocos.eq2.aplicaEq(pr.eq2 || EQ_BASE);
    if (pr.eq2) {
      estado.pedais.eq2.params.hp = pr.eq2.hp.on ? pr.eq2.hp.hz : 20;
      estado.pedais.eq2.params.bLow = pr.eq2.bands[1].db;
      estado.pedais.eq2.params.bMid = pr.eq2.bands[3].db;
    }
    estado.pedais.eq.params.b165 = (pr.eq || EQ_BASE).bands[1].db;
    estado.pedais.eq.params.b1030 = (pr.eq || EQ_BASE).bands[3].db;
    estado.pedais.eq.params.b1920 = (pr.eq || EQ_BASE).bands[4].db;
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
  rig.volume.gain.setTargetAtTime(Math.pow(10, estado.volume / 20), ctx.currentTime, 0.02);
}

// ---- Desenho --------------------------------------------------------------

function knobSvg(frac) {
  const a0 = -135, a1 = 135;
  const ang = a0 + (a1 - a0) * frac;
  const r = Math.PI / 180;
  const arco = (de, ate, raio) => {
    const x1 = 22 + raio * Math.sin(de * r), y1 = 22 - raio * Math.cos(de * r);
    const x2 = 22 + raio * Math.sin(ate * r), y2 = 22 - raio * Math.cos(ate * r);
    const big = Math.abs(ate - de) > 180 ? 1 : 0;
    return `M ${x1.toFixed(2)} ${y1.toFixed(2)} A ${raio} ${raio} 0 ${big} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
  };
  const px = 22 + 10.5 * Math.sin(ang * r), py = 22 - 10.5 * Math.cos(ang * r);
  return `<svg viewBox="0 0 44 44" aria-hidden="true">
    <path d="${arco(a0, a1, 19)}" fill="none" stroke="rgba(0,0,0,.45)" stroke-width="3" stroke-linecap="round"/>
    <path d="${arco(a0, Math.max(a0 + 0.01, ang), 19)}" fill="none" stroke="rgba(255,255,255,.85)" stroke-width="3" stroke-linecap="round"/>
    <circle cx="22" cy="22" r="13.5" fill="url(#g)" stroke="rgba(0,0,0,.6)"/>
    <defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#cfd4db"/><stop offset=".55" stop-color="#8d949f"/><stop offset="1" stop-color="#4d535c"/>
    </linearGradient></defs>
    <line x1="22" y1="22" x2="${px.toFixed(2)}" y2="${py.toFixed(2)}" stroke="#15171c" stroke-width="2.4" stroke-linecap="round"/>
  </svg>`;
}

function formata(k, v) {
  if (k.un === 'Hz') return v >= 1000 ? (v / 1000).toFixed(2) + ' k'
                    : v < 10 ? v.toFixed(2) + ' Hz' : Math.round(v) + ' Hz';
  if (k.un === '%' || k.un === 'ms') return Math.round(v) + (k.un === '%' ? '%' : ' ms');
  if (k.un === ':1') return v.toFixed(1) + ':1';
  if (k.un === 'dB') return (v > 0 ? '+' : '') + v.toFixed(1);
  return v.toFixed(1);
}

function desenha() {
  // presets
  presetsEl.innerHTML = '';
  for (const p of PRESETS) {
    const b = document.createElement('button');
    b.className = 'preset';
    b.style.setProperty('--cor', p.cor);
    b.setAttribute('aria-current', String(p.id === estado.preset));
    b.innerHTML = `<b>${p.nome}</b><span>${p.sub}</span>`;
    b.onclick = () => aplicaPreset(p.id);
    presetsEl.appendChild(b);
  }

  // pedais
  boardEl.innerHTML = '';
  for (const pid of ORDEM_PEDAIS) {
    const def = PEDAIS[pid], st = estado.pedais[pid];
    const el = document.createElement('section');
    el.className = 'pedal' + (st.on ? '' : ' off');
    el.style.setProperty('--cor', def.cor);
    el.dataset.pedal = pid;

    const knobs = (def.knobs || []).map((k) => {
      const v = st.params[k.id];
      const frac = (v - k.min) / (k.max - k.min);
      return `<div class="knob" data-knob="${k.id}">${knobSvg(Math.max(0, Math.min(1, frac)))}
        <div class="rot">${k.rot}</div><div class="val">${formata(k, v)}</div></div>`;
    }).join('');

    el.innerHTML = `<h2>${def.nome}</h2><div class="tipo">${def.sub}</div>
      <div class="knobs">${knobs}</div>
      <div class="rodapePedal"><i class="led"></i>
        <button class="foot" aria-label="Ligar ou desligar ${def.nome}"></button></div>`;
    boardEl.appendChild(el);
  }

  const pr = achaPreset(estado.preset);
  cadeiaEl.innerHTML = 'Cadeia: ' + pr.cadeia.map((c) => PEDAIS[c].nome).join(' → ')
    + ' · <a href="../" style="color:inherit">AURA Bass</a>';
}

// ---- Interação ------------------------------------------------------------

let arrasto = null, ultimoToque = { t: 0, alvo: null };

boardEl.addEventListener('pointerdown', (ev) => {
  const foot = ev.target.closest('.foot');
  if (foot) {
    const pid = foot.closest('.pedal').dataset.pedal;
    estado.pedais[pid].on = !estado.pedais[pid].on;
    rig && rig.blocos[pid].bypass(estado.pedais[pid].on);
    foot.closest('.pedal').classList.toggle('off', !estado.pedais[pid].on);
    salva();
    return;
  }
  const knobEl = ev.target.closest('.knob');
  if (!knobEl) return;
  const pid = knobEl.closest('.pedal').dataset.pedal;
  const kid = knobEl.dataset.knob;
  const def = PEDAIS[pid].knobs.find((k) => k.id === kid);

  const agora = performance.now();
  if (ultimoToque.alvo === pid + kid && agora - ultimoToque.t < 400) {
    const base = (PADROES[pid] || {})[kid];
    poeValor(pid, kid, base !== undefined ? base : valorNeutro(pid, def));
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
    alvo.querySelector('svg').outerHTML = knobSvg(frac);
    alvo.querySelector('.val').textContent = formata(def, v);
  }
}

window.addEventListener('keydown', (ev) => {
  if (ev.key >= '1' && ev.key <= '8') {
    const p = PRESETS[+ev.key - 1];
    if (p) aplicaPreset(p.id);
  }
  if (ev.key === 'Escape') fechaMenu();
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

  const dica = (t) => { const e = document.createElement('div'); e.className = 'dica'; e.textContent = t; menuEl.appendChild(e); };

  const ref = menuEl.querySelector('#ancoraPresets');
  const poe = (el) => ref ? menuEl.insertBefore(el, ref) : menuEl.appendChild(el);

  poe(seletor('audioinput', estado.entradaId, 'Entrada', trocaEntrada));
  if (estado.entradaNome) { const e = document.createElement('div'); e.className = 'dica'; e.textContent = 'Em uso: ' + estado.entradaNome; poe(e); }
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
    rig && rig.volume.gain.setTargetAtTime(Math.pow(10, v / 20), ctx.currentTime, 0.05);
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
  b('Ligar todos os pedais', () => { ORDEM_PEDAIS.forEach((p) => { estado.pedais[p].on = true; }); mandaTudo(); desenha(); salva(); });
  b('Desligar todos os pedais', () => { ORDEM_PEDAIS.forEach((p) => { estado.pedais[p].on = false; }); mandaTudo(); desenha(); salva(); });

  menuEl.appendChild(document.createElement('hr'));
  h('Dicas');
  d('Arraste o knob para cima ou para baixo');
  d('Shift + arrastar: ajuste fino');
  d('Toque duplo no knob: valor original');
  d('Teclas 1 a 8: trocar de preset');

  menuEl.appendChild(document.createElement('hr'));
  h('Sobre');
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
    // aproveita o estado salvo sob o nome antigo do app
    if (!localStorage.getItem(CHAVE)) {
      const velho = localStorage.getItem('cortes-pedalboard-v1');
      if (velho) { localStorage.setItem(CHAVE, velho); localStorage.removeItem('cortes-pedalboard-v1'); }
    }
    const s = JSON.parse(localStorage.getItem(CHAVE) || 'null');
    if (!s || !s.pedais) return false;
    estado.preset = s.preset || estado.preset;
    estado.volume = typeof s.volume === 'number' ? s.volume : 0;
    if (typeof s.entradaId === 'string') estado.entradaId = s.entradaId;
    if (typeof s.saidaId === 'string') estado.saidaId = s.saidaId;
    for (const pid of ORDEM_PEDAIS) {
      if (!s.pedais[pid]) continue;
      estado.pedais[pid].on = !!s.pedais[pid].on;
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

async function carregaIR(c) {
  try {
    const r = await fetch('assets/cab_4x10.wav');
    if (!r.ok) return null;
    return await c.decodeAudioData(await r.arrayBuffer());
  } catch (e) { return null; }
}

async function liga() {
  const entrada = await navigator.mediaDevices.getUserMedia(restricoes(estado.entradaId));

  ctx = new (window.AudioContext || window.webkitAudioContext)({ latencyHint: 'interactive' });
  await ctx.audioWorklet.addModule('pb-processor.js');
  const ir = await carregaIR(ctx);

  fonte = ctx.createMediaStreamSource(entrada);
  rig = montaCadeia(ctx, ORDEM_PEDAIS, ir);
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

  $('#inicio').hidden = true;
  const atraso = Math.round((ctx.baseLatency + (ctx.outputLatency || 0)) * 1000);
  mostra(`Entrando por: ${estado.entradaNome || 'entrada padrão'} · ${atraso} ms`
       + `${ir ? '' : ' · sem a resposta de alto-falante'}`, 5200);
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

$('#botaoLigar').onclick = async () => {
  $('#botaoLigar').disabled = true;
  try {
    await liga();
  } catch (e) {
    $('#botaoLigar').disabled = false;
    mostra('Não consegui abrir a entrada de áudio: ' + (e && e.message ? e.message : e), 6000);
  }
};

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
};
