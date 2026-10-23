// Frankenstein Engine - backend web (Emscripten, sin SDL)
// Implementa las funciones DG_* de doomgeneric usando canvas y eventos HTML5.

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdint.h>
#include <stdlib.h>

#include <string.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "w_file.h"        // W_RegisterMemoryFile

// ---- Cola de teclas: el navegador las empuja, Doom las consume ----
#define KEYQ_SIZE 64
static struct { int pressed; unsigned char key; } keyq[KEYQ_SIZE];
static int keyq_r = 0, keyq_w = 0;

// Traduce el keyCode de JavaScript a una tecla de Doom (0 = ignorar)
static unsigned char MapKey(int jskey)
{
    switch (jskey)
    {
        case 37:  return KEY_LEFTARROW;
        case 38:  return KEY_UPARROW;
        case 39:  return KEY_RIGHTARROW;
        case 40:  return KEY_DOWNARROW;
        case 17:  return KEY_FIRE;      // Ctrl
        case 32:  return KEY_USE;       // Espacio
        case 16:  return KEY_RSHIFT;    // Shift
        case 18:  return KEY_LALT;      // Alt (strafe)
        case 27:  return KEY_ESCAPE;
        case 13:  return KEY_ENTER;
        case 9:   return KEY_TAB;
        case 187: return KEY_EQUALS;    // =
        case 189: return KEY_MINUS;     // -

        // F1..F12 (en doomkeys.h NO son consecutivas, por eso un switch)
        case 112: return KEY_F1;
        case 113: return KEY_F2;
        case 114: return KEY_F3;
        case 115: return KEY_F4;
        case 116: return KEY_F5;
        case 117: return KEY_F6;
        case 118: return KEY_F7;
        case 119: return KEY_F8;
        case 120: return KEY_F9;
        case 121: return KEY_F10;
        case 122: return KEY_F11;
        case 123: return KEY_F12;

        default:
            if (jskey >= 65 && jskey <= 90) return jskey + 32;  // a-z
            if (jskey >= 48 && jskey <= 57) return jskey;       // 0-9
            return 0;
    }
}

static EM_BOOL OnKey(int type, const EmscriptenKeyboardEvent *e, void *ud)
{
    (void) ud;
    unsigned char k = MapKey(e->keyCode);
    if (k == 0) return EM_FALSE;

    int next = (keyq_w + 1) % KEYQ_SIZE;
    if (next != keyq_r)
    {
        keyq[keyq_w].pressed = (type == EMSCRIPTEN_EVENT_KEYDOWN);
        keyq[keyq_w].key = k;
        keyq_w = next;
    }
    return EM_TRUE;   // evita scroll con flechas/espacio
}

// ---- Dibujo: framebuffer (0x00RRGGBB) -> canvas (RGBA) ----
EM_JS(void, js_draw, (const uint8_t *rgba, int w, int h), {
    var c = document.getElementById('canvas');
    if (!c) return;
    if (c.width !== w) { c.width = w; c.height = h; }
    var ctx = c.getContext('2d');
    var data = new Uint8ClampedArray(HEAPU8.buffer, rgba, w * h * 4);
    ctx.putImageData(new ImageData(data, w, h), 0, 0);
});

static uint32_t *rgba_buf;

// ---- Las 6 funciones de plataforma de doomgeneric ----
void DG_Init(void)
{
    rgba_buf = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, OnKey);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, OnKey);
}

void DG_DrawFrame(void)
{
    int n = DOOMGENERIC_RESX * DOOMGENERIC_RESY;
    for (int i = 0; i < n; i++)
    {
        uint32_t p = DG_ScreenBuffer[i];   // 0x00RRGGBB
        // en memoria little-endian, RGBA = R | G<<8 | B<<16 | A<<24
        rgba_buf[i] = 0xff000000u | ((p & 0xff) << 16) | (p & 0xff00) | ((p >> 16) & 0xff);
    }
    js_draw((const uint8_t *) rgba_buf, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void DG_SleepMs(uint32_t ms) { (void) ms; }   // en web no se puede bloquear

uint32_t DG_GetTicksMs(void) { return (uint32_t) emscripten_get_now(); }

int DG_GetKey(int *pressed, unsigned char *key)
{
    if (keyq_r == keyq_w) return 0;
    *pressed = keyq[keyq_r].pressed;
    *key = keyq[keyq_r].key;
    keyq_r = (keyq_r + 1) % KEYQ_SIZE;
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    EM_ASM({ document.title = UTF8ToString($0); }, title);
}

// ---- Bucle principal: el navegador nos llama en cada frame ----
static void Frame(void) { doomgeneric_Tick(); }

// ---- Arranque diferido: JavaScript entrega el WAD y luego arranca ----

// Reserva un bloque en el heap de WASM para que JS copie ahi el WAD.
// OJO: el bloque NO se libera nunca; el motor lo lee en vivo mientras dure
// la partida (W_RegisterMemoryFile no copia los datos).
EMSCRIPTEN_KEEPALIVE
uint8_t *fk_alloc(unsigned int size)
{
    return (uint8_t *) malloc(size);
}

// Registra el WAD como archivo en memoria con el nombre "name" y arranca
// el motor. El nombre importa: Doom identifica el IWAD por su nombre
// (freedoom1.wad, doom1.wad, doom2.wad...).
EMSCRIPTEN_KEEPALIVE
int fk_start(const char *name, uint8_t *data, unsigned int size)
{
    static int started = 0;
    static char *args[4];

    if (started) return 0;
    started = 1;

    W_RegisterMemoryFile(name, data, size);   // copia el nombre, no los datos

    args[0] = "doom";
    args[1] = "-iwad";
    args[2] = strdup(name);                   // Doom conserva este puntero
    args[3] = NULL;

    doomgeneric_Create(3, args);
    emscripten_set_main_loop(Frame, 0, 0);    // 0 = no bloquear (volvemos a JS)
    return 1;
}

// main ya no arranca nada: espera a que JS llame a fk_start.
int main(void)
{
    return 0;
}
