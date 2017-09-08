#include "wa.h"

#include "config.h"

#ifndef BMEM_NO_WINAMP

// from wa_ipc.h
#define WINAMP_VOLUMEUP                 40058 // turns the volume up a little
#define WINAMP_VOLUMEDOWN               40059 // turns the volume down a little
#define WINAMP_FFWD5S                   40060 // fast forwards 5 seconds
#define WINAMP_REW5S                    40061 // rewinds 5 seconds
#define WINAMP_BUTTON1                  40044
#define WINAMP_BUTTON2                  40045
#define WINAMP_BUTTON3                  40046
#define WINAMP_BUTTON4                  40047
#define WINAMP_BUTTON5                  40048
static struct keycmdmap { int code, msg; } list[] = {
    { 'Z', WINAMP_BUTTON1 },
    { 'X', WINAMP_BUTTON2 },
    { 'C', WINAMP_BUTTON3 },
    { 'V', WINAMP_BUTTON4 },
    { 'B', WINAMP_BUTTON5 },
    { VK_DOWN, WINAMP_VOLUMEDOWN },
    { VK_UP, WINAMP_VOLUMEUP },
    { VK_LEFT, WINAMP_REW5S },
    { VK_RIGHT, WINAMP_FFWD5S },
    { -1, -1},
};

void sendWinampMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
  HWND hwnd_winamp = FindWindow("Winamp v1.x",NULL);
  if (hwnd_winamp == NULL) return;
  SendMessage(hwnd_winamp, msg, wParam, lParam);
}

int sendWinampKey(int msg) {
  for (keycmdmap *lp = list; lp->code >= 0; lp++) {
    if (msg == lp->code) {
      sendWinampMsg(WM_COMMAND, lp->msg, 0);
      return 1;
    }
  }
  return 0;
}

#endif
