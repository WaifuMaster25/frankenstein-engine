//
// Frankenstein Engine - WAD en memoria
//
// Clase de archivo que sirve un WAD desde un bloque de RAM en vez de
// desde disco. Base del ImageGame, de la carga desde la UI web y de las
// consolas sin sistema de archivos.
//
// Licencia: GPLv2 (igual que el resto del codigo de Doom).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "w_file.h"
#include "z_zone.h"

#define MAX_MEMORY_FILES 8

typedef struct
{
    char name[256];
    byte *data;            // no es nuestro: no se libera al cerrar
    unsigned int length;
} memory_entry_t;

static memory_entry_t entries[MAX_MEMORY_FILES];
static int num_entries = 0;

typedef struct
{
    wad_file_t wad;
} memory_wad_file_t;

extern wad_file_class_t memory_wad_file;

static memory_entry_t *FindEntry(const char *name)
{
    int i;

    for (i = 0; i < num_entries; ++i)
    {
        if (strcmp(entries[i].name, name) == 0)
        {
            return &entries[i];
        }
    }

    return NULL;
}

// Registra un bloque de memoria como si fuera el archivo "name".
// El motor NO copia ni libera los datos: deben vivir mientras se use el WAD.

void W_RegisterMemoryFile(const char *name, byte *data, unsigned int length)
{
    memory_entry_t *e;

    e = FindEntry(name);

    if (e == NULL)
    {
        if (num_entries >= MAX_MEMORY_FILES)
        {
            printf("W_Memory: demasiados archivos en memoria\n");
            return;
        }

        e = &entries[num_entries++];
    }

    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';
    e->data = data;
    e->length = length;
}

// Gancho de pruebas (-memwad): lee el archivo de disco a un buffer y lo
// registra, para ejercitar la ruta de memoria sin necesidad de un cargador.

void W_MemoryAutoload(const char *path)
{
    FILE *f;
    byte *buf;
    long len;

    if (FindEntry(path) != NULL)
    {
        return;
    }

    f = fopen(path, "rb");

    if (f == NULL)
    {
        return;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(len);

    if (buf == NULL || fread(buf, 1, len, f) != (size_t) len)
    {
        free(buf);
        fclose(f);
        return;
    }

    fclose(f);

    W_RegisterMemoryFile(path, buf, (unsigned int) len);
    printf("W_Memory: %s cargado en memoria (%ld bytes)\n", path, len);
}

static wad_file_t *W_Memory_OpenFile(char *path)
{
    memory_entry_t *e;
    memory_wad_file_t *result;

    e = FindEntry(path);

    if (e == NULL)
    {
        return NULL;
    }

    result = Z_Malloc(sizeof(memory_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &memory_wad_file;
    result->wad.mapped = e->data;        // acceso directo, sin copias
    result->wad.length = e->length;

    printf("W_Memory: sirviendo %s desde memoria\n", path);

    return &result->wad;
}

static void W_Memory_CloseFile(wad_file_t *wad)
{
    // Los datos no son nuestros: solo liberamos el manejador.
    Z_Free(wad);
}

static size_t W_Memory_Read(wad_file_t *wad, unsigned int offset,
                            void *buffer, size_t buffer_len)
{
    if (offset >= wad->length)
    {
        return 0;
    }

    if (buffer_len > wad->length - offset)
    {
        buffer_len = wad->length - offset;
    }

    memcpy(buffer, wad->mapped + offset, buffer_len);

    return buffer_len;
}

wad_file_class_t memory_wad_file =
{
    W_Memory_OpenFile,
    W_Memory_CloseFile,
    W_Memory_Read,
};
