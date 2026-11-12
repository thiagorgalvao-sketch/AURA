#!/usr/bin/env python3
# AURA Bass — põe a versão nas URLs do site montado
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
#
# O GitHub Pages manda o navegador guardar os arquivos por dez minutos. Sem
# isto, quem já abriu o site continua com a cópia velha depois de uma
# publicação. Pendurando a versão na URL de cada script, folha e manifesto, a
# cópia nova não pode ser confundida com a antiga — e o service worker troca
# de cache junto, porque o nome do cache também leva a versão.
#
# Uso: python3 Tools/versiona-site.py <pasta do site> <versão>

import pathlib
import re
import sys

out = pathlib.Path(sys.argv[1])
ver = sys.argv[2]

ALVO = re.compile(r'(src|href)="([A-Za-z0-9_./-]+\.(?:js|css|webmanifest))"')

for html in list(out.glob("index.html")) + list(out.glob("*/index.html")):
    t = html.read_text(encoding="utf-8")
    html.write_text(ALVO.sub(lambda m: f'{m.group(1)}="{m.group(2)}?v={ver}"', t), encoding="utf-8")

for sw in list(out.glob("sw.js")) + list(out.glob("*/sw.js")):
    t = sw.read_text(encoding="utf-8")
    t = re.sub(r"(const CACHE = ')([^']+)(')", lambda m: m.group(1) + m.group(2) + "-" + ver + m.group(3), t, count=1)
    # o service worker não deve aceitar cópia do cache HTTP do navegador
    t = t.replace("fetch(e.request)\n", "fetch(e.request, { cache: 'reload' })\n", 1)
    sw.write_text(t, encoding="utf-8")

for js in list(out.glob("app.js")) + list(out.glob("*/app.js")):
    t = js.read_text(encoding="utf-8")
    js.write_text(t.replace("AURA_VERSAO", ver), encoding="utf-8")

print(f"versão do site: {ver}")
