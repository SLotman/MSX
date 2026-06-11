#ifndef MSX_PICO_H
#define MSX_PICO_H

#include "MsxTypes.h"
#include "Windows.h"

int MSXPicoCreate();
void MSXPicoDestroy();
void MSXPicoSetHWND(HWND hwnd);
void MSXPicoSetPath(char *path);
void MSXPicoNotify();
void MSXPicoMusicEnded();
#endif