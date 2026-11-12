// CORTES Pedalboard — presets extraídos do concerto do MainStage
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
//
// Origem: "PRESET LOGIC : MAIN STAGE CORTES - FINAL" (8 patches .patch + concerto).
// A cadeia de plugins e os números abaixo foram lidos dos arquivos #Root.cst.
// Onde o formato do Logic não permite saber a que knob um número corresponde,
// o valor foi ajustado de ouvido para o mesmo caráter — está marcado com ~.

'use strict';

// ---- Blocos reutilizados pelos patches ------------------------------------

// Channel EQ do Logic: bandas (ligada, frequência, ganho dB, Q)
const EQ_BASE = {        // #default.pst — o EQ que está em todos os patches
  hp:    { on: false, hz: 20,   q: 0.71 },
  bands: [
    { on: true, tipo: 'lowshelf', hz: 75,   db: 0,    q: 1.0  },
    { on: true, tipo: 'peaking',  hz: 165,  db: 2.5,  q: 0.6  },
    { on: true, tipo: 'peaking',  hz: 250,  db: 0,    q: 0.3  },
    { on: true, tipo: 'peaking',  hz: 1030, db: -3.0, q: 0.41 },
    { on: true, tipo: 'peaking',  hz: 1920, db: 3.3,  q: 0.93 },
  ],
};

const EQ_SUB = {         // pós-oitava: empurra 172 Hz e tira 805 Hz
  hp:    { on: false, hz: 20, q: 0.71 },
  bands: [
    { on: true, tipo: 'lowshelf', hz: 75,   db: 0,     q: 1.0  },
    { on: true, tipo: 'peaking',  hz: 172,  db: 5.7,   q: 0.6  },
    { on: true, tipo: 'peaking',  hz: 250,  db: 0,     q: 0.3  },
    { on: true, tipo: 'peaking',  hz: 805,  db: -12.0, q: 0.41 },
    { on: true, tipo: 'peaking',  hz: 2500, db: 0,     q: 0.2  },
  ],
};

const EQ_DRIVE2 = {      // pós-drive estágio 2: corta grave em 73 Hz
  hp:    { on: true, hz: 73, q: 1.0 },
  bands: [
    { on: true, tipo: 'lowshelf', hz: 75,   db: 0,   q: 1.0  },
    { on: true, tipo: 'peaking',  hz: 100,  db: 0,   q: 0.6  },
    { on: true, tipo: 'peaking',  hz: 250,  db: 0,   q: 0.2  },
    { on: true, tipo: 'peaking',  hz: 1040, db: 0,   q: 0.41 },
    { on: true, tipo: 'peaking',  hz: 735,  db: 1.6, q: 0.35 },
  ],
};

const EQ_TUDO = {        // igual ao de cima, mas com o corte em 127 Hz
  hp:    { on: true, hz: 127, q: 0.88 },
  bands: EQ_DRIVE2.bands,
};

// ---- Ajustes de fábrica de cada pedal --------------------------------------

const PADROES = {
  consoleEq: { low: 0.5, lowMid: -0.1, highMid: 7.2, high: 0.5, hp: 50 },   // CONSOLE VINTAGE
  comp:      { thresh: -22.5, ratio: 6.8, attack: 10, release: 48, makeup: 8 },
  amp:       { gain: 2.8, bass: 7.5, mid: 6.6, treble: 7.4, master: 6.2, drive: 1.8 },
  drive:     { drive: 37, tone: 1500, sym: 46, nivel: 5.5 },                // Dusty Drive
  fuzz:      { drive: 59, tone: 2300, nivel: -11.9, mix: 100 },             // Pedalboard do Logic
  sub:       { mix: 50, nivel: 0 },                                        // PShft -12 semitons
  chorus:    { rate: 0.33, depth: 45, mix: 64 },                           // Pad Enchancer
  gate:      { thresh: -65, release: 27 },
};

// ---- Os oito patches, na ordem do concerto ---------------------------------
// "cadeia" lista os blocos ligados, na ordem exata lida do arquivo.

const PRESETS = [
  {
    id: 'colorindo',
    nome: 'COLORINDO',
    sub: 'clean',
    cor: '#2f7f9e',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp'],
    eq: EQ_BASE,
  },
  {
    id: 'caixa-suja',
    nome: 'CAIXA SUJA',
    sub: 'drive 1',
    cor: '#b87333',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'drive'],
    eq: EQ_BASE,
  },
  {
    id: 'rato',
    nome: 'RATO DO PORÃO',
    sub: 'drive 2',
    cor: '#8c2f2f',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'fuzz', 'eq2'],
    eq: EQ_BASE,
    eq2: EQ_DRIVE2,
  },
  {
    id: 'octavio',
    nome: 'OCTAVIO SUB',
    sub: 'oitava abaixo',
    cor: '#6a4fa3',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'sub', 'eq2'],
    eq: EQ_BASE,
    eq2: EQ_SUB,
  },
  {
    id: 'macro',
    nome: 'MACRO SYNTH',
    sub: 'oitava + fuzz',
    cor: '#3f7d4f',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'gate', 'sub', 'fuzz'],
    eq: EQ_BASE,
    ajustes: { sub: { mix: 49, nivel: 0 } },
  },
  {
    id: 'chora',
    nome: 'CHORA',
    sub: 'drive + chorus',
    cor: '#2f6f6f',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'drive', 'chorus'],
    eq: EQ_BASE,
  },
  {
    id: 'tudo',
    nome: 'TUDO JUNTO',
    sub: 'a cadeia inteira',
    cor: '#9e7b2f',
    cadeia: ['consoleEq', 'comp', 'eq', 'amp', 'drive', 'fuzz', 'eq2', 'gate', 'sub', 'chorus'],
    eq: EQ_BASE,
    eq2: EQ_TUDO,
    ajustes: { sub: { mix: 49, nivel: 0 } },
  },
  {
    id: 'moog',
    nome: 'SYNTH MOOG',
    sub: 'só console + comp',
    cor: '#4a4a52',
    cadeia: ['consoleEq', 'comp'],
    eq: EQ_BASE,
    ajustes: { consoleEq: { low: 1.5, lowMid: 0, highMid: 5.0, high: 1.0, hp: 40 } },
  },
];

// ---- Descrição dos pedais para a interface ---------------------------------

const PEDAIS = {
  consoleEq: {
    nome: 'CONSOLE', sub: 'vintage EQ', cor: '#45505c',
    knobs: [
      { id: 'low',     rot: 'LOW',  min: -12, max: 12, passo: 0.1, un: 'dB' },
      { id: 'lowMid',  rot: 'L-MID', min: -12, max: 12, passo: 0.1, un: 'dB' },
      { id: 'highMid', rot: 'H-MID', min: -12, max: 12, passo: 0.1, un: 'dB' },
      { id: 'high',    rot: 'HIGH', min: -12, max: 12, passo: 0.1, un: 'dB' },
    ],
  },
  comp: {
    nome: 'BLOOM', sub: 'compressor', cor: '#3a5a7a',
    knobs: [
      { id: 'thresh',  rot: 'THRESH',  min: -40, max: 0,   passo: 0.5, un: 'dB' },
      { id: 'ratio',   rot: 'RATIO',   min: 1,   max: 20,  passo: 0.1, un: ':1' },
      { id: 'attack',  rot: 'ATTACK',  min: 0.5, max: 100, passo: 0.5, un: 'ms' },
      { id: 'release', rot: 'RELEASE', min: 10,  max: 500, passo: 1,   un: 'ms' },
      { id: 'makeup',  rot: 'MAKEUP',  min: 0,   max: 20,  passo: 0.1, un: 'dB' },
    ],
  },
  amp: {
    nome: 'CORTES 421', sub: 'bass amp', cor: '#2e2e34',
    knobs: [
      { id: 'gain',   rot: 'GAIN',   min: 0, max: 10, passo: 0.1, un: '' },
      { id: 'bass',   rot: 'BASS',   min: 0, max: 10, passo: 0.1, un: '' },
      { id: 'mid',    rot: 'MID',    min: 0, max: 10, passo: 0.1, un: '' },
      { id: 'treble', rot: 'TREBLE', min: 0, max: 10, passo: 0.1, un: '' },
      { id: 'master', rot: 'MASTER', min: 0, max: 10, passo: 0.1, un: '' },
    ],
  },
  drive: {
    nome: 'CAIXA SUJA', sub: 'dusty drive', cor: '#8a5a24',
    knobs: [
      { id: 'drive', rot: 'DRIVE', min: 0,   max: 60,    passo: 0.5, un: 'dB' },
      { id: 'tone',  rot: 'TONE',  min: 300, max: 6000,  passo: 10,  un: 'Hz' },
      { id: 'sym',   rot: 'SYM',   min: 0,   max: 100,   passo: 1,   un: '%' },
      { id: 'nivel', rot: 'LEVEL', min: -20, max: 12,    passo: 0.1, un: 'dB' },
    ],
  },
  fuzz: {
    nome: 'RATO DO PORÃO', sub: 'fuzz', cor: '#7a2626',
    knobs: [
      { id: 'drive', rot: 'DRIVE', min: 0,   max: 100,  passo: 1,   un: '' },
      { id: 'tone',  rot: 'TONE',  min: 300, max: 6000, passo: 10,  un: 'Hz' },
      { id: 'mix',   rot: 'MIX',   min: 0,   max: 100,  passo: 1,   un: '%' },
      { id: 'nivel', rot: 'LEVEL', min: -24, max: 6,    passo: 0.1, un: 'dB' },
    ],
  },
  sub: {
    nome: 'OCTAVIO', sub: 'oitava abaixo', cor: '#5a4590',
    knobs: [
      { id: 'mix',   rot: 'MIX',   min: 0,   max: 100, passo: 1,   un: '%' },
      { id: 'nivel', rot: 'LEVEL', min: -12, max: 12,  passo: 0.1, un: 'dB' },
    ],
  },
  chorus: {
    nome: 'CHORA', sub: 'chorus', cor: '#26645f',
    knobs: [
      { id: 'rate',  rot: 'RATE',  min: 0.05, max: 4,  passo: 0.01, un: 'Hz' },
      { id: 'depth', rot: 'DEPTH', min: 0,    max: 100, passo: 1,   un: '%' },
      { id: 'mix',   rot: 'MIX',   min: 0,    max: 100, passo: 1,   un: '%' },
    ],
  },
  gate: {
    nome: 'GATE', sub: 'noise gate', cor: '#3c4a3c',
    knobs: [
      { id: 'thresh',  rot: 'THRESH',  min: -90, max: -20, passo: 1, un: 'dB' },
      { id: 'release', rot: 'RELEASE', min: 5,   max: 300, passo: 1, un: 'ms' },
    ],
  },
  eq: {
    nome: 'EQ', sub: 'channel EQ', cor: '#4a4550',
    knobs: [
      { id: 'b165',  rot: '165 Hz',  min: -12, max: 12, passo: 0.1, un: 'dB' },
      { id: 'b1030', rot: '1 kHz',   min: -12, max: 12, passo: 0.1, un: 'dB' },
      { id: 'b1920', rot: '1.9 kHz', min: -12, max: 12, passo: 0.1, un: 'dB' },
    ],
  },
  eq2: {
    nome: 'EQ PÓS', sub: 'channel EQ', cor: '#4a4550',
    knobs: [
      { id: 'hp',    rot: 'HI-PASS', min: 20,  max: 300, passo: 1,   un: 'Hz' },
      { id: 'bLow',  rot: 'GRAVE',   min: -12, max: 12,  passo: 0.1, un: 'dB' },
      { id: 'bMid',  rot: 'MÉDIO',   min: -12, max: 12,  passo: 0.1, un: 'dB' },
    ],
  },
};

const ORDEM_PEDAIS = ['consoleEq', 'comp', 'eq', 'amp', 'drive', 'fuzz', 'eq2', 'gate', 'sub', 'chorus'];

if (typeof module !== 'undefined') module.exports = { PRESETS, PEDAIS, PADROES, ORDEM_PEDAIS, EQ_BASE, EQ_SUB, EQ_DRIVE2, EQ_TUDO };
