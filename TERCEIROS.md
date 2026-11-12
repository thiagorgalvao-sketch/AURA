# Componentes de terceiros

O AURA Bass é obra de Thiago Galvão (veja `LICENSE`). Ele usa os componentes
abaixo, cada um sob a sua própria licença. Essas licenças se aplicam somente
a esses componentes — não às partes criadas pelo autor (DSP, interface,
desenho do painel, respostas de impulso, afinador, versão web e documentação).

| Componente | Uso | Licença |
|---|---|---|
| [JUCE 8.0.4](https://juce.com) | framework do plugin (VST3, AU, Standalone) | Licença dupla: AGPLv3 **ou** licença JUCE. Veja a observação abaixo. |
| [Roboto](https://fonts.google.com/specimen/Roboto) | fonte dos valores na interface | SIL Open Font License 1.1 — `Resources/fonts/OFL-Roboto.txt` |
| [Russo One](https://fonts.google.com/specimen/Russo+One) | fonte do logotipo | SIL Open Font License 1.1 — `Resources/fonts/OFL-RussoOne.txt` |
| [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk) | formato VST3, trazido pelo JUCE | Licença dupla: GPLv3 ou licença Steinberg |

## Sobre o JUCE

O JUCE é distribuído sob duas licenças, e cabe ao titular escolher uma:

- **AGPLv3** — gratuita, mas obriga a publicar o código-fonte do programa sob
  a mesma licença, o que é incompatível com o `LICENSE` deste projeto.
- **Licença JUCE** — tem uma faixa gratuita para pessoa física, com limite de
  receita, e **não exige** a tela "Made with JUCE" desde o JUCE 8. É a opção
  compatível com manter os direitos reservados.

Este projeto usa a licença JUCE (por isso `JUCE_DISPLAY_SPLASH_SCREEN=0` no
`CMakeLists.txt`). **Antes de distribuir ou vender o plugin**, cadastre-se em
<https://juce.com> na faixa gratuita e confira os limites de receita no texto
atual do contrato: <https://juce.com/legal/juce-8-licence/>. É gratuito e leva
poucos minutos.

## O que não vem de terceiros

Foram criados do zero para este projeto e pertencem ao autor:

- todo o processamento de áudio (`Source/PluginProcessor.cpp`,
  `web/aura-processor.js`): saturação, oversampling, equalização, blend
  paralelo, compensação de volume e presença;
- o afinador (`Source/Tuner.h`, `web/tuner.js`);
- o desenho e a arte do painel (`Resources/ui/`, gerados por
  `Tools/build_ui_assets.py` e `Tools/FootswitchRender/`);
- as respostas de impulso de alto-falante (`Resources/ir/`, geradas por
  `Tools/make_cab_irs.py`);
- toda a interface do plugin e da versão web;
- os presets, os nomes e a identidade visual.
