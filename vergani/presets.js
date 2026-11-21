// VERGANI PEDALBOARD — presets e descrição dos pedais.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
//
// Os presets são pontos de partida criados para este app (não vêm de nenhum
// arquivo de terceiros). Ajuste de ouvido, com a guitarra que você usa.

'use strict';

// ---- Ajustes de fábrica de cada pedal --------------------------------------

const PADROES = {
  tuner:      { ref: 440 },
  overdrive:  { drive: 35, tone: 3500, nivel: 0 },
  distortion: { drive: 60, tone: 3000, nivel: -4 },
  detune:     { cents: 14, mix: 50 },
  chorus:     { rate: 0.8, depth: 45, mix: 50 },
  delay:      { time: 380, feedback: 35, tone: 3500, mix: 30 },
};

// ---- Os oito presets -------------------------------------------------------
// "cadeia" lista os pedais ligados; "ajustes" troca só os knobs que mudam.

const PRESETS = [
  {
    id: 'limpo',
    nome: 'LIMPO',
    sub: 'chorus + delay',
    cor: '#2f7f9e',
    cadeia: ['chorus', 'delay'],
    ajustes: {
      chorus: { rate: 0.6, depth: 40, mix: 45 },
      delay:  { time: 420, feedback: 25, tone: 3200, mix: 20 },
    },
  },
  {
    id: 'crunch',
    nome: 'CRUNCH',
    sub: 'overdrive leve',
    cor: '#b87333',
    cadeia: ['overdrive'],
    ajustes: { overdrive: { drive: 28, tone: 4000, nivel: 0 } },
  },
  {
    id: 'blues',
    nome: 'LEAD BLUES',
    sub: 'overdrive + delay',
    cor: '#9e7b2f',
    cadeia: ['overdrive', 'delay'],
    ajustes: {
      overdrive: { drive: 55, tone: 3200, nivel: 2 },
      delay:     { time: 340, feedback: 30, tone: 3000, mix: 25 },
    },
  },
  {
    id: 'rock',
    nome: 'ROCK',
    sub: 'distorção',
    cor: '#8c2f2f',
    cadeia: ['distortion'],
    ajustes: { distortion: { drive: 55, tone: 3400, nivel: -4 } },
  },
  {
    id: 'pesado',
    nome: 'PESADO',
    sub: 'overdrive + distorção',
    cor: '#5a2a2a',
    cadeia: ['overdrive', 'distortion'],
    ajustes: {
      overdrive:  { drive: 20, tone: 3800, nivel: 3 },
      distortion: { drive: 82, tone: 2600, nivel: -8 },
    },
  },
  {
    id: 'shoegaze',
    nome: 'SHOEGAZE',
    sub: 'dist + detune + chorus + delay',
    cor: '#6a4fa3',
    cadeia: ['distortion', 'detune', 'chorus', 'delay'],
    ajustes: {
      distortion: { drive: 45, tone: 2400, nivel: -6 },
      detune:     { cents: 18, mix: 45 },
      chorus:     { rate: 0.5, depth: 60, mix: 55 },
      delay:      { time: 600, feedback: 55, tone: 2600, mix: 40 },
    },
  },
  {
    id: 'ambiente',
    nome: 'AMBIENTE',
    sub: 'limpo, detune + delay longo',
    cor: '#3f7d4f',
    cadeia: ['detune', 'chorus', 'delay'],
    ajustes: {
      detune: { cents: 10, mix: 50 },
      chorus: { rate: 0.4, depth: 60, mix: 60 },
      delay:  { time: 520, feedback: 60, tone: 2800, mix: 45 },
    },
  },
  {
    id: 'tudo',
    nome: 'TUDO JUNTO',
    sub: 'os cinco pedais',
    cor: '#2f6f6f',
    cadeia: ['overdrive', 'distortion', 'detune', 'chorus', 'delay'],
    ajustes: {
      overdrive:  { drive: 30, tone: 3500, nivel: 2 },
      distortion: { drive: 50, tone: 2800, nivel: -8 },
      detune:     { cents: 12, mix: 40 },
      chorus:     { rate: 0.7, depth: 45, mix: 45 },
      delay:      { time: 400, feedback: 35, tone: 3000, mix: 28 },
    },
  },
];

// ---- Descrição dos pedais para a interface ---------------------------------

const PEDAIS = {
  tuner: {
    nome: 'AFINADOR', sub: 'liga = saída muda', cor: '#2c6b46', luz: '#7bf0a0',
    knobs: [
      { id: 'ref', rot: 'LÁ =', min: 430, max: 450, passo: 1, un: 'Hz' },
    ],
  },
  overdrive: {
    nome: 'OVERDRIVE', sub: 'saturação macia', cor: '#8a6a24', luz: '#f0c45a',
    knobs: [
      { id: 'drive', rot: 'DRIVE', min: 0,   max: 100,  passo: 1,   un: '' },
      { id: 'tone',  rot: 'TONE',  min: 800, max: 8000, passo: 10,  un: 'Hz' },
      { id: 'nivel', rot: 'LEVEL', min: -20, max: 12,   passo: 0.1, un: 'dB' },
    ],
  },
  distortion: {
    nome: 'DISTORTION', sub: 'corte duro', cor: '#7a2626', luz: '#ff7a6a',
    knobs: [
      { id: 'drive', rot: 'DRIVE', min: 0,   max: 100,  passo: 1,   un: '' },
      { id: 'tone',  rot: 'TONE',  min: 800, max: 8000, passo: 10,  un: 'Hz' },
      { id: 'nivel', rot: 'LEVEL', min: -24, max: 6,    passo: 0.1, un: 'dB' },
    ],
  },
  detune: {
    nome: 'DETUNE', sub: 'afinação em cents', cor: '#5a4590', luz: '#b59cff',
    knobs: [
      { id: 'cents', rot: 'CENTS', min: -50, max: 50,  passo: 1, un: 'ct' },
      { id: 'mix',   rot: 'MIX',   min: 0,   max: 100, passo: 1, un: '%' },
    ],
  },
  chorus: {
    nome: 'CHORUS', sub: 'duas vozes', cor: '#26645f', luz: '#4fe0cc',
    knobs: [
      { id: 'rate',  rot: 'RATE',  min: 0.05, max: 6,   passo: 0.01, un: 'Hz' },
      { id: 'depth', rot: 'DEPTH', min: 0,    max: 100, passo: 1,    un: '%' },
      { id: 'mix',   rot: 'MIX',   min: 0,    max: 100, passo: 1,    un: '%' },
    ],
  },
  delay: {
    nome: 'DELAY', sub: 'repetições escuras', cor: '#2f5a7a', luz: '#62b8f5',
    knobs: [
      { id: 'time',     rot: 'TIME',     min: 40,  max: 1000, passo: 5,  un: 'ms' },
      { id: 'feedback', rot: 'FEEDBACK', min: 0,   max: 90,   passo: 1,  un: '%' },
      { id: 'tone',     rot: 'TONE',     min: 800, max: 8000, passo: 10, un: 'Hz' },
      { id: 'mix',      rot: 'MIX',      min: 0,   max: 100,  passo: 1,  un: '%' },
    ],
  },
};

const ORDEM_PEDAIS = ['tuner', 'overdrive', 'distortion', 'detune', 'chorus', 'delay'];

if (typeof module !== 'undefined') module.exports = { PRESETS, PEDAIS, PADROES, ORDEM_PEDAIS };
