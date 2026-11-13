// AURA Bass — medidas do painel (versão web)
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
// AURA Bass — geometria do painel (espelha Source/Layout.h).
// Tests/web-dsp-test.js compara estes valores com o cabeçalho C++, para não divergirem.
(function (root) {
  const L = {
    width: 726,
    height: 339,

    knobs: [
      { paramId: 'level',    cx: 71.5,   cy: 172.5,  valueX: 74.5 },
      { paramId: 'drive',    cx: 159.25, cy: 174.5,  valueX: 162.0 },
      { paramId: 'low',      cx: 247.75, cy: 172.5,  valueX: 249.0 },
      { paramId: 'mid',      cx: 336.0,  cy: 172.5,  valueX: 336.5 },
      { paramId: 'high',     cx: 424.5,  cy: 172.5,  valueX: 423.5 },
      { paramId: 'presence', cx: 513.5,  cy: 172.75, valueX: 510.5 },
      { paramId: 'blend',    cx: 601.0,  cy: 172.25, valueX: 598.0 },
    ],
    knobHitRadius: 34.0,
    knobMinAngleDeg: -135.0,
    knobMaxAngleDeg: 135.0,
    pointerHalf: 24.0,
    valueY: 232.0,
    valueFontSize: 14.0,

    arcCentreDy: 1.5,
    arcLitRadius: 46.0,
    arcMaxDeg: 142.0,

    driveSwitchX: 655.5,
    driveSwitchY: 298.0,
    driveSwitchDownHalf: 24.0,
    driveLedX: 616.0,
    driveLedY: 298.0,
    driveLedHalf: 11.0,
    driveHit: { x: 554.0, y: 274.0, w: 126.0, h: 48.0 },

    tunerSwitchX: 70.5,
    tunerSwitchY: 298.0,
    tunerLedX: 110.0,
    tunerLedY: 298.0,
    tunerHit: { x: 46.0, y: 274.0, w: 126.0, h: 48.0 },
    tunerDisplay: { x: 30.0, y: 98.0, w: 666.0, h: 156.0 },

    menuButton: { x: 662.0, y: 40.0, w: 26.0, h: 22.0 },

    cabLedX: 671.35,
    cabLedY: 138.65,
    cabToggleX: 671.5,
    cabToggleY: 175.0,
    cabHit: { x: 648.0, y: 108.0, w: 48.0, h: 94.0 },

    meter: { x: 196.0, y: 288.0, w: 210.0, h: 15.0 },
    meterHit: { x: 190.0, y: 276.0, w: 244.0, h: 40.0 },
    meterMinDb: -42.0,
    meterClipX: 420.0,
    meterLabelY: 274.0,

    ledHalf: 16.0,
    toggleHalf: 25.0,
  };

  // Faixas dos parâmetros (iguais às de createParameterLayout no plugin)
  L.params = {
    trim:     { min: -24, max: 24, step: 0.1, def: 0,   unit: 'dB', label: 'TRIM' },
    level:    { min: -15, max: 15, step: 0.1, def: 0,   unit: 'dB', label: 'LEVEL' },
    drive:    { min: -15, max: 15, step: 0.1, def: 0,   unit: 'dB', label: 'DRIVE' },
    low:      { min: -12, max: 12, step: 0.1, def: 0,   unit: 'dB', label: 'LOW' },
    mid:      { min: -12, max: 12, step: 0.1, def: 0,   unit: 'dB', label: 'MID' },
    high:     { min: -12, max: 12, step: 0.1, def: 0,   unit: 'dB', label: 'HIGH' },
    presence: { min: -7,  max: 7,  step: 0.1, def: 3,   unit: 'dB', label: 'PRESENCE' },
    blend:    { min: 0,   max: 100, step: 1,  def: 100, unit: '%',  label: 'BLEND' },
  };

  // Presets de fábrica (iguais aos de Source/PluginProcessor.cpp)
  L.presets = [
    { name: 'AURA (padrão)',  level: 0, drive: 0,   low: 0, mid: 0,  high: 0,  presence: 3, blend: 100, cab: true, driveOn: true },
    { name: 'Clean DI',       level: 0, drive: 0,   low: 2, mid: 0,  high: 1,  presence: 2, blend: 100, cab: true, driveOn: false },
    { name: 'Grit leve',      level: 0, drive: -8,  low: 1, mid: 2,  high: 0,  presence: 3, blend: 60,  cab: true, driveOn: true },
    { name: 'Drive pesado',   level: 0, drive: 10,  low: 3, mid: 4,  high: -1, presence: 4, blend: 85,  cab: true, driveOn: true },
    { name: 'Slap brilhante', level: 0, drive: -12, low: 4, mid: -5, high: 6,  presence: 6, blend: 35,  cab: true, driveOn: true },
  ];

  L.formatValue = function (id, v) {
    if (id === 'blend') return Math.round(v) + '%';
    if (Math.abs(v) < 0.05) v = 0;
    return v.toFixed(1) + ' dB';
  };

  if (typeof module !== 'undefined') module.exports = L; else root.AuraLayout = L;
})(typeof self !== 'undefined' ? self : this);
