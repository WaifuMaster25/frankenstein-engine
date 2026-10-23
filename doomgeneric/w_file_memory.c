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
#include "m_argv.h"
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

// ---------------------------------------------------------------------
// Registro
// ---------------------------------------------------------------------

static void LazyInit(void);

static memory_entry_t *FindEntry(const char *name)
{
    int i;

    LazyInit();

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

int W_MemoryFileExists(const char *name)
{
    return FindEntry(name) != NULL;
}

// ---------------------------------------------------------------------
// Ganchos de pruebas (solo para validar la ruta de memoria)
// ---------------------------------------------------------------------

// Lee "diskpath" de disco a un buffer y lo registra con el nombre "regname".

static int LoadFromDisk(const char *diskpath, const char *regname)
{
    FILE *f;
    byte *buf;
    long len;

    f = fopen(diskpath, "rb");

    if (f == NULL)
    {
        printf("W_Memory: no se pudo abrir %s\n", diskpath);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = malloc(len);

    if (buf == NULL || fread(buf, 1, len, f) != (size_t) len)
    {
        free(buf);
        fclose(f);
        return 0;
    }

    fclose(f);

    W_RegisterMemoryFile(regname, buf, (unsigned int) len);
    printf("W_Memory: %s cargado en memoria como %s (%ld bytes)\n",
           diskpath, regname, len);

    return 1;
}

// -memwadfrom <archivo>: carga <archivo> a RAM y lo registra con el nombre
// que se paso en -iwad. Asi se puede pedir un IWAD cuya ruta NO existe
// en disco. Se ejecuta una sola vez, la primera vez que se consulta el
// registro (mucho antes de que d_iwad.c busque el WAD).

static void LazyInit(void)
{
    static int done = 0;
    int from, iwad;

    if (done)
    {
        return;
    }

    done = 1;

    from = M_CheckParmWithArgs("-memwadfrom", 1);
    iwad = M_CheckParmWithArgs("-iwad", 1);

    if (from > 0 && iwad > 0)
    {
        LoadFromDisk(myargv[from + 1], myargv[iwad + 1]);
    }
}

// -memwad: carga a RAM cada archivo que se abra, con su mismo nombre.

void W_MemoryAutoload(const char *path)
{
    if (FindEntry(path) != NULL)
    {
        return;
    }

    LoadFromDisk(path, path);
}

// ---------------------------------------------------------------------
// Clase de archivo
// ---------------------------------------------------------------------

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
