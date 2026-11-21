// AURA Preamp Bass — medidas do painel (versão web)
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
// AURA Preamp Bass — geometria do painel (espelha Source/Layout.h).
// Tests/web-dsp-test.js compara estes valores com o cabeçalho C++, para não divergirem.
(function (root) {
  const L = {
    width: 726,
    height: 443,

    // A faixa de efeitos foi inserida no meio da placa, logo acima dos
    // footswitches: tudo o que estava abaixo desceu 104 unidades.
    faixaEfeitosY: 265.0,
    faixaEfeitosH: 104.0,

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
    driveSwitchY: 402.0,
    driveSwitchDownHalf: 24.0,
    driveLedX: 616.0,
    driveLedY: 402.0,
    driveLedHalf: 11.0,
    driveHit: { x: 554.0, y: 378.0, w: 126.0, h: 48.0 },

    tunerSwitchX: 70.5,
    tunerSwitchY: 402.0,
    tunerLedX: 110.0,
    tunerLedY: 402.0,
    tunerHit: { x: 46.0, y: 378.0, w: 126.0, h: 48.0 },
    tunerDisplay: { x: 30.0, y: 98.0, w: 666.0, h: 156.0 },

    menuButton: { x: 662.0, y: 40.0, w: 26.0, h: 22.0 },

    cabLedX: 671.35,
    cabLedY: 138.65,
    cabToggleX: 671.5,
    cabToggleY: 175.0,
    cabHit: { x: 648.0, y: 108.0, w: 48.0, h: 94.0 },

    efeitos: [
      { paramId: 'octave', cx: 146.0, cy: 318.0, valueX: 146.0 },
      { paramId: 'chorus', cx: 274.0, cy: 318.0, valueX: 274.0 },
    ],
    efeitoRaio: 28.0,
    efeitoHitRaio: 30.0,
    efeitoRotuloY: 276.0,
    efeitoValorY: 360.0,
    efeitoArcoRaio: 36.0,

    meter: { x: 360.0, y: 310.0, w: 250.0, h: 15.0 },
    meterHit: { x: 352.0, y: 298.0, w: 290.0, h: 42.0 },
    meterMinDb: -42.0,
    meterClipX: 626.0,
    meterLabelY: 296.0,

    ledHalf: 16.0,
    toggleHalf: 25.0,
  };

  // Faixas dos parâmetros (iguais às de createParameterLayout no plugin)
  L.params = {
    trim:     { min: -24, max: 24, step: 0.1, def: 0,   unit: 'dB', label: 'TRIM' },
    octave:   { min: 0,   max: 100, step: 1,  def: 0,   unit: '%',  label: 'OCTAVE' },
    chorus:   { min: 0,   max: 100, step: 1,  def: 0,   unit: '%',  label: 'CHORUS' },
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
    { name: 'AURA (padrão)',   level: 0, drive: 0,  low: 0,  mid: 0,     high: 0,    presence: 3,    blend: 100, octave: 0,   chorus: 0,  cab: true, driveOn: true },
    { name: 'Clean',           level: 0, drive: 0,  low: 2,  mid: 0,     high: 1,    presence: 2,    blend: 100, octave: 0,   chorus: 0,  cab: true, driveOn: false },

    // Medidos a partir do vídeo de referência (ver Source/PluginProcessor.cpp)
    { name: 'Drive',           level: 0, drive: 0,  low: 1,  mid: 1.5,   high: 1.5,  presence: 0,    blend: 90,  octave: 0,   chorus: 0,  cab: true, driveOn: true },
    { name: 'Drive (Stage 2)', level: 0, drive: 5,  low: -1, mid: 3,     high: -4.5, presence: 0,    blend: 100, octave: 0,   chorus: 0,  cab: true, driveOn: true },
    { name: 'Octave (Clean)',  level: 0, drive: -2, low: 1,  mid: 0.5,   high: -1,   presence: 0,    blend: 100, octave: 70,  chorus: 0,  cab: true, driveOn: true },
    { name: 'Octave (Wild)',   level: 0, drive: 15, low: -7, mid: -4.5,  high: -1,   presence: -3,   blend: 100, octave: 100, chorus: 0,  cab: true, driveOn: true },
    { name: 'Sub Bass',        level: 0, drive: -8, low: -1, mid: -11.5, high: -3,   presence: -2.5, blend: 100, octave: 100, chorus: 0,  cab: true, driveOn: true },
    { name: 'Chorus',          level: 0, drive: 2,  low: 0,  mid: 0,     high: -2.5, presence: 0,    blend: 100, octave: 0,   chorus: 65, cab: true, driveOn: true },
  ];

  L.formatValue = function (id, v) {
    if (id === 'blend' || id === 'octave' || id === 'chorus') return Math.round(v) + '%';
    if (Math.abs(v) < 0.05) v = 0;
    return v.toFixed(1) + ' dB';
  };

  if (typeof module !== 'undefined') module.exports = L; else root.AuraLayout = L;
})(typeof self !== 'undefined' ? self : this);
