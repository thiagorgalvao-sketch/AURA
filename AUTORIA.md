# Autoria e proteção da obra

> Este documento reúne o que protege o AURA Bass e como provar a autoria sem
> gastar nada. Não é parecer jurídico — para um caso concreto, procure um
> advogado de propriedade intelectual.

**Autor e titular:** Thiago Galvão
**Obra:** AURA Bass — plugin de overdrive e pré-amplificador para baixo, com
afinador, simulação de alto-falante e versão para navegador
**Criação iniciada em:** 23 de outubro de 2026
**Repositório oficial:** <https://github.com/thiagorgalvao-sketch/AURA>
**Endereço oficial da versão web:** <https://thiagorgalvao-sketch.github.io/AURA/>

---

## 1. A proteção já existe, e é automática

No Brasil, programa de computador é protegido pelo regime de direito autoral
(Lei nº 9.609/1998, a Lei do Software, e Lei nº 9.610/1998). A lei é expressa:

> "§ 3º A proteção aos direitos de que trata esta Lei independe de registro."
> — art. 2º, § 3º, da Lei nº 9.609/1998

Ou seja: **o direito nasce com a criação da obra**, não com o registro. O
registro no INPI tem função *declaratória* — serve como prova forte de autoria
e data, mas não cria o direito. A proteção dura 50 anos contados de 1º de
janeiro do ano seguinte ao da publicação ou, não havendo publicação, da
criação. Pela Convenção de Berna, vale também no exterior.

Portanto, **não registrar agora não significa estar desprotegido**. Significa
apenas que, num eventual conflito, a prova de autoria e de data precisa vir de
outro lugar. É isso que o resto deste documento resolve.

## 2. O que já está dentro da obra

| Onde | O que foi inserido |
|---|---|
| `LICENSE` | Licença proprietária: autoria, direitos reservados, o que é e o que não é permitido |
| `TERCEIROS.md` | Componentes de terceiros e suas licenças — separa o que é seu do que não é |
| Todo arquivo de código | Cabeçalho de copyright com nome, ano, leis aplicáveis e link do repositório |
| `Source/Autoria.h` | Dados de autoria gravados **dentro do binário** do plugin |
| Menu do plugin → "Sobre" | Autoria visível para quem usa |
| Menu da versão web → "Sobre" | O mesmo, no navegador |
| `web/index.html` | `meta author`, `meta copyright`, `link rel=license` e aviso em comentário |
| Site publicado | `LICENSE.txt` e `CREDITOS.txt` junto dos arquivos |
| Binário e pacote web | Marca `AURA-PROV/1` — veja abaixo |

### A marca AURA-PROV

Tanto o binário do plugin quanto o `app.js` da versão web carregam a cadeia:

```
AURA-PROV/1 obra=AURA-Bass autor=Thiago-Galvao ano=2026
lei=9609/1998+9610/1998 origem=github.com/thiagorgalvao-sketch/AURA
direitos=reservados
```

Se alguém redistribuir o plugin com outro nome, essa marca continua no arquivo
a não ser que a pessoa recompile o código alterado — e alterar ou remover o
aviso é, por si só, violação do art. 3 (d) da `LICENSE`. Para checar um binário
suspeito:

```bash
strings AuraBass.vst3 | grep AURA-PROV      # macOS e Linux
# na versão web: procure AURA_PROV no app.js, ou digite no console do navegador:
#   window.AURA_PROV
```

## 3. Prova de data, de graça

A ideia é conseguir dizer, com prova verificável por terceiros: *"em tal data,
este código já existia exatamente assim, e é meu"*. Três camadas, todas
gratuitas, da mais fácil para a mais forte:

### Camada 1 — O histórico do Git e do GitHub (já existe)

Cada commit guarda autor, data e o conteúdo exato. O GitHub registra do lado
dele a data em que cada commit foi recebido — isso é um registro de terceiro,
fora do seu computador. O primeiro commit desta obra é de **23/10/2026**.

Para exportar um histórico legível a qualquer momento:

```bash
git log --reverse --date=iso --format='%ad  %H  %s' > historico-aura.txt
```

Guarde também uma cópia completa do repositório (`git bundle create aura.bundle --all`)
em outro lugar — um HD externo, um pendrive, um e-mail para você mesmo.

### Camada 2 — Resumo criptográfico da obra (já gerado)

O arquivo [`AUTORIA-HASHES.txt`](AUTORIA-HASHES.txt) lista o SHA-256 de cada
arquivo da obra e termina com um **resumo** — um único código que identifica a
obra inteira naquele estado. Qualquer alteração em qualquer arquivo muda o
resumo. Para regerar depois de mudanças:

```bash
bash Tools/autoria-hashes.sh
```

### Camada 3 — Carimbo de tempo no Bitcoin (grátis, 2 minutos)

O **OpenTimestamps** ancora o resumo da sua obra na blockchain do Bitcoin. É
gratuito, não exige cadastro e a prova vale para sempre, mesmo que o serviço
saia do ar.

1. Rode `bash Tools/autoria-hashes.sh` para gerar `AUTORIA-HASHES.txt`.
2. Abra <https://opentimestamps.org> e arraste o arquivo na área
   **Stamp & Verify**.
3. Baixe o arquivo `.ots` gerado e guarde-o **ao lado** do `AUTORIA-HASHES.txt`
   (pode commitar no repositório também).
4. Depois de algumas horas, arraste o `.ots` de volta no site para "upgrade" —
   aí ele passa a apontar para um bloco já confirmado.

A partir daí, qualquer pessoa consegue verificar, sozinha, que aquele conteúdo
exato existia antes daquele bloco do Bitcoin. Refaça o carimbo a cada versão
importante.

### Camada 4 — Arquivo público e permanente (grátis)

O **Software Heritage** é um arquivo universal de código-fonte, criado pelo
Inria com apoio da UNESCO. Ele guarda o repositório com todo o histórico e
devolve um identificador permanente (SWHID, padrão ISO/IEC 18670).

1. Vá em <https://archive.softwareheritage.org/save/>
2. Escolha `git` e informe `https://github.com/thiagorgalvao-sketch/AURA`
3. Envie. Não precisa de conta.

Depois, o repositório fica consultável para sempre naquele endereço — uma
terceira instituição, independente do GitHub e de você, com data registrada.

## 4. Declaração de autoria (para imprimir e assinar)

Imprima, assine à mão, date e guarde. Se puder, peça a duas pessoas que assinem
como testemunhas. Não tem o peso de um registro no INPI, mas é prova
documental e custa zero.

---

> **DECLARAÇÃO DE AUTORIA DE PROGRAMA DE COMPUTADOR**
>
> Eu, **Thiago Galvão**, portador do CPF nº ____________________, residente em
> ______________________________________________, **declaro**, para os devidos
> fins e sob as penas da lei, que sou o **autor e único titular** dos direitos
> sobre o programa de computador denominado **AURA Bass**, descrito como
> plugin de overdrive e pré-amplificador para contrabaixo, com afinador,
> simulação de alto-falante por resposta de impulso e versão executável em
> navegador, nos formatos VST3, Audio Unit, aplicativo autônomo e aplicativo
> web.
>
> Declaro que a criação teve início em **23 de outubro de 2026** e que a obra
> foi desenvolvida de forma original, com auxílio de ferramentas de
> programação assistida por inteligência artificial sob minha direção,
> concepção e supervisão, sem cópia de código-fonte de terceiros, ressalvados
> os componentes de terceiros expressamente relacionados no arquivo
> `TERCEIROS.md`, utilizados conforme suas respectivas licenças.
>
> O conteúdo integral da obra, nesta data, é identificado pelo resumo
> criptográfico SHA-256 constante do arquivo `AUTORIA-HASHES.txt`, cujo resumo
> final é:
>
> `________________________________________________________________`
>
> O código-fonte encontra-se publicado, com histórico datado, em
> <https://github.com/thiagorgalvao-sketch/AURA>, sob licença de uso restrita,
> com todos os direitos reservados, nos termos do arquivo `LICENSE`.
>
> ____________________, ______ de ___________________ de 20____.
>
> \_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_\_
> Thiago Galvão — autor
>
> Testemunha 1: ______________________________ CPF: ____________________
>
> Testemunha 2: ______________________________ CPF: ____________________

---

## 5. Se alguém copiar

1. **Reúna a prova antes de falar**: salve prints com data, baixe o arquivo do
   infrator, rode `strings` procurando `AURA-PROV`, e compare os hashes.
2. **Notifique por escrito** o responsável, pedindo a retirada e citando a
   autoria, a `LICENSE` e a data de criação comprovada.
3. **Peça a remoção na plataforma**: GitHub, lojas de plugin, provedores de
   hospedagem e as grandes lojas de aplicativos têm formulário de violação de
   direitos autorais (no GitHub, "DMCA Takedown"). Em geral é rápido e
   gratuito, e costuma resolver sem processo.
4. Só então avalie a via judicial, com advogado.

## 6. Quando valer a pena pagar

O registro no INPI (sistema e-Software) custa **R$ 210,00** por programa e
exige certificado digital ICP-Brasil, que tem custo próprio. Ele não cria o
direito, mas é a prova mais confortável de apresentar em juízo e em
negociações. Vale considerar quando:

- o plugin começar a gerar receita;
- surgir conversa de licenciamento, venda ou sociedade;
- houver uma cópia concreta para enfrentar.

Até lá, as camadas 1 a 4 acima cobrem o essencial sem custo.

## 7. Dois cuidados importantes

**O repositório é público.** Isso ajuda a provar autoria e data, mas também
deixa o código à vista de qualquer um. A `LICENSE` reserva os direitos, porém
não impede tecnicamente a cópia. Se um dia o plugin for virar produto pago,
vale pesar deixar o código privado e publicar só os binários — lembrando que,
no plano gratuito do GitHub, o GitHub Pages exige repositório público.

**O JUCE.** O framework do plugin tem licença dupla: AGPLv3 (que obrigaria a
abrir o código sob a mesma licença, incompatível com o `LICENSE` daqui) ou a
licença JUCE, com uma faixa gratuita para pessoa física, limitada por receita e
sem exigência da tela "Made with JUCE" desde o JUCE 8. Cadastre-se na faixa
gratuita em <https://juce.com> e confira os limites no texto atual do contrato
antes de distribuir. Detalhes em `TERCEIROS.md`.
