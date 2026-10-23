# AURA Bass — overdrive de baixo (JUCE)

Plugin de overdrive para baixo com Drive, Blend paralelo, Low, Mid, High, Presence, Level, simulação de caixa e footswitches para ligar e desligar o DRIVE e o CAB.

## Controles

| Controle | O que faz |
|---|---|
| **DRIVE** (knob) | Quantidade de saturação (0 a 10 → até +40 dB antes do clipper) |
| **BLEND** | Mistura do sinal limpo com o saturado |
| **LOW / MID / HIGH** | Shelf 80 Hz / pico 600 Hz / shelf 2,5 kHz, ±12 dB |
| **PRESENCE** | Pico em 5 kHz, 0 a +12 dB |
| **LEVEL** | Volume de saída, −24 a +12 dB |
| **Footswitch DRIVE** | Liga/desliga só a distorção. Desligado, o som passa limpo pelo EQ, Cab e Level. Troca sem estalo e pode ser automatizado ou mapeado para um pedal MIDI na DAW |
| **Footswitch CAB** | Liga/desliga a simulação de caixa |

O valor de cada knob aparece no rótulo enquanto você gira. Duplo clique volta o knob ao valor padrão.
Gera **VST3** e **Standalone** no Windows e no Mac, e **AU** no Mac. AAX: veja abaixo.

## Fluxo do sinal

```
entrada → HPF 25 Hz → pré-ênfase (+6 dB @ 800 Hz)
        → [oversampling 4x] → Drive (0 a +40 dB) → soft clip assimétrico (tanh) → [downsample]
        → remove DC + LPF 7 kHz (anti-fizz)
        → BLEND (sinal limpo, atrasado para alinhar, × saturado; footswitch DRIVE desligado = só o limpo)
        → LOW (shelf 80 Hz) → MID (pico 600 Hz) → HIGH (shelf 2,5 kHz) → PRESENCE (pico 5 kHz)
        → CAB (HPF 45 Hz, +3 dB @ 110 Hz, −3 dB @ 1,8 kHz, LPF 4,5 kHz 24 dB/oct)
        → LEVEL
```

Todos os parâmetros têm suavização (sem cliques), o Blend é alinhado com a latência do oversampling (sem phasing), e o plugin aceita mono e estéreo.

## MacBook: jeito mais fácil

1. Descompacte o zip (por exemplo, na pasta Downloads).
2. Clique com o botão direito em **`Compilar no Mac.command`**, escolha **Abrir** e confirme. Só na primeira vez o macOS pede essa confirmação.
3. Se o Mac pedir para instalar as "Ferramentas de Linha de Comando", aceite e espere terminar. Depois abra o arquivo de novo.
4. Espere a compilação. A primeira leva de 5 a 15 minutos.

O script instala tudo sozinho:
- VST3 em `~/Library/Audio/Plug-Ins/VST3`
- AU em `~/Library/Audio/Plug-Ins/Components`
- o app Standalone em `~/Applications`

O binário funciona em Mac com chip Apple (M1–M4) e com Intel.

Se o macOS disser que o arquivo "não pode ser aberto", vá em **Ajustes do Sistema > Privacidade e Segurança** e clique em **Abrir Mesmo Assim**. Outra opção: no Terminal, rode `bash ` (com espaço), arraste o arquivo para a janela e tecle Enter.

## Compilar

Pré-requisitos: CMake 3.22+ e um compilador C++17. No Windows, Visual Studio 2022 com "Desenvolvimento para desktop com C++". No Mac, Xcode.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

O CMake baixa o JUCE 8.0.4 sozinho na primeira vez. Para usar uma cópia local: `-DJUCE_DIR=/caminho/JUCE`.

Os plugins ficam em `build/AuraBass_artefacts/Release/`:

| Formato | Para onde copiar |
|---|---|
| `VST3/AURA Bass.vst3` (Windows) | `C:\Program Files\Common Files\VST3\` |
| `VST3/AURA Bass.vst3` (Mac) | `~/Library/Audio/Plug-Ins/VST3/` |
| `AU/AURA Bass.component` (Mac) | `~/Library/Audio/Plug-Ins/Components/` |
| `Standalone/` | roda direto, sem DAW |

**Sem compilador?** O projeto inclui `.github/workflows/build.yml`. Suba a pasta para um repositório no GitHub e a aba *Actions* compila Windows e Mac. Os binários aparecem como *artifacts* para baixar.

No Mac, plugins não assinados podem ser bloqueados. Para uso pessoal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"AURA Bass.component"`. Para distribuir, assine e notarize com uma conta Apple Developer.

### AAX (Pro Tools)

Exige o AAX SDK (grátis em developer.avid.com) e, para rodar no Pro Tools comum, assinatura PACE/iLok. Em `CMakeLists.txt`, chame `juce_set_aax_sdk_path(...)` e adicione `AAX` em `AURA_FORMATS`.

## Interface fotorrealista

O painel não é desenhado por código. Ele é **renderizado** com iluminação física pelo programa em `Tools/AuraRender`, um pequeno renderizador escrito para este projeto. Ele usa:
- microfacetas GGX anisotrópicas para o alumínio escovado e torneado e para o cromado;
- um ambiente de estúdio com softboxes;
- uma luz principal com sombras suaves;
- uma câmera levemente inclinada, que mostra as laterais serrilhadas dos knobs.

As imagens ficam em `Resources/ui/` e vão embutidas no plugin:

| Arquivo | Conteúdo |
|---|---|
| `background.jpg` | painel completo em 2x (1960×940) |
| `knob_0/1/2.jpg` + `_mask.png` | 81 quadros por tamanho de knob (grande, médio, pequeno) |
| `switch_N_down.jpg` | footswitch pressionado |
| `led_N_on.jpg` | LED aceso, com o brilho refletido no metal |

A posição de cada elemento vem de `Source/Layout.h`, compartilhado entre o renderizador e a interface. Para mudar o visual (layout, textos, luz ou materiais), edite esses arquivos e gere as imagens de novo:

```bash
cmake -B build -DAURA_BUILD_RENDERER=ON
cmake --build build --target AuraRender --config Release
./build/AuraRender_artefacts/Release/AuraRender Resources/ui 40   # 40 amostras/pixel, ~15 min
```

## Teste do DSP

```bash
cmake -B build -DAURA_BUILD_TESTS=ON
cmake --build build --config Release
./build/AuraDSPTest_artefacts/Release/AuraDSPTest
```

O teste mede nível e THD em vários valores de Drive. Também verifica NaN/Inf, o alinhamento do Blend, o footswitch DRIVE (sinal limpo, EQ ativo, troca sem estalo), o corte de agudos do Cab e o modo mono. Com `AURA_SNAPSHOT=ui.png`, ele salva uma imagem da interface.

## Aproximando o som da referência

As constantes de som ficam no topo de `Source/PluginProcessor.cpp`. Com o Plugin Doctor (ou outro analisador), meça o plugin de referência e o AURA Bass com os mesmos ajustes e compare:

1. **Resposta de frequência com Drive no mínimo e EQ no meio:** ajuste a pré-ênfase, o `postLpf` e o filtro do cab até as curvas coincidirem.
2. **Cada knob de EQ no máximo e no mínimo:** ajuste `lowFreq`, `midFreq`, `midQ`, `highFreq`, `presenceFreq` e as faixas em dB.
3. **Harmônicos (modo "Harmonic") em 3 posições de Drive:** muito H3 indica clipagem simétrica e muito H2, assimétrica. Ajuste `clipBias` e `maxDriveDb`. Para um caráter mais "duro", troque o `tanh` em `shape()` por um clip mais agressivo, ou coloque dois estágios em série.
4. **Nível de saída por posição de Drive:** ajuste `wetTrim` e a compensação `makeup`.
5. **Cab:** para mais realismo, troque os filtros por uma resposta ao impulso (`juce::dsp::Convolution`) de uma caixa de baixo.

Use nome, visual e presets próprios. Medir e se inspirar no som é legítimo. Copiar marca, interface ou código de outro produto não é.
