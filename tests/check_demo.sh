#!/bin/bash
WAD="${WAD:-/c/Users/Chelo/Documents/Proyectos/freedoom1.wad}"
EXPECTED_FILE="tests/expected_hash.txt"

cd doomgeneric || exit 1
ACTUAL=$(./doomgeneric.exe -iwad "$WAD" -timedemo demo1 | grep STATEHASH)
cd ..

if [ "$1" = "--update" ]; then
    echo "$ACTUAL" > "$EXPECTED_FILE"
    echo "Referencia guardada: $ACTUAL"
    exit 0
fi

if [ "$ACTUAL" = "$(cat $EXPECTED_FILE)" ]; then
    echo "OK: $ACTUAL"
else
    echo "FALLO"
    echo "  esperado: $(cat $EXPECTED_FILE)"
    echo "  obtenido: $ACTUAL"
    exit 1
fi
