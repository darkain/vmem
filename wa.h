#ifndef _BMEM_WA_H
#define _BMEM_WA_H

// interface to winamp ipc
#include <windows.h>

void sendWinampMsg(UINT msg, WPARAM wParam, LPARAM lParam=0);
int sendWinampKey(int msg);

#endif
