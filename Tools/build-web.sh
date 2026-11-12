#!/bin/bash
# AURA Bass — montagem do site da versão web
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
# cópia, redistribuição ou obra derivada apenas com autorização por escrito.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
# Monta a pasta "site/" da versão web: página + imagens, IRs e fontes do projeto.
# Uso: bash Tools/build-web.sh [pasta de saída]
set -e
cd "$(dirname "$0")/.."
OUT="${1:-site}"

rm -rf "$OUT"
mkdir -p "$OUT/assets"

cp web/index.html web/style.css web/app.js web/layout.js web/tuner.js web/aura-processor.js web/sw.js web/manifest.webmanifest "$OUT/"
cp Resources/ui/background.jpg Resources/ui/arcs_lit.png Resources/ui/pointer.png \
   Resources/ui/led_drive_on.png Resources/ui/led_tuner_on.png Resources/ui/led_cab_on.png \
   Resources/ui/cab_off.png Resources/ui/footswitch_down.png Resources/ui/tuner_down.png "$OUT/assets/"
cp Resources/ir/cab_4x10.wav Resources/ir/cab_1x15.wav "$OUT/assets/"
cp Resources/fonts/Roboto.ttf Resources/fonts/RussoOne-Regular.ttf "$OUT/assets/"
cp Resources/fonts/OFL-Roboto.txt Resources/fonts/OFL-RussoOne.txt "$OUT/assets/"

# Ícones do app, recortados do próprio painel (logotipo sobre o metal).
# Os ícones ficam versionados em web/icons/ para que o site possa ser montado
# em qualquer máquina; se o Pillow estiver instalado, são regerados do painel.
if python3 -c "import PIL" 2>/dev/null; then
  python3 - "$OUT" <<'PY'
import sys
from PIL import Image
out = sys.argv[1]
panel = Image.open('Resources/ui/background.jpg').convert('RGB')
logo = panel.crop((150, 100, 1150, 300))           # faixa do logotipo, em 2x
for size in (192, 512):
    canvas = Image.new('RGB', (size, size), (16, 17, 19))
    w = int(size * 0.86)
    scaled = logo.resize((w, max(1, round(w * logo.height / logo.width))), Image.LANCZOS)
    canvas.paste(scaled, ((size - scaled.width) // 2, (size - scaled.height) // 2))
    canvas.save(f'web/icons/icon-{size}.png')
PY
fi
cp web/icons/icon-192.png web/icons/icon-512.png "$OUT/assets/"

cp LICENSE "$OUT/LICENSE.txt"
cat > "$OUT/CREDITOS.txt" <<'TXT'
AURA Bass — versão para navegador
Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.

Obra protegida pelas Leis 9.609/1998 (Lei do Software) e 9.610/1998 (Direitos
Autorais) e, no exterior, pela Convenção de Berna. Uso pessoal permitido;
cópia, redistribuição, hospedagem em outro endereço ou obra derivada apenas
com autorização por escrito do autor. Veja LICENSE.txt.

Origem oficial: https://thiagorgalvao-sketch.github.io/AURA/
Código: https://github.com/thiagorgalvao-sketch/AURA

AURA-PROV/1 obra=AURA-Bass autor=Thiago-Galvao ano=2026
lei=9609/1998+9610/1998 direitos=reservados

Fontes Roboto e Russo One sob a SIL Open Font License (assets/OFL-*.txt).
TXT
# ---- Pedalboard WORSHIP BASS ESSENTIALS (app separado, em /pedalboard) ----------------------
mkdir -p "$OUT/pedalboard/assets"
cp pedalboard/index.html pedalboard/style.css pedalboard/app.js pedalboard/engine.js \
   pedalboard/presets.js pedalboard/pb-processor.js pedalboard/sw.js \
   pedalboard/manifest.webmanifest "$OUT/pedalboard/"
cp Resources/ir/cab_4x10.wav "$OUT/pedalboard/assets/"
cp pedalboard/icons/icon-192.png pedalboard/icons/icon-512.png "$OUT/pedalboard/assets/"
cp LICENSE "$OUT/pedalboard/LICENSE.txt"
cp "$OUT/CREDITOS.txt" "$OUT/pedalboard/CREDITOS.txt"

VERSAO=$(git rev-parse --short HEAD 2>/dev/null || date -u +%Y%m%d%H%M)
python3 Tools/versiona-site.py "$OUT" "$VERSAO"

touch "$OUT/.nojekyll"
echo "site em $OUT ($(du -sh "$OUT" | cut -f1))"
