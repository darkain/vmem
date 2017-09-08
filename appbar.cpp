#include "appbar.h"
#include "viewport.h"

// originally copied from http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_int/shell_int_programming/appbars.asp

static int g_uSide, g_fAppRegistered;	// gross

BOOL RegisterAccessBar(HWND hwndAccessBar, BOOL fRegister) 
{ 
    APPBARDATA abd; 

    // Specify the structure size and handle to the appbar. 
    abd.cbSize = sizeof(APPBARDATA); 
    abd.hWnd = hwndAccessBar; 

    if (fRegister) { 

        // Provide an identifier for notification messages. 
        abd.uCallbackMessage = APPBAR_CALLBACK; 

        // Register the appbar. 
        if (!SHAppBarMessage(ABM_NEW, &abd)) 
            return FALSE; 

        g_uSide = ABE_TOP;       // default edge 
        g_fAppRegistered = TRUE; 

    } else { 

        // Unregister the appbar. 
        SHAppBarMessage(ABM_REMOVE, &abd); 
        g_fAppRegistered = FALSE; 
    } 

    return TRUE; 

}

void AppBarQuerySetPos(UINT uEdge, LPRECT lprc, PAPPBARDATA pabd) 
{ 
    int iHeight = 0; 
    int iWidth = 0; 

    pabd->rc = *lprc; 
    pabd->uEdge = uEdge; 

    // Copy the screen coordinates of the appbar's bounding 
    // rectangle into the APPBARDATA structure. 
    if ((uEdge == ABE_LEFT) || 
            (uEdge == ABE_RIGHT)) { 

        iWidth = pabd->rc.right - pabd->rc.left; 
        pabd->rc.top = 0; 
        pabd->rc.bottom = GetSystemMetrics(SM_CYSCREEN); 

    } else {

RECT r = pabd->rc;
getViewport(&r, pabd->hWnd, TRUE);

        iHeight = pabd->rc.bottom - pabd->rc.top; 
#if 0
        pabd->rc.left = 0; 
        pabd->rc.right = GetSystemMetrics(SM_CXSCREEN); 
#endif

      if (uEdge == ABE_TOP) {
        pabd->rc.top = r.top;
        pabd->rc.bottom = r.top + iHeight;
        pabd->rc.left = r.left;
        pabd->rc.right = r.right;
      } else {
        pabd->rc.top = r.bottom - iHeight;
        pabd->rc.bottom = r.bottom;
        pabd->rc.left = r.left;
        pabd->rc.right = r.right;
      }
    }

    // Query the system for an approved size and position. 
    SHAppBarMessage(ABM_QUERYPOS, pabd); 

    // Adjust the rectangle, depending on the edge to which the 
    // appbar is anchored. 
    switch (uEdge) { 

        case ABE_LEFT: 
            pabd->rc.right = pabd->rc.left + iWidth; 
            break; 

        case ABE_RIGHT: 
            pabd->rc.left = pabd->rc.right - iWidth; 
            break; 

        case ABE_TOP: 
            pabd->rc.bottom = pabd->rc.top + iHeight; 
            break; 

        case ABE_BOTTOM: 
            pabd->rc.top = pabd->rc.bottom - iHeight; 
            break; 

    } 

    // Pass the final bounding rectangle to the system. 
    SHAppBarMessage(ABM_SETPOS, pabd); 

    // Move and size the appbar so that it conforms to the 
    // bounding rectangle passed to the system. 
    MoveWindow(pabd->hWnd, pabd->rc.left, pabd->rc.top, 
        pabd->rc.right - pabd->rc.left, 
        pabd->rc.bottom - pabd->rc.top, TRUE); 

}

// AppBarCallback - processes notification messages sent by the system. 
// hwndAccessBar - handle to the appbar 
// uNotifyMsg - identifier of the notification message 
// lParam - message parameter 

void AppBarCallback(HWND hwndAccessBar, UINT_PTR uNotifyMsg, LPARAM lParam) {
  APPBARDATA abd; 
  UINT_PTR uState; 

  ZeroMemory(&abd, sizeof abd);
  abd.cbSize = sizeof(abd); 
  abd.hWnd = hwndAccessBar; 

  switch (uNotifyMsg) { 
    case ABN_STATECHANGE: 
      // Check to see if the taskbar's always-on-top state has 
      // changed and, if it has, change the appbar's state 
      // accordingly. 
      uState = SHAppBarMessage(ABM_GETSTATE, &abd); 

      SetWindowPos(hwndAccessBar, 
        (ABS_ALWAYSONTOP & uState) ? HWND_TOPMOST : HWND_BOTTOM, 
         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); 

    break; 

    case ABN_FULLSCREENAPP: 

      // A full-screen application has started, or the last full- 
      // screen application has closed. Set the appbar's 
      // z-order appropriately. 
      uState = SHAppBarMessage(ABM_GETSTATE, &abd); 
      if (lParam) { 
        SetWindowPos(hwndAccessBar, 
          /*(ABS_ALWAYSONTOP & uState) ? HWND_TOPMOST :*/ HWND_BOTTOM, 
          0, 0, 0, 0, 
          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); 
      } else {

        if (uState & ABS_ALWAYSONTOP) {
          SetWindowPos(hwndAccessBar, HWND_TOPMOST, 
                       0, 0, 0, 0, 
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); 
        }
      } 
    break; //? wasn't in example

    case ABN_POSCHANGED: 
      // The taskbar or another appbar has changed its 
      // size or position. 
      AppBarPosChanged(&abd); 
    break; 
  } 
}

void AppBarPosChanged(PAPPBARDATA pabd) {
  RECT rc; 
  RECT rcWindow; 

  rc.top = 0; 
  rc.left = 0; 
  rc.right = GetSystemMetrics(SM_CXSCREEN); 
  rc.bottom = GetSystemMetrics(SM_CYSCREEN); 

  GetWindowRect(pabd->hWnd, &rcWindow); 
  int iHeight = rcWindow.bottom - rcWindow.top; 
  int iWidth = rcWindow.right - rcWindow.left; 

  switch (g_uSide) { 
    case ABE_TOP: 
      rc.bottom = rc.top + iHeight; 
    break; 

    case ABE_BOTTOM: 
      rc.top = rc.bottom - iHeight; 
    break; 

    case ABE_LEFT: 
      rc.right = rc.left + iWidth; 
    break; 

    case ABE_RIGHT: 
      rc.left = rc.right - iWidth; 
    break; 
  } 

  AppBarQuerySetPos(g_uSide, &rc, pabd); 
}
