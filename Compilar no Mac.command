#!/bin/bash
# Grit DI — compila e instala no Mac (Apple Silicon e Intel).
# Dê dois cliques neste arquivo (ou botão direito > Abrir na primeira vez).

set -e
cd "$(dirname "$0")"

echo ""
echo "=============================================="
echo "  Grit DI — compilação para Mac"
echo "=============================================="
echo ""

# 1) Ferramentas da Apple (compilador, git, python3)
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Instalando as Ferramentas de Linha de Comando da Apple..."
    xcode-select --install || true
    echo ""
    echo ">> Conclua a instalação na janela que abriu e depois rode este arquivo de novo."
    read -r -p "Pressione Enter para fechar."
    exit 1
fi

# 2) CMake: usa o instalado; senão instala uma cópia local só para este projeto
CMAKE="$(command -v cmake || true)"
if [ -z "$CMAKE" ] && [ -x /Applications/CMake.app/Contents/bin/cmake ]; then
    CMAKE=/Applications/CMake.app/Contents/bin/cmake
fi
if [ -z "$CMAKE" ]; then
    echo "Instalando o CMake (cópia local, dentro da pasta do projeto)..."
    python3 -m venv .tools
    .tools/bin/python -m pip install --quiet --upgrade pip
    .tools/bin/python -m pip install --quiet cmake
    CMAKE="$PWD/.tools/bin/cmake"
fi
echo "CMake: $("$CMAKE" --version | head -1)"

# 3) Compilar (a primeira vez baixa o JUCE e leva alguns minutos)
echo ""
echo "Compilando... (a primeira vez leva de 5 a 15 minutos)"
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release
"$CMAKE" --build build --config Release --parallel "$(sysctl -n hw.ncpu)"

ART="build/GritDI_artefacts/Release"

# 4) Instalar
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/Grit DI.vst3" "$AU_DIR/Grit DI.component"
cp -R "$ART/VST3/Grit DI.vst3" "$VST3_DIR/"
cp -R "$ART/AU/Grit DI.component" "$AU_DIR/"
mkdir -p "$HOME/Applications"
rm -rf "$HOME/Applications/Grit DI.app"
cp -R "$ART/Standalone/Grit DI.app" "$HOME/Applications/"

xattr -dr com.apple.quarantine "$VST3_DIR/Grit DI.vst3" "$AU_DIR/Grit DI.component" "$HOME/Applications/Grit DI.app" 2>/dev/null || true

# Faz o macOS reler a lista de Audio Units (Logic, GarageBand etc.)
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

echo ""
echo "Validando o Audio Unit..."
if auval -v aufx Grdi Thgo >/tmp/gritdi-auval.log 2>&1; then
    echo "AU validado com sucesso."
else
    echo "Aviso: o auval não passou (detalhes em /tmp/gritdi-auval.log). VST3 e Standalone funcionam mesmo assim."
fi

echo ""
echo "=============================================="
echo "  Pronto!"
echo "  VST3:        $VST3_DIR/Grit DI.vst3"
echo "  AU:          $AU_DIR/Grit DI.component"
echo "  Standalone:  $HOME/Applications/Grit DI.app"
echo ""
echo "  Abra sua DAW e faça uma nova busca de plugins"
echo "  (no Logic: Configurações > Plug-in Manager)."
echo "=============================================="
read -r -p "Pressione Enter para fechar."
