# De onde saiu este pedalboard

Tudo aqui foi lido do arquivo **"PRESET LOGIC : MAIN STAGE CORTES - FINAL"**:
oito patches do MainStage (`.patch`), o concerto (`MAINSTAGE CORTES.concert`) e o
projeto do Logic (`PRESET CORTES PEDALBOARD.logicx`).

Cada patch guarda a cadeia num arquivo `#Root.cst`. Dele saíram **a ordem exata
dos plugins** e **os valores dos parâmetros** (gravados como números de 32 bits).

## As oito cadeias, como estão no arquivo

| Preset | Cadeia |
|---|---|
| **COLORINDO** (clean) | Console EQ → Compressor → Channel EQ → Bass Amp |
| **CAIXA SUJA** (drive 1) | … → Bass Amp → **Clip Distortion** |
| **RATO DO PORÃO** (drive 2) | … → Bass Amp → **Pedalboard** (3 blocos) → Channel EQ |
| **OCTAVIO SUB** | … → Bass Amp → **Pitch Shifter** → Channel EQ |
| **MACRO SYNTH** | … → Bass Amp → **Noise Gate** → **Pitch Shifter** → **Pedalboard** |
| **CHORA** | … → Bass Amp → **Clip Distortion** → **Chorus** |
| **TUDO JUNTO** | … → Bass Amp → Clip Dist → Pedalboard → EQ → Gate → Pitch → Pedalboard → Chorus → Pitch → EQ |
| **SYNTH MOOG** | Console EQ (*Synth Bass*) → Compressor |

Os presets de fábrica usados: `CONSOLE VINTAGE`, `COMPRESSOR LEGAL BLOOM`,
`AMPEG CORTES 421`, `Dusty Drive`, `Pad Enchancer`, `Synth Bass`.

## Os números lidos do arquivo

**Channel EQ** — o Logic guarda cada banda como (ligada, frequência, ganho, Q):

| Banda | Base (todos os patches) | Pós-oitava (OCTAVIO) | Pós-drive (RATO) | TUDO JUNTO |
|---|---|---|---|---|
| Passa-altas | desligado | desligado | **73 Hz** | **127 Hz** |
| Shelf grave | 75 Hz, 0 dB | 75 Hz, 0 dB | 75 Hz, 0 dB | 75 Hz, 0 dB |
| Pico 1 | 165 Hz, **+2,5 dB**, Q 0,6 | 172 Hz, **+5,7 dB**, Q 0,6 | 100 Hz, 0 dB | 100 Hz, 0 dB |
| Pico 2 | 250 Hz, 0 dB | 250 Hz, 0 dB | 250 Hz, 0 dB | 250 Hz, 0 dB |
| Pico 3 | 1030 Hz, **−3,0 dB** | 805 Hz, **−12,0 dB** | 1040 Hz, 0 dB | 1040 Hz, 0 dB |
| Pico 4 | 1920 Hz, **+3,3 dB** | 2500 Hz, 0 dB | 735 Hz, +1,6 dB | 735 Hz, +1,6 dB |

Dá para ver a lógica do rig: o EQ base tira um pouco do médio em 1 kHz e abre
1,9 kHz (ataque da palheta/dedo); o pós-oitava empurra 172 Hz e cava 805 Hz para
o sub ficar redondo; os pós-drive cortam grave (73 e 127 Hz) para a distorção não
embolar.

**Compressor (LEGAL BLOOM)** — threshold −22,5 dB, ratio 6,8:1, attack 10 ms,
release 48 ms, make-up 8 dB.

**Pitch Shifter** — **−12 semitons**, mix 50% (49% em dois dos slots). É a oitava
abaixo do OCTAVIO SUB e do MACRO SYNTH.

**Clip Distortion (Dusty Drive)** — drive 37 dB, tone 1500 Hz, simetria 46%,
saída +5,5 dB.

**Chorus (Pad Enchancer)** — 0,33 Hz, profundidade 45%, mistura 64%.

**Noise Gate** — threshold −65 dB, release 27 ms.

**Bass Amp Designer (AMPEG CORTES 421)** — os números estão no arquivo
(13,4 / 2,8 / 7,5 / 7,4 / 6,6 / 6,2 / 1,8 …), mas o Logic não guarda o nome de
cada posição, então a correspondência knob a knob é interpretação. Os valores
foram mantidos nos knobs equivalentes — gain, grave, médio, agudo e master.

## O que foi recriado, e como

O Logic não roda no navegador, então cada plugin virou um bloco equivalente:

| Plugin do Logic | Aqui |
|---|---|
| Console EQ | quatro bandas + passa-altas |
| Compressor | compressor do Web Audio com os mesmos tempos |
| Channel EQ | passa-altas + cinco bandas, com as frequências e ganhos da tabela |
| Bass Amp Designer | pré-amp com saturação assimétrica (4× oversampling), tone stack e a resposta de impulso de caixa 4×10 do AURA Bass |
| Clip Distortion | corte duro com ponto deslocado pela simetria, tone e nível |
| Pedalboard (fuzz) | fuzz com corte áspero, tone e mistura |
| Pitch Shifter −12 | divisor de oitava analógico (comparador com histerese + envelope), como nos pedais de sub |
| Chorus | atraso modulado |
| Noise Gate | gate com ataque rápido e release ajustável |

A oitava foi feita como divisor, e não como pitch shifter por FFT, de propósito:
num baixo é o que soa certo, responde no mesmo instante e não "borra" a nota.

Na saída há um limitador e um teto que não deixa nenhum pico passar de 0 dBFS,
por mais drive que se coloque.

## O que não dá para garantir

- A correspondência knob a knob do **Bass Amp Designer** e do **Console EQ** é
  interpretação, não leitura direta.
- Os três blocos internos do **Pedalboard** do Logic (identificados no arquivo só
  por códigos) viraram um fuzz único.
- O som não é idêntico ao do Logic: os algoritmos da Apple são fechados. O que foi
  reproduzido é a **estrutura e o ajuste** do rig — ordem, cortes, ganhos e tempos.

Ajuste de ouvido o que faltar: os valores ficam salvos no aparelho e o menu tem
"Voltar o preset aos valores originais" para recomeçar do que estava no MainStage.
