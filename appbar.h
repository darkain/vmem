#ifndef _APPBAR_H
#define _APPBAR_H

#include <windows.h>

#define APPBAR_CALLBACK (WM_USER+0x1e10)

BOOL RegisterAccessBar(HWND hwndAccessBar, BOOL fRegister);
void AppBarQuerySetPos(UINT uEdge, LPRECT lprc, PAPPBARDATA pabd);
void AppBarCallback(HWND hwndAccessBar, UINT_PTR uNotifyMsg, LPARAM lParam);
void AppBarPosChanged(PAPPBARDATA pabd);

#endif
