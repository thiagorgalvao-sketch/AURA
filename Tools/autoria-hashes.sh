#!/bin/bash
# AURA Bass — gerador do registro de integridade da obra
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
# cópia, redistribuição ou obra derivada apenas com autorização por escrito.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
#
# Gera AUTORIA-HASHES.txt: a impressão digital (SHA-256) de cada arquivo da
# obra e um resumo final. Esse resumo é o que deve receber carimbo de tempo
# gratuito (veja AUTORIA.md).
#
# Uso: bash Tools/autoria-hashes.sh
set -e
cd "$(dirname "$0")/.."

SAIDA=AUTORIA-HASHES.txt
COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "sem git")
DATA=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Arquivos da obra (o que foi criado pelo autor), em ordem estável
mapfile -t ARQUIVOS < <(git ls-files \
  'Source/*' 'Tools/*' 'Tests/*' 'web/*' 'Resources/ui/*' 'Resources/ir/*' \
  'CMakeLists.txt' 'README.md' 'LICENSE' 'TERCEIROS.md' 'AUTORIA.md' \
  '.github/workflows/*' 'packaging/*' | sort)

{
  echo "AURA BASS — REGISTRO DE INTEGRIDADE DA OBRA"
  echo "Autor e titular: Thiago Galvão"
  echo "Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados."
  echo "Leis 9.609/1998 e 9.610/1998 — a proteção independe de registro."
  echo
  echo "Gerado em (UTC): $DATA"
  echo "Commit do repositório: $COMMIT"
  echo "Arquivos: ${#ARQUIVOS[@]}"
  echo
  echo "SHA-256 de cada arquivo:"
  echo "--------------------------------------------------------------------"
  for f in "${ARQUIVOS[@]}"; do
    [ -f "$f" ] && sha256sum "$f"
  done
} > "$SAIDA.tmp"

RESUMO=$(grep -A 100000 "SHA-256 de cada arquivo:" "$SAIDA.tmp" | tail -n +3 | sha256sum | cut -d' ' -f1)

{
  cat "$SAIDA.tmp"
  echo "--------------------------------------------------------------------"
  echo
  echo "RESUMO DA OBRA (SHA-256 da lista acima):"
  echo "$RESUMO"
  echo
  echo "Esse resumo identifica a obra inteira nesta data. Qualquer alteração em"
  echo "qualquer arquivo muda o resumo. Para ter prova de data independente,"
  echo "carimbe este arquivo conforme as instruções de AUTORIA.md."
} > "$SAIDA"
rm -f "$SAIDA.tmp"

echo "$SAIDA gerado — ${#ARQUIVOS[@]} arquivos"
echo "resumo da obra: $RESUMO"
