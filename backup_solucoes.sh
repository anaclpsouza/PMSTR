#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="${1:-pmstr-basecases}"
DATA_ATUAL="$(date +%Y-%m-%d_%H-%M-%S)"
BACKUP_DIR="backup-${DATA_ATUAL}"

if [ ! -d "$BASE_DIR" ]; then
    echo "Diretório base '$BASE_DIR' não encontrado."
    exit 1
fi

# Busca todas as pastas chamadas "solucoes" dentro de pmstr-basecases
mapfile -t PASTAS_SOLUCOES < <(find "$BASE_DIR" -type d -name "solucoes" | sort)

if [ "${#PASTAS_SOLUCOES[@]}" -eq 0 ]; then
    echo "Nenhuma pasta 'solucoes' encontrada dentro de '$BASE_DIR'."
    exit 0
fi

mkdir -p "$BACKUP_DIR"
echo "Criando backup em: $BACKUP_DIR"
echo "----------------------------------------"

for pasta in "${PASTAS_SOLUCOES[@]}"; do
    # Extrai o caminho relativo sem o prefixo "pmstr-basecases/" e sem o sufixo "/solucoes"
    # Ex: pmstr-basecases/6M140/Scenario1/solucoes -> 6M140/Scenario1
    caminho_rel="${pasta#${BASE_DIR}/}"
    caminho_org="${caminho_rel%/solucoes}"

    destino="${BACKUP_DIR}/${caminho_org}"
    mkdir -p "$(dirname "$destino")"

    # Move a pasta solucoes para o destino organizado (removendo-a do caminho original)
    mv "$pasta" "$destino"
    echo "[OK] $pasta -> $destino"
done

echo "----------------------------------------"
echo "Backup concluído! ${#PASTAS_SOLUCOES[@]} pasta(s) movida(s) para '$BACKUP_DIR'."

