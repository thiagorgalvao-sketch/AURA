#!/bin/bash
# Grit DI — instala os plugins já compilados neste Mac.
# Rode no Terminal:  bash  (espaço)  e arraste este arquivo para a janela, depois Enter.

set -e
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"
mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"

echo "Instalando Grit DI..."
rm -rf "$VST3_DIR/Grit DI.vst3" "$AU_DIR/Grit DI.component" "$APP_DIR/Grit DI.app"
cp -R "Grit DI.vst3" "$VST3_DIR/"
cp -R "Grit DI.component" "$AU_DIR/"
cp -R "Grit DI.app" "$APP_DIR/"

# Remove o bloqueio de "baixado da internet" e reaplica a assinatura local
for item in "$VST3_DIR/Grit DI.vst3" "$AU_DIR/Grit DI.component" "$APP_DIR/Grit DI.app"; do
    xattr -cr "$item" 2>/dev/null || true
    codesign --force --deep --sign - "$item" >/dev/null 2>&1 || true
done

# Faz o macOS reler a lista de Audio Units
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Validando o Audio Unit..."
if auval -v aufx Grdi Thgo >/tmp/gritdi-auval.log 2>&1; then
    echo "AU validado com sucesso."
else
    echo "Aviso: o auval não passou (detalhes em /tmp/gritdi-auval.log). VST3 e Standalone funcionam mesmo assim."
fi

echo ""
echo "Pronto!"
echo "  VST3:        $VST3_DIR/Grit DI.vst3"
echo "  AU:          $AU_DIR/Grit DI.component"
echo "  Standalone:  $APP_DIR/Grit DI.app"
echo ""
echo "Abra sua DAW e faça uma nova busca de plugins (no Logic: Configurações > Plug-in Manager)."
