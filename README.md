# AURA Bass — overdrive de baixo (JUCE)

> Copyright © 2026 **Thiago Galvão**. Todos os direitos reservados.
> Obra protegida pelas Leis nº 9.609/1998 e nº 9.610/1998 e pela Convenção de
> Berna. Uso pessoal permitido; cópia, redistribuição ou obra derivada apenas
> com autorização por escrito. Veja [`LICENSE`](LICENSE), os componentes de
> terceiros em [`TERCEIROS.md`](TERCEIROS.md) e a prova de autoria em
> [`AUTORIA.md`](AUTORIA.md).

Plugin de overdrive para baixo com Drive, Blend paralelo, Low, Mid, High, Presence, Level, simulação de caixa (chave CAB) e footswitch para ligar e desligar o DRIVE.

## Controles

| Controle | O que faz |
|---|---|
| **LEVEL** | Volume de saída, −15 a +15 dB |
| **DRIVE** (knob) | Ganho antes da saturação, −15 a +15 dB. 0 dB é o ponto nominal (+20 dB no clipper), −15 dB só esquenta, +15 dB satura bastante |
| **LOW / MID / HIGH** | Shelf 80 Hz / pico 600 Hz / shelf 2,5 kHz, −12 a +12 dB |
| **PRESENCE** | Pico em 5 kHz, −7 a +7 dB (padrão +3 dB) |
| **BLEND** | Mistura do sinal limpo com o saturado, 0 a 100% |
| **Footswitch ENGAGE** (LED azul) | Liga/desliga só a distorção, como um pedal de pisar. Desligado, o som passa limpo pelo EQ, Cab e Level. Troca sem estalo e pode ser automatizado ou mapeado para um pedal MIDI na DAW |
| **Chave CAB** (LED laranja) | Liga/desliga a simulação de caixa |

O valor de cada knob aparece embaixo dele, e o arco metálico em volta acende até o valor (a partir do centro nos controles em dB, a partir do início no Blend). Duplo clique volta o knob ao valor padrão: Level, Drive, Low, Mid e High em 0.0 dB, Presence em 3.0 dB e Blend em 100%. Segure **Shift** ao arrastar (ou ao girar a roda do mouse) para ajuste fino.

### Afinador

Pise no footswitch **TUNER** (canto inferior esquerdo, LED verde). O painel dos knobs vira o visor do afinador e a saída fica muda, como num pedal afinador. Pise de novo para voltar a tocar.

- O visor mostra a nota e a oitava, o desvio em cents (medidor de −50 a +50) e a frequência em Hz.
- Âmbar com seta: a seta da esquerda acende quando a nota está baixa, a da direita quando está alta. Verde: afinado (dentro de ±3 cents).
- Faixa: do Si grave do baixo de 5 cordas (30,9 Hz) até cerca de 650 Hz. Referência fixa em Lá = 440 Hz.
- "Tuner" é um parâmetro do plugin: dá para mapear a um pedal MIDI na DAW. Ele nunca volta ligado ao reabrir uma sessão.
- A detecção (`Source/Tuner.h`) usa o método YIN sobre a entrada crua, antes de qualquer efeito. Nos testes, o erro fica abaixo de 0,5 cent, mesmo com harmônicos mais fortes que a fundamental.

### Compensação de volume do DRIVE

O som saturado é compensado automaticamente para ficar no mesmo volume do limpo (calibrado com um sinal de −12 dBFS, nível típico de baixo na entrada). Ligar o footswitch muda o timbre, não o volume. O Level continua ajustando a saída final.

### CAB por resposta de impulso (IR)

O CAB usa convolução. Clique no nome embaixo da chave do CAB (ex.: "4x10 ▾") para escolher:
- **4x10** e **1x15**: IRs embutidas, geradas por `Tools/make_cab_irs.py` a partir de um modelo de caixa de baixo;
- **Carregar IR (.wav / .aiff)...**: qualquer IR de caixa (usa o primeiro canal, até 1 s).

Toda IR é ajustada para 0 dB na faixa de 100 a 800 Hz, então ligar o CAB muda o timbre sem derrubar o volume. A escolha fica salva na sessão.

### Presets de fábrica

No menu de presets da DAW: **AURA (padrão)**, **Clean DI**, **Grit leve**, **Drive pesado** e **Slap brilhante**.

### Menu, tamanho da interface e versão

O ícone de três traços no canto superior direito (ou o clique direito numa parte vazia do painel) abre o menu do plugin: presets de fábrica, tamanho da interface (100%, 125% ou 150%, salvo na sessão), versão instalada e dicas de uso.

### Uso dos controles

- **Digitar valores:** clique no valor embaixo de um knob e digite ("3", "-4,5", "60%"). Enter confirma, Esc cancela.
- **Dicas ao passar o mouse:** cada controle explica o que faz.
- **ENGAGE desligado:** os knobs Drive e Blend ficam esmaecidos, porque não atuam.

Nos knobs e botões, o clique direito abre o menu da DAW para aquele parâmetro (automação, MIDI learn), quando a DAW oferece um. Senão, abre o menu do plugin. Também dá para digitar valores direto na DAW ("3", "-4.5 dB", "60%").

### Detalhes técnicos

- **Versão automática:** cada build do GitHub gera uma versão nova (1.x.y), o que faz o macOS e o Logic reconhecerem o plugin atualizado.
- **Tempo real:** o processamento não aloca memória nem lê arquivos. Blocos maiores que o anunciado pela DAW são processados em pedaços.
- **Bypass da DAW:** atrasa o sinal pela mesma latência do plugin (61 amostras do oversampling), mantendo o alinhamento com as outras faixas.
- **Cauda (tail):** o plugin informa a duração da IR do CAB à DAW, que não corta o fim do som ao parar.
- **Economia de CPU:** com o DRIVE desligado, a saturação não é processada; com o CAB desligado, a convolução não é processada. Ao religar, não há estalo.
- **EQ:** os ganhos são suavizados (sem "zipper" ao girar ou automatizar).
- **Canais:** entrada mono com saída estéreo é suportada (faixa de baixo mono).
- **Filtro de entrada:** fica em 18 Hz, abaixo do Si grave de um baixo de 5 cordas.

## Fluxo do sinal

```
entrada → HPF 18 Hz → pré-ênfase (+6 dB @ 800 Hz)
        → [oversampling 4x] → Drive (+5 a +35 dB; knob de −15 a +15 dB) → soft clip assimétrico (tanh) → [downsample]
        → remove DC + LPF 7 kHz (anti-fizz) → compensação automática de volume
        → BLEND (sinal limpo, atrasado para alinhar, × saturado; footswitch ENGAGE desligado = só o limpo)
        → LOW (shelf 80 Hz) → MID (pico 600 Hz) → HIGH (shelf 2,5 kHz) → PRESENCE (pico 5 kHz)
        → CAB (convolução com IR: 4x10, 1x15 ou arquivo do usuário)
        → LEVEL
```

Todos os parâmetros têm suavização (sem cliques), o Blend é alinhado com a latência do oversampling (sem phasing), e o plugin aceita mono e estéreo.

## No navegador (celular, tablet ou computador)

O AURA Bass também roda direto no navegador, com o mesmo painel e o mesmo
processamento do plugin:

**https://thiagorgalvao-sketch.github.io/AURA/**

Abra o endereço, toque em **Ligar** e autorize o microfone (ou a interface de
áudio). Funciona no Chrome, no Edge e no Safari.

- **Ligue no fone de ouvido.** Sem fone, o som da caixa volta para o microfone
  e microfona.
- **Interface de áudio:** ligue o baixo na interface e escolha-a como entrada do
  navegador. Pelo microfone do celular também funciona, mas com menos graves.
- **Instalar como app:** no Chrome, menu > *Instalar aplicativo*; no iPhone,
  *Compartilhar > Adicionar à Tela de Início*. Depois de instalado, abre sem
  internet e sem a barra do navegador.
- **Celular em pé:** o painel gira sozinho para ocupar a tela inteira.
- **Latência:** depende do aparelho — num computador fica em torno de 20 a 40 ms;
  no celular costuma ser maior. Para tocar ao vivo, o plugin (VST3/AU) continua
  sendo o caminho de menor latência.

Os controles são os mesmos do plugin: arraste o knob para cima ou para a direita
para aumentar, toque duas vezes para voltar ao padrão, toque no número para
digitar um valor, e use os footswitches **ENGAGE** (drive) e **TUNER**. Os
ajustes ficam salvos no aparelho. Atalhos no computador: `E` liga o drive,
`T` o afinador, `C` o CAB.

## CORTES Pedalboard (no navegador)

**https://thiagorgalvao-sketch.github.io/AURA/pedalboard/**

Os oito presets do concerto do MainStage ("PRESET LOGIC : MAIN STAGE CORTES"),
recriados como um pedalboard que roda no navegador: COLORINDO, CAIXA SUJA, RATO
DO PORÃO, OCTAVIO SUB, MACRO SYNTH, CHORA, TUDO JUNTO e SYNTH MOOG.

A ordem dos pedais e os valores vieram dos próprios arquivos `.patch` do
MainStage — o que foi lido e o que foi recriado está em
[`pedalboard/PRESETS.md`](pedalboard/PRESETS.md).

- Toque num preset para montar a cadeia; o footswitch de cada pedal liga e
  desliga na hora; teclas `1` a `8` trocam de preset.
- Arraste o knob para cima ou para baixo; toque duplo volta ao valor original
  do MainStage.
- Os ajustes ficam salvos no aparelho e dá para instalar como app.

```bash
bash Tools/build-web.sh site                 # monta os dois apps
python3 Tests/pedalboard-ui-test.py site     # testa num Chromium de verdade
```

O teste confere a cadeia de cada um dos oito presets, que sai som, que nenhum
preset passa de 0 dBFS, que a oitava abaixo aparece no espectro, os footswitches,
os knobs, o menu, o estado salvo e as telas pequenas.

### Publicar a versão web

O site é montado a partir do próprio projeto (mesmo painel, mesmas IRs, mesmo
`Source/Layout.h`) e publicado pelo GitHub Pages:

```bash
bash Tools/build-web.sh site      # monta a pasta site/ (~1 MB)
node Tests/web-dsp-test.js        # compara o áudio da web com o do plugin
python3 Tests/web-ui-test.py      # abre num Chromium e testa a interface
```

O workflow `.github/workflows/pages.yml` faz isso a cada push em `main` que
mexa em `web/`, nos recursos ou no layout: roda o teste de áudio, monta o site
e publica no branch `gh-pages`.

O Pages já está ligado, servindo o branch `gh-pages`. Se um dia o endereço
parar de funcionar, confira em **Settings > Pages** que *Source* está em
**Deploy from a branch**, branch **`gh-pages`**, pasta **`/ (root)`**.

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

## Interface

A interface usa a arte de referência do painel: placa de metal escuro escovado, moldura cromada, knobs serrilhados com arco, footswitch do DRIVE com LED azul e chave alavanca do CAB com LED laranja. O footswitch é renderizado com iluminação física por `Tools/FootswitchRender` (porca sextavada, arruela e botão de cromo, com sombra na placa) e aplicado sobre a arte. O script `Tools/build_ui_assets.py` monta as imagens em `Resources/ui/` a partir de `Resources/mockup/`, comparando o painel completo com a placa vazia (as duas imagens são alinhadas):

| Arquivo | Conteúdo |
|---|---|
| `background.jpg` | painel em 2x, sem os indicadores dos knobs e sem os valores, LEDs apagados, com o logotipo AURA BASS (Russo One, com desgaste) e o footswitch do DRIVE no canto inferior direito |
| `pointer.png` | indicador do knob, que gira conforme o valor |
| `led_drive_on.png`, `led_cab_on.png` | LEDs acesos |
| `cab_off.png` | chave do CAB na outra posição (alavanca espelhada) |
| `footswitch_down.png` | footswitch do DRIVE pressionado |

Os valores embaixo dos knobs são desenhados ao vivo com a fonte Roboto. As posições ficam em `Source/Layout.h`. Para gerar as imagens de novo depois de mudar a arte ou os textos:

```bash
python3 Tools/build_ui_assets.py
```

Fontes incluídas: Russo One e Roboto, ambas sob a SIL Open Font License (`Resources/fonts`).

## Teste do DSP

```bash
cmake -B build -DAURA_BUILD_TESTS=ON
cmake --build build --config Release
./build/AuraDSPTest_artefacts/Release/AuraDSPTest
```

O teste mede nível e THD em vários valores de Drive. Também verifica:
- NaN/Inf e o alinhamento do Blend;
- o footswitch (sinal limpo, EQ ativo, troca sem estalo) e a compensação de volume;
- as IRs do CAB e os presets;
- o estado salvo, incluindo o preset atual;
- blocos de qualquer tamanho e o bypass alinhado;
- a cauda informada à DAW e silêncio na entrada;
- 44,1, 96 e 192 kHz;
- mono e mono→estéreo;
- religar o DRIVE e o CAB sem estalo e o EQ sem zipper.

O GitHub roda esses testes em todo build, e a versão só é publicada se todos passarem. Com `AURA_SNAPSHOT=ui.png`, o teste salva uma imagem da interface.

### Teste da versão web

```bash
node Tests/web-dsp-test.js        # Node, sem dependências
python3 Tests/web-ui-test.py      # precisa do Playwright com Chromium
```

O primeiro compara, amostra por amostra, o resultado do `web/aura-processor.js`
com os números medidos no plugin (nível, THD por valor de Drive, EQ, blend) e
confere que `web/layout.js` continua igual a `Source/Layout.h`. O segundo abre
o site num Chromium de verdade, com um baixo em E1 como entrada falsa, e checa
o afinador, o silêncio com o afinador ligado, os knobs, os footswitches, o
menu, o estado salvo, o painel girado no celular em pé e o funcionamento sem
internet. O primeiro também confere que a faixa, o passo e o valor padrão de
cada controle continuam iguais aos de `Source/PluginProcessor.cpp`.

## Aproximando o som da referência

As constantes de som ficam no topo de `Source/PluginProcessor.cpp`. Com o Plugin Doctor (ou outro analisador), meça o plugin de referência e o AURA Bass com os mesmos ajustes e compare:

1. **Resposta de frequência com Drive no mínimo e EQ no meio:** ajuste a pré-ênfase, o `postLpf` e o filtro do cab até as curvas coincidirem.
2. **Cada knob de EQ no máximo e no mínimo:** ajuste `lowFreq`, `midFreq`, `midQ`, `highFreq`, `presenceFreq` e as faixas em dB.
3. **Harmônicos (modo "Harmonic") em 3 posições de Drive:** muito H3 indica clipagem simétrica e muito H2, assimétrica. Ajuste `clipBias` e `maxDriveDb`. Para um caráter mais "duro", troque o `tanh` em `shape()` por um clip mais agressivo, ou coloque dois estágios em série.
4. **Nível de saída por posição de Drive:** ajuste `wetTrim` e a compensação `makeup`.
5. **Cab:** para mais realismo, troque os filtros por uma resposta ao impulso (`juce::dsp::Convolution`) de uma caixa de baixo.

Use nome, visual e presets próprios. Medir e se inspirar no som é legítimo. Copiar marca, interface ou código de outro produto não é.
