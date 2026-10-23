#!/bin/bash
# Frankenstein Engine - test de regresion con timedemos.
#
# Uso:
#   ./tests/check_demo.sh              comprueba todos los demos contra la referencia
#   ./tests/check_demo.sh --update     regenera la referencia (SOLO si sabes que el build es bueno)
#
# Variables opcionales:
#   WAD=/ruta/al/freedoom1.wad         IWAD a usar
#   DEMOS="demo1 demo2"                demos a ejecutar (por defecto demo1 demo2 demo3)
#   MB=3                               tamano de la zona de memoria en MiB (-mb)
#
# Funciona desde cualquier carpeta.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WAD="${WAD:-/c/Users/Chelo/Documents/Proyectos/freedoom1.wad}"
DEMOS="${DEMOS:-demo1 demo2 demo3}"
EXPECTED_FILE="$ROOT/tests/expected_hash.txt"

EXTRA=()
if [ -n "$MB" ]; then
    EXTRA=(-mb "$MB")
fi

if [ -n "$1" ] && [ "$1" != "--update" ]; then
    echo "Uso: $0 [--update]"
    exit 2
fi

cd "$ROOT/doomgeneric" || exit 1

ACTUAL=""
for d in $DEMOS; do
    line=$(./doomgeneric.exe -iwad "$WAD" "${EXTRA[@]}" -timedemo "$d" 2>&1 | grep STATEHASH | tr -d '\r')
    if [ -z "$line" ]; then
        echo "ERROR: $d no produjo STATEHASH (el motor fallo o no se recompilo)"
        exit 1
    fi
    ACTUAL="${ACTUAL}${d} ${line}"$'\n'
done
ACTUAL="${ACTUAL%$'\n'}"

if [ "$1" = "--update" ]; then
    echo "$ACTUAL" > "$EXPECTED_FILE"
    echo "Referencia guardada:"
    echo "$ACTUAL"
    exit 0
fi

EXPECTED="$(tr -d '\r' < "$EXPECTED_FILE")"

if [ "$ACTUAL" = "$EXPECTED" ]; then
    echo "OK:"
    echo "$ACTUAL"
else
    echo "FALLO"
    echo "--- esperado"
    echo "$EXPECTED"
    echo "--- obtenido"
    echo "$ACTUAL"
    exit 1
fi
