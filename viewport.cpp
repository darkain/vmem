#include <windows.h>
#include <multimon.h>

static void getViewport(RECT *r, POINT *p, RECT *sr, HWND wnd, int full) {
  if (p || sr || wnd) {
    HINSTANCE h=LoadLibrary("user32.dll");
    if (h) {
      HMONITOR (WINAPI *Mfp)(POINT pt, DWORD dwFlags) = (HMONITOR (WINAPI *)(POINT,DWORD)) GetProcAddress(h,"MonitorFromPoint");
      HMONITOR (WINAPI *Mfr)(LPCRECT lpcr, DWORD dwFlags) = (HMONITOR (WINAPI *)(LPCRECT, DWORD)) GetProcAddress(h, "MonitorFromRect");
      HMONITOR (WINAPI *Mfw)(HWND wnd, DWORD dwFlags)=(HMONITOR (WINAPI *)(HWND, DWORD)) GetProcAddress(h, "MonitorFromWindow");
      BOOL (WINAPI *Gmi)(HMONITOR mon, LPMONITORINFO lpmi) = (BOOL (WINAPI *)(HMONITOR,LPMONITORINFO)) GetProcAddress(h,"GetMonitorInfoA");    
      if (Mfp && Mfr && Mfw && Gmi) {
        HMONITOR hm;
        if (p)
          hm=Mfp(*p,MONITOR_DEFAULTTONULL);
        else if (sr)
          hm=Mfr(sr,MONITOR_DEFAULTTONULL);
        else if (wnd)
          hm=Mfw(wnd,MONITOR_DEFAULTTONULL);
        if (hm) {
          MONITORINFOEX mi;
          ZeroMemory(&mi, sizeof(MONITORINFOEX));
          mi.cbSize=sizeof(mi);

          if (Gmi(hm,&mi)) {
            if(!full) *r=mi.rcWork;
            else *r=mi.rcMonitor;
            FreeLibrary(h);
            return;
          }          
        }
      }
      FreeLibrary(h);
    }
  }
  SystemParametersInfo(SPI_GETWORKAREA,0,r,0);
}

void getViewport(RECT *r, HWND wnd, int full) {
  getViewport(r, NULL, NULL, wnd, full);
}
