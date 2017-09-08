// bmem.cpp: by Brennan Underwood
// this file is part of BMEM, see license.txt for licensing

// notes: the original height comes from the dialog box, which only exists
// as an easy way to get a consistent physical height on the monitor no
// matter the resolution. well, and to be the main window.

#include <stdio.h> // sprintf
#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <time.h>

#include <math.h> // floor

#include "appbar.h"
#include "cpu.h"
#include "diskio.h"
#include "viewport.h"
#include "wa.h"

extern "C" {
#include <powrprof.h>
}

#ifndef _PROCESSOR_POWER_INFORMATION
typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG  Number;
  ULONG  MaxMhz;
  ULONG  CurrentMhz;
  ULONG  MhzLimit;
  ULONG  MaxIdleState;
  ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION , *PPROCESSOR_POWER_INFORMATION;
#endif

#define COLOR_TEXT          RGB(0xf0, 0xf0, 0xf0)
#define COLOR_TEXT_SHADOW   RGB(0x10, 0x10, 0x10)
#define COLOR_RAM           RGB(  41,  200,    4)
#define COLOR_CACHE         RGB( 166,  191,    3)
#define COLOR_VM            RGB( 156,   36,   28)
#define COLOR_CPU           RGB(  36,  147,  179)
#define COLOR_CPU_HT        RGB(  16,  220,  189)
#define COLOR_MHZ           RGB(  36,  180,  114)
#define COLOR_DISKIO        RGB( 255,    0,    0)
#define COLOR_SIZER         RGB(  96,   96,   96)


#define TEXT_NONE     -1
#define TEXT_MEMORY    1
#define TEXT_PERCENT   2
#define TEXT_MHZ       3


#define WINDOW_ALPHA 160

#define APPBAR_BORDER_WIDTH 1

#ifdef NDEBUG

// these pragmas help keep the .exe small
#pragma optimize("gsy",on)
//#pragma comment(linker,"/RELEASE")
//#pragma comment(linker,"/opt:nowin98")

#endif

#pragma comment(lib, "powrprof.lib")


#define STRLEN lstrlen

#include "resource.h"

#define APPNAME "vmem"

#define TIMER_CPU_READ		2
#define TIMER_INVALIDATE	3
#define TIMER_TOPMOST		4
#define TIMER_SWAPOUT		5

#define DEFAULT_INTERVAL 66
#define DEF_FATFONT 0

#define IDC_ABOUT 30009
#define IDC_TASKMGR 30010
#define IDC_CLEARCACHE 30019
//#define IDC_10000 30016
//#define IDC_2000 30010
//#define IDC_1000 30011
#define IDC_500 30012
#define IDC_250 30013
#define IDC_100 30014
#define IDC_66 30015
#define IDC_50 30016

#define IDC_SHOWMEM	30020
#define IDC_SHOWVIRT	30021
#define IDC_SHOWCPU	30021
#define IDC_FATFONT	30022
#define IDC_SHOWTEXT	30023
#define IDC_SHOWINDIVIDUALCPUS	30024
#define IDC_QUIT	30025
#define IDC_USE_BORDER	30026
#define IDC_USE_ALPHA	30027
#define IDC_HALFHEIGHT	30028
#define IDC_APPBAR_TOP	30029
#define IDC_APPBAR_BOT	30030
#define IDC_APPBAR_LEF	30031
#define IDC_APPBAR_RIG	30032
#define IDC_APPBAR_UN   30033

const int SIZER_WIDTH = 8;
const int MINWIDTH = 220;
const int MAXWIDTH = 4096;
const int DEF_WIDTH = 400;
const int DEF_X = 200;
const int DEF_Y = 0;
#define VBORDER 1	// sexay
#define HBORDER 1

#define int64 __int64

#include "config.h"

HINSTANCE kernel32 = NULL;	// kernel32.dll
BOOL (__stdcall *globalMemoryStatusEx)(MEMORYSTATUSEX *);
void (__stdcall *getNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);
BOOL (__stdcall *isWow64Process)(HANDLE hProcess, PBOOL Wow64Process);

static HINSTANCE user32 = NULL;
static int checked_for_alpha_proc;

#ifndef LWA_ALPHA
#  define LWA_ALPHA     2;
#endif 

#ifndef WS_EX_LAYERED
#  define WS_EX_LAYERED 0x80000;
#endif

static void (__stdcall *setLayeredWindowAttributes)(HWND, int, int, int);
void setAlpha(HWND hwnd, int alpha) {
  if (setLayeredWindowAttributes == NULL /*|| updateLayeredWindow == NULL*/) {
    if (user32 == NULL) user32 = LoadLibrary("USER32.DLL");
    if (user32 != NULL) {
      if (setLayeredWindowAttributes == NULL /*|| updateLayeredWindow == NULL*/) {
        if (!checked_for_alpha_proc) {
          setLayeredWindowAttributes=(void (__stdcall *)(HWND,int,int,int))GetProcAddress(user32 ,"SetLayeredWindowAttributes");
//        updateLayeredWindow=(BOOL (__stdcall *)(HWND,HDC,POINT*,SIZE*,HDC,POINT*,COLORREF,BLENDFUNCTION*,DWORD))GetProcAddress(user32 ,"UpdateLayeredWindow");
        }
        checked_for_alpha_proc = 1;
      }
    }
  }
  if (setLayeredWindowAttributes != NULL) {
    (*setLayeredWindowAttributes)(hwnd, 0, alpha, LWA_ALPHA); 
  }
}

//
// System cache information class
//
#define SYSTEMCACHEINFORMATION 0x15

//
// Used to get cache information
//
typedef struct {
	ULONG    	CurrentSize;
	ULONG    	PeakSize;
	ULONG    	PageFaultCount;
	ULONG    	MinimumWorkingSet;
	ULONG    	MaximumWorkingSet;
	ULONG    	Unused[4];
} SYSTEM_CACHE_INFORMATION;

typedef struct {
  SYSTEM_CACHE_INFORMATION a;
  char fuck[256];
} combo;

//
// Calls to get and set the information
//
ULONG (__stdcall *NtQuerySystemInformation)( 
				ULONG SystemInformationClass, 
				PVOID SystemInformation, 
				ULONG SystemInformationLength,
				PULONG ReturnLength 
				);

ULONG (__stdcall *NtSetSystemInformation)( 
				ULONG SystemInformationClass, 
				PVOID SystemInformation, 
				ULONG SystemInformationLength
				); 

static bool screensaver_active;
static void checkScreenSaverRunning() {
  HWND hwnd_ss = FindWindow("WindowsScreenSaverClass",NULL);
  int ss_active=0;
#ifndef SPI_GETSCREENSAVERRUNNING
#define SPI_GETSCREENSAVERRUNNING 0x72
#endif
  BOOL r = SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, NULL, &ss_active, NULL);
  if (!r) {
    ss_active = 0;
  }
  ss_active |= (hwnd_ss != NULL);
  screensaver_active = !!ss_active;
}

/*
static bool detected_fullscreen;
static BOOL CALLBACK enumWindowsProc(
  HWND hwnd,      // handle to parent window
  LPARAM lParam   // application-defined value
) {
  GetWindowRect(hwnd, &rect);
}
*/

static bool detectFullScreenWindow() {
#if 0
  detected_fullscreen = false;
  EnumWindows(enumWindowsProc, 0);
  detected_fullscreen = false;
#endif
  const int w = GetSystemMetrics(SM_CXSCREEN);
  const int h = GetSystemMetrics(SM_CYSCREEN); 

  HWND hWnd = 0;
  while (hWnd = FindWindowEx(NULL, hWnd, NULL, NULL)) {
    if (GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) {
      RECT rcWindow;
      GetWindowRect(hWnd, &rcWindow);
      if ((w >= (rcWindow.right - rcWindow.left)) &&
          (h >= (rcWindow.bottom - rcWindow.top)))
        return true;
    }
  }
  return false;
}

int isNT;

RECT phys, virt, cpu, mhz, diskio, netio;

HWND memWindow = NULL;
HINSTANCE hInst;
static int topmost;

#define NORMAL_DIV 0		// 1<<0 = 1
#define HALFHEIGHT_DIV 1	// 1<<1 = 2
#define MAX_HEIGHT_DIV 2	// 1<<2 = 4

int fatfont, showtext;
int height_div=NORMAL_DIV;
int showindcpu;
int use_alpha=0;
int appbar_mode=0;
int appbar_side = ABE_TOP;

int popup_showing;

int pagesize;

bool show_vm = true;

static void set_config_str(const char *name,const char *str) {
  WritePrivateProfileString(APPNAME, name, str, ".\\" APPNAME".ini");
}

static void set_config_int(const char *name, int val) {
  char str[40];
  wsprintf(str, "%d", val);
  set_config_str(name,str);
  if (memWindow) InvalidateRect(memWindow, NULL, FALSE);
}

static int get_config_int(const char *name, int def=0) {
  char def_val[20]; // todo: use c++ header file for integer sizes to specify how big a buf we need
  char buf[20]="";
  wsprintf(def_val, "%d", def);
  const int r = GetPrivateProfileString(APPNAME, name, def_val, buf, sizeof buf, ".\\"APPNAME".ini");
  buf[sizeof(buf-1)] = '\0'; // force 0-termination for safety (even tho api does it)
  return atoi(buf);
}

static void init_config_storage() {
  const int cchBuf=MAX_PATH*2;
  char buf[cchBuf];
  DWORD len = GetModuleFileName(NULL, buf, cchBuf);
  char*p = &buf[len];
  while(*p!='\\' && *p!='/')--p;
  *p = NULL;
  SetCurrentDirectory(buf);//hacky way to avoid giving full path in g/set_config_str

  DWORD csidl = get_config_int("configdir",1);
  if(csidl) {
	switch(csidl) 
	{
	case 3:csidl=CSIDL_COMMON_APPDATA;break;
	case 2:csidl=CSIDL_LOCAL_APPDATA;break;
	default:csidl=CSIDL_APPDATA;break;
	}
	if(SUCCEEDED(SHGetFolderPath(NULL,csidl|CSIDL_FLAG_CREATE,NULL,0,buf)))SetCurrentDirectory(buf);
  }
}

static void set_fatfont(int f) {
  fatfont = f;
  set_config_int("fatfont", f);
}

static void set_showtext(int f) {
  showtext = f;
  set_config_int("showtext", f);
}

static void set_height_div(int f) {
  height_div = f;
  set_config_int("height_div", f);
}

static void set_showindcpu(int f) {
  showindcpu = f;
  set_config_int("showindividualcpus", f);
}

static void set_appbar_mode(int f) {
  appbar_mode = f;
  set_config_int("appbar_mode", f);
}

static void set_appbar_side(int f) {
  appbar_side = f;
  set_config_int("appbar_side", f);
}

static void set_use_alpha(int f) {
  use_alpha = f;
  set_config_int("use_alpha", f);
}

static int interval = DEFAULT_INTERVAL, timer_set;
static void set_timer_interval(int t) {
  interval = t;
  KillTimer(memWindow, TIMER_INVALIDATE);
  if (t > 0) {
    SetTimer(memWindow, TIMER_INVALIDATE, interval, NULL);
  }
  set_config_int("timer_interval", t);
  if (memWindow) InvalidateRect(memWindow, NULL, FALSE);
}

static int isXPOrGreater() {
  static int isxp=-1;
  if (isxp == -1) {
    OSVERSIONINFO osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    if (osvi.dwMajorVersion > 5 || (osvi.dwMinorVersion==5 && osvi.dwMinorVersion >=1)) {
        isxp = 1;
        return 1;
      }
    isxp = 0;
  }
  return isxp;
}
static int isXP;

static void unAppbar(HWND hDlg) {
  if (appbar_mode)
    RegisterAccessBar(hDlg, FALSE); // bah bah
  set_appbar_mode(FALSE);
  InvalidateRect(hDlg, NULL, FALSE);
  PostMessage(hDlg, WM_SIZE, 0, 0); // resize display
}

static void goAppbar(HWND hDlg, int side) {
  if (appbar_mode) unAppbar(hDlg);	// detach before reattach

  RegisterAccessBar(hDlg, TRUE); // git it on
  set_appbar_side(side);
  RECT rr;
  GetWindowRect(hDlg, &rr);
  APPBARDATA abd; 
  ZeroMemory(&abd, sizeof abd);
  abd.cbSize = sizeof(abd); 
  abd.hWnd = hDlg; 
  SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
  AppBarQuerySetPos(side, &rr, &abd);
  InvalidateRect(hDlg, NULL, FALSE);
  set_appbar_mode(TRUE);

  PostMessage(hDlg, WM_SIZE, 0, 0);	// resize display
}

static void fillRect(HDC hdc, RECT *r, COLORREF color, bool shaded=true) {
  HBRUSH brush = CreateSolidBrush(color);
  const bool rounded = true;

  if (!rounded) {
    FillRect(hdc, r, brush);

  } else {
    COLORREF brushcolor = color;

    if (shaded) {  // blend in 25% black
      brushcolor = ((color & 0x00fcfcfc) >> 2) + ((color & 0x00fefefe) >> 1);
    }

    HBRUSH brush2 = CreateSolidBrush(brushcolor);

    RECT cr = *r;
    cr.left++; cr.right--;
    if (cr.left < cr.right) FillRect(hdc, &cr, brush2);

    cr = *r;
    cr.top++; cr.bottom--;
    if (cr.top < cr.bottom) FillRect(hdc, &cr, brush);

    DeleteObject(brush2);
  }

  DeleteObject(brush);
}

// this renders one particular bar
static void renderBox(HDC hdc, RECT r,
  int64 avail, int64 total, int64 line,
  COLORREF color, COLORREF color2,
  int text_type, int combine_line, bool shaded=true)
{
  const int width = r.right - r.left;
  const int height = r.bottom - r.top;

// fill in background
  COLORREF halfcolor = (color & 0x00fcfcfc) >> 2;
  fillRect(hdc, &r, halfcolor, shaded);

// fill in foreground
  double percent = (double)avail / (double)total;

  const int barwidth = (int)floor(width * percent);

  RECT nr = r;
  nr.right = nr.left + barwidth;
  const int prevright = nr.right;
  if (barwidth > 0) fillRect(hdc, &nr, color, shaded);
  const int prevleft = nr.left;

// fill in sub-bar if given one
  if (line != 0) {
    percent = (double)line / (double)total;

    nr = r;
    if (line > avail) nr.left += prevleft;

    const int subbarwidth = (int)floor(width*percent);
    nr.right = nr.left + subbarwidth;
    if (nr.right > prevright) nr.left = prevright;

    if (subbarwidth > 0) fillRect(hdc, &nr, color2, shaded);
  }

  if (showtext) {
    // label
//    HFONT oldfont;
//    if (!fatfont) oldfont = (HFONT)SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));
//	PointSize
//	  SetMapMode(hdc, MM_TEXT);
    int fh = 0;
	if (height_div == NORMAL_DIV) {
		fh = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	} else {
		fh = -MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	}

	const char  *font_name = "Arial";
	if (fatfont) font_name = "Arial Bold";


	HFONT myFont = CreateFont(fh, 0, 0, 0, FW_NORMAL, 0, 0, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, font_name);
	HFONT oldfont = (HFONT)SelectObject(hdc, myFont);

    char txt[512]="";

    if (text_type == TEXT_PERCENT) {
		if (combine_line && line > 0) {
		  percent = (double)(avail+line) / (double)total;
		}
		sprintf_s(txt, sizeof(txt), "%2.0f%%", percent * 100.f);

    } else if (text_type == TEXT_MEMORY) {
        double t = (double)avail;
        t /= (1024.0*1024.0);
        sprintf_s(txt, sizeof(txt), "%2.1f Mb", t);

	} else if (text_type == TEXT_MHZ) {
		double t = (double) avail;
		sprintf_s(txt, sizeof(txt), "%2.0f MHz", t);

	} else {
		strcpy_s(txt, 1, "");
	}


    SIZE size;
    GetTextExtentPoint32(hdc, txt, STRLEN(txt), &size);

    if (/*size.cy <= height+1*/height_div != MAX_HEIGHT_DIV) {
      int oldmode = SetBkMode(hdc, TRANSPARENT);

      COLORREF oldcolor = SetTextColor(hdc, COLOR_TEXT_SHADOW);

      int texty = r.top+(height - size.cy)/2 ;//- !fatfont;

      TextOut(hdc, r.left+(width - size.cx)/2+1, texty, txt, STRLEN(txt));
      TextOut(hdc, r.left+(width - size.cx)/2+1, texty+1, txt, STRLEN(txt));
      TextOut(hdc, r.left+(width - size.cx)/2, texty+1, txt, STRLEN(txt));

      SetTextColor(hdc, COLOR_TEXT);
      TextOut(hdc, r.left+(width - size.cx)/2, texty, txt, STRLEN(txt));

      // done, clean up
      //if (!fatfont) SelectObject(hdc, oldfont);
      SelectObject(hdc, oldfont);

      SetBkMode(hdc, oldmode);
      SetTextColor(hdc, oldcolor);
    }

	DeleteObject(myFont);
  }//if showtext
}

struct myMEMORYSTATUS {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    SIZE_T dwTotalPhys;
    SIZE_T dwAvailPhys;
    SIZE_T dwTotalPageFile;
    SIZE_T dwAvailPageFile;
    SIZE_T dwTotalVirtual;
    SIZE_T dwAvailVirtual;
};
struct myMEMORYSTATUSEX {
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
};

static void myGetMemoryStatus(
int64 *phys_total, int64 *phys_avail,
int64 *virt_total, int64 *virt_avail) {
  if (!globalMemoryStatusEx) {
    myMEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus((MEMORYSTATUS *)&ms);
    if (phys_avail) *phys_avail = ms.dwAvailPhys;
    if (phys_total) *phys_total = ms.dwTotalPhys;

    if (virt_avail) *virt_avail = ms.dwAvailPageFile;
    if (virt_total) *virt_total = ms.dwTotalPageFile;
  } else {
    myMEMORYSTATUSEX msex;
    msex.dwLength = sizeof(msex);
    globalMemoryStatusEx((MEMORYSTATUSEX *)&msex);
    if (phys_avail) *phys_avail = msex.ullAvailPhys;
    if (phys_total) *phys_total = msex.ullTotalPhys;

    if (virt_avail) *virt_avail = msex.ullAvailPageFile;
    if (virt_total) *virt_total = msex.ullTotalPageFile;
  }
}

double timestamp() {
  static bool time_inited;
  static bool has_perf_counters;
  static LARGE_INTEGER time_freq, time_offset;
{
  if (!time_inited) {
    time_inited = true;
    const BOOL r = QueryPerformanceFrequency(&time_freq);
    if (!r) goto fallback;
    const BOOL r2 = QueryPerformanceCounter(&time_offset);
    if (!r2) goto fallback;
    has_perf_counters = true;
  }
  if (!has_perf_counters) goto fallback;

  LARGE_INTEGER ll;
  const BOOL r = QueryPerformanceCounter(&ll);

  if (!r) goto fallback;

  ll.QuadPart -= time_offset.QuadPart;

  const double lld = static_cast<double>(ll.QuadPart);
  const double llf = static_cast<double>(time_freq.QuadPart);

  return lld / llf;
}

  fallback:

  //l8r needs winmm.lib return ((double)timeGetTime())/1000.f;
  return ((double)GetTickCount())/1000.f;
}

static double final_usage[MAX_CPUS];
static double allcpu_usage;

void read_cpus() {
  static double weights[NCPUHISTORY];

  const double interval_s = CPU_INTERVAL; // needs to fit into NCPUHISTORY

  const double now = timestamp();
  static double last_read_at;

  static double prev_usage[MAX_CPUS][NCPUHISTORY];

  bool first=false;
  if (last_read_at == 0) {
    last_read_at = now - CPU_READ_INTERVAL/1000.;
    // initialize weights to reasonable values
    for (int i = 0; i < NCPUHISTORY; i++) {
      weights[i] = CPU_READ_INTERVAL/1000.;
    }
    first = true;
  }

  const double weight = now - last_read_at;
  //const double weight = min(now - last_read_at, interval_s/2);	// cap weight to reasonable size

#if 0
  if (weight <= 0) {	// ensure no time goin backwards
    return;
  }
#endif

  last_read_at = now;

  // advance buffer (future: circular buffer) newest on end, oldest at #0
  memcpy(weights, weights+1, sizeof(double) * (NCPUHISTORY-1));
  weights[NCPUHISTORY-1] = weight;

  double total_weight=0;
  int num_cpuhistory=0;
  double oldest_sample_weight = 1.f;
  for (int j = NCPUHISTORY-1; j >= 0; j--) {	// seek backwards for how many samples fit in interval_s
    num_cpuhistory++;
    if (num_cpuhistory >= 2 && total_weight >= interval_s) {	//FUCKO epsilon FUCKO magicnum
      const double diff = total_weight - interval_s;
      // weight oldest sample so we don't consider the part that's past the interval_s
      oldest_sample_weight = (diff / interval_s) * weights[j];
      total_weight += oldest_sample_weight;
//FUCKO assert total_weight == interval_s?
      break;
    }
    total_weight += weights[j];
  }

  allcpu_usage = 0;
  for (int i = 0; i < get_num_cpus(); i++) {
    // get the usage for this CPU (0-100)
    const double usage = get_cpu_usage(i);

    // add this usage to the list of recent values
    memcpy(&prev_usage[i][0], &prev_usage[i][1], sizeof(double)*(NCPUHISTORY-1));
    prev_usage[i][NCPUHISTORY-1] = usage;

    // sum up all the values
    // seek backwards from now until we have enough samples
    double sum = 0;
    for (int c=0, k = NCPUHISTORY-1; c < num_cpuhistory && k >= 0; c++, k--) {
      const double ww = (c == (num_cpuhistory-1)) ? oldest_sample_weight : weights[k];
      //const double ww = weights[k];
      sum += prev_usage[i][k] * ww;
    }

    const double final = sum / total_weight;

    // add to total cpu use
    allcpu_usage += final;
    // and keep track of individual too
    final_usage[i] = final;
  }
}



void calculateBoxSizes(HWND hDlg) {
      RECT r;
      GetWindowRect(hDlg, &r);
      // normalize rect to 0,0 in top left corner
      r.right -= r.left; r.left = 0;
      r.bottom -= r.top; r.top = 0;
      int w = r.right;
      int h = r.bottom;

#if 0//this was tryin to add an extra pixel of border on the desktop/appbar shared side... eh
      if (appbar_mode && height_div == MAX_HEIGHT_DIV) {
        switch (appbar_side) {
          case ABE_TOP:
            h -= APPBAR_BORDER_WIDTH;
          break;
#if 0//later
          case ABE_BOTTOM:
            h -= APPBAR_BORDER_WIDTH;
          break;
#endif
        }
      }
#endif

      const int topoff = r.top + 1;
      const int height = r.top + h - VBORDER;

// disk IO light
      //int diskio_w = h;
      const int diskio_w = height;
      const int diskio_x = r.right - diskio_w - 2;

      SetRect(&diskio, diskio_x, topoff, diskio_x+diskio_w, height);

      w -= diskio_w+1;	// for diskio (proportional to height ie a square)

      w -= 1;	// right border, looks better on LCD

      // give the 3 main meters the rest of the space equally

#define PIXELS_BETWEEN_DISPLAYS 3
      int pixels_between_displays = PIXELS_BETWEEN_DISPLAYS;
//      if (height_div == MAX_HEIGHT_DIV) pixels_between_displays = 3;

      const int NUM_DISPLAYS = 5 + (show_vm * 2);

      // reserve space in between
      w -= pixels_between_displays * (NUM_DISPLAYS+1 - 1 - show_vm);	// +1 for diskio -2 for doubled up stuff

      int remain = w % NUM_DISPLAYS;
      w /= NUM_DISPLAYS;

      int x = r.left + HBORDER;
      int left = x;

// physical mem
      x += (w * 2);
      if (remain) { x++; remain--; }
      if (remain) { x++; remain--; }
      SetRect(&phys, left, topoff, x, height);

      x += pixels_between_displays;

	if (show_vm) {
	// virtual mem
      left = x;
      x += (w * 2);
      if (remain) { x++; remain--; }
      if (remain) { x++; remain--; }
      SetRect(&virt, left, topoff, x, height);

      x += pixels_between_displays;
	}

	// cpu
      left = x;
      x += (w * 2);
      if (remain) { x++; remain--; }
      if (remain) { x++; remain--; }
      SetRect(&cpu, left, topoff, x, height);

      x += (pixels_between_displays * 2);

	// mhz
      left = x;
      x += w;
      if (remain) { x++; remain--; }
      SetRect(&mhz, left, topoff, x, height);
}




// the main render fn
static int renderAll(HDC hdc, HWND hDlg) {
  // get global mem status
  int64 phys_total=0, phys_avail=0;
  int64 virt_total=0, virt_avail=0;

  // get OS memory stats
  myGetMemoryStatus(&phys_total, &phys_avail,
                    &virt_total, &virt_avail);

  /* find out how much of that is disk caching */

  int64 cachesize=0;
  if (!isNT) {
    // force a recalc of performance data
    RegQueryValueEx(HKEY_PERFORMANCE_DATA, "", 0, NULL, NULL, NULL);
    // read out the value
    HKEY perfstats;
    if (RegOpenKeyEx(HKEY_DYN_DATA, "PerfStats\\StatData", 0,
                     KEY_QUERY_VALUE,
                     &perfstats) == ERROR_SUCCESS) {
      unsigned long dw = REG_DWORD, siz = sizeof(int);
      unsigned long tmp=0;
      if (RegQueryValueEx(perfstats, "VMM\\cpgDiskCache", NULL, &dw,
                          (LPBYTE)&tmp, &siz) == ERROR_SUCCESS) {
        cachesize = tmp;
        if (dw == REG_DWORD || dw == REG_BINARY)
          phys_avail += cachesize;
      }
      RegCloseKey(perfstats);
    }
  } else if (NtQuerySystemInformation) {
      //SYSTEM_CACHE_INFORMATION sci;
      combo sci;
      ULONG rl;
      ZeroMemory(&sci, sizeof sci);
      if (!NtQuerySystemInformation(SYSTEMCACHEINFORMATION, &sci, sizeof sci, &rl)) {
        //cachesize = sci.a.CurrentSize;
        cachesize = sci.a.Unused[0] * pagesize;
      }
  }

// render physical memory usage
  renderBox(hdc, phys, phys_avail, phys_total, cachesize, COLOR_RAM,
            COLOR_CACHE, TEXT_MEMORY, FALSE);

  if (isXP) { // XP and up add the size of physical memory to virtual
    virt_avail -= phys_total;
    virt_total -= phys_total;
  }
  // clamp negative virtual mem to 0
  if (virt_avail < 0) virt_avail = 0;
  if (virt_total < 0) virt_total = 0;

  const bool prev_show_vm = show_vm;
  if (virt_total <= 16*1024*1024) {
    show_vm = false;
  } else {
    show_vm = true;
  }
  if (show_vm != prev_show_vm) {
    calculateBoxSizes(hDlg);
  }

  if (show_vm) { // render page file usage
    renderBox(hdc, virt,
      virt_avail, virt_total, 0,
      COLOR_VM, 0, TEXT_MEMORY, FALSE);
  }

// render CPU usage
  const int cpuw = cpu.right - cpu.left;
  const int show_n_cpus = get_num_physical_cpus();
  if (showindcpu)  {
    // render each cpu
    const int percpuw = cpuw / show_n_cpus;
    int remain = cpuw % show_n_cpus;
    RECT r = cpu;

    // use core map to determine which cpus are on the same physical core (HT)
    if (cpu_core_map_valid) {
      for (int i = 0; i <= get_num_physical_cpus(); i ++) {
        r.right = r.left + percpuw;
        if (remain) {
          r.right++;
          remain--;
        }

        const unsigned long cpu_map = cpu_core_map[i];
        double usage = 0;
        for (int j = 0; j < sizeof(cpu_map)*8; j++) {
          const unsigned long bit = 1<<j;
          if (cpu_map & bit) {
            usage += final_usage[j];
          }
        }

        double line = 0;
        if (usage > 100) {	// HT overflow!
          line = usage - 100;
          usage = 100;
        }

        // round to int for display
        const int int_usage = (int)(usage+0.5f);
        const int int_line = (int)(line+0.5f);
        renderBox(hdc, r, int_usage, 100, int_line, COLOR_CPU, COLOR_CPU_HT, TEXT_PERCENT, TRUE);
        r.left = r.right + 1;
      }
    } else {	 // fall back on assuming alternating physical/virtual numbering
      const bool htcombine = (get_num_cpus() != get_num_physical_cpus());
      for (int i = 0; i < get_num_cpus(); i += get_num_cpus() / show_n_cpus) {
        r.right = r.left + percpuw;
        if (remain) {
          r.right++;
          remain--;
        }

		double usage = final_usage[i];
		if (htcombine) {
		  usage += final_usage[i+1];
		}

		double line = 0;
		if (usage > 100) {	// HT overflow!
		  line = usage - 100;
		  usage = 100;
		}

		// round to int for display
		const int int_usage = (int)(usage+0.5f);
		const int int_line = (int)(line+0.5f);
		renderBox(hdc, r, int_usage, 100, int_line, COLOR_CPU, COLOR_CPU_HT, TEXT_PERCENT, TRUE);
		r.left = r.right + 1;
      }
    }
  } else {
    // render all cpus merged into one
    double usage = allcpu_usage;
    double line = 0;

    const int max_usage = 100*show_n_cpus;
    if (usage > max_usage) {
      line = usage - max_usage;
      usage = max_usage;
    }

    // round to int for display
    const int int_usage = (int)(usage+0.5f);
    const int int_line = (int)(line+0.5f);

    renderBox(hdc, cpu, int_usage, max_usage, int_line, COLOR_CPU, COLOR_CPU_HT, TEXT_PERCENT, TRUE);
  }


	//Render MHz value
	{
		double usage = 0;
		double total = 100;
		usage = 0;
		PROCESSOR_POWER_INFORMATION info[MAX_CPUS];
		memset(&info, 0, sizeof(info));

		if (CallNtPowerInformation(ProcessorInformation, NULL, 0, info, sizeof(info)) == ERROR_SUCCESS) {
			static double prev_usage[NMHZHISTORY] = { -1 };
			if (prev_usage[0] < 1) {
				for (int xi=0; xi<NMHZHISTORY; xi++) {
					prev_usage[xi] = info[0].CurrentMhz;
				}
			}

			double subtotal = 0;
			for (int xi=0; xi<NMHZHISTORY-1; xi++) {
				prev_usage[xi] = prev_usage[xi+1];
				subtotal += prev_usage[xi];
			}

			prev_usage[NMHZHISTORY-1] = info[0].CurrentMhz;
			
			subtotal += info[0].CurrentMhz;

			usage = subtotal / NMHZHISTORY;
			total = info[0].MaxMhz;
		}

		const int int_usage = (int)(usage+0.5f);
		const int int_total = (int)(total+0.5f);
		renderBox(hdc, mhz, int_usage, int_total, 0, COLOR_MHZ, COLOR_CPU_HT, TEXT_MHZ, TRUE);
	}


	// and disk IO light
	{
		const int show_disk_light = disk_io;
		disk_io = 0;
		renderBox(hdc, diskio, show_disk_light, 1, 0, RGB(255,0,0), 0, TEXT_NONE, FALSE, true);
	}

//someday
//  renderBox(hdc, netio, !!net_io, 1, 0, RGB(0,255,0), 0, -1);
//  net_io = 0;

  return 1;
}

void keepOnScreen(HWND hwnd) {
  RECT sr;
  getViewport(&sr, hwnd, TRUE);
  RECT r;
  GetWindowRect(hwnd, &r);
  int need_to_move = 0;
  if (r.top < sr.top) {
    r.top = sr.top;
    need_to_move = 1;
  }
  if (r.right >= sr.right) {
    int w = (r.right - r.left);
    r.left = sr.right - w;
    need_to_move = 1;
  }
  if (r.left < sr.left) {
    r.left = sr.left;
    need_to_move = 1;
  }
  if (r.bottom >= sr.bottom) {
    r.top = sr.bottom - (r.bottom - r.top);
    need_to_move = 1;
  }
  if (need_to_move)
    SetWindowPos(hwnd, NULL, r.left, r.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

  /* re-get coords */
  GetWindowRect(hwnd, &r);

  set_config_int("xpos",r.left);
  set_config_int("ypos",r.top);
  set_config_int("width",r.right - r.left);
}


#define cardinal(n) (((n)==1)?"":"s")

LRESULT CALLBACK AboutProc(HWND hDlg, UINT message,WPARAM wParam,LPARAM lParam){
  switch (message) {
    case WM_INITDIALOG: {
      HWND ctrl = GetDlgItem(hDlg, IDC_CPUINFO);
      const int ncpu = get_num_cpus();
      const int nphyscpu = get_num_physical_cpus();

      char cpumsg[1000];
      wsprintf(cpumsg, "%d %sCPU%s", nphyscpu, (ncpu!=nphyscpu&&ncpu>1)?"physical ":"", cardinal(nphyscpu));

      char physvirtmsg[1000]="";
      if (nphyscpu != ncpu) {
        wsprintf(physvirtmsg, " (%d virtual CPUs.)", ncpu, nphyscpu);
      }

      const char *htmsg;
      if (ht_supported()) {
        if (ht_enabled())
          htmsg = "supported and enabled";
        else
          htmsg = "supported, but disabled";
      } else {
        htmsg = "not supported";
      }

      // report on cpu core map AABB or ABAB or AABBCCDD etc
      char coremapstr[2000]="\n";
      if (cpu_core_map_valid && ht_enabled()) {
        memset(coremapstr, 0, sizeof(coremapstr));

        strcat_s(coremapstr, sizeof(coremapstr), "Core layout: ");

        for (int j = 0; j < sizeof(unsigned long)*8; j++) {
          const unsigned long bit = 1<<j;
          for (int i = 0; i < num_cpu_core_map; i++) {
            if (cpu_core_map[i] & bit) {
              const char txt[2] = { 'A'+i, '\0' };
              strcat_s(coremapstr, sizeof(coremapstr), txt);
              break;
            }
          }
        }
        strcat_s(coremapstr, sizeof(coremapstr), "\n");
      }

      int64 phys_total, virt_total;
      myGetMemoryStatus(&phys_total, NULL, &virt_total, NULL);
      if (isXP) virt_total -= phys_total;
      char membuf[1000]="";
      const double fphys = phys_total / (1024.*1024) + 1;
      double fvirt = virt_total / (1024.*1024) + 1;
      if (fvirt < 0) fvirt = 0;
      sprintf_s(membuf, sizeof(membuf), "%0.f Mb physical RAM, %0.f Mb virtual RAM.", fphys, fvirt);

      char finalbuf[5000]="";
      wsprintf(finalbuf,
        "%s detected.%s\nHyperthreading is %s.\n%s%s",
        cpumsg, physvirtmsg, htmsg, coremapstr, membuf);

      SetWindowText(ctrl, finalbuf);
    }
    return TRUE;

    case WM_COMMAND:
      switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case IDCANCEL:
        case IDOK:
          EndDialog(hDlg, 0);
        break;
      }
    break;
  }
  return 0;
}

static int captured; // 1 during a move
static int sizing, sizing_side;
static int mouse_x, mouse_y;
static POINT screen_mousedown_pos;
static RECT mousedown_wndrect, origwndrect;

static WNDPROC prevWndProc;

// intercept keystrokes and pass em to winamp
LRESULT CALLBACK newWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifndef BMEM_NO_WINAMP
  switch (msg) {
    case WM_CHAR:
    case WM_KEYDOWN:
      if (sendWinampKey(wParam)) return 0; // eat it
    break;
  }
#endif
  return CallWindowProc(prevWndProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MemProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam){
  switch (message) {
    case WM_INITDIALOG: {
      // keep track of our dialog box handle
      memWindow = hDlg;

      SetWindowText(hDlg, APPNAME);

      isXP = isXPOrGreater();

      get_cpu_layout();

      // reposition where we left it
      init_config_storage();

      const int x = get_config_int("xpos", DEF_X);
      const int y = get_config_int("ypos", DEF_Y);
      int w = get_config_int("width", DEF_WIDTH);
      if (w < MINWIDTH) w = MINWIDTH;

      // load some options
      height_div = get_config_int("height_div", NORMAL_DIV);
      fatfont = get_config_int("fatfont", DEF_FATFONT);
      showtext = get_config_int("showtext", TRUE);
      const int was_appbar_mode = get_config_int("appbar_mode", FALSE);
      appbar_side = get_config_int("appbar_side", ABE_TOP);
      use_alpha = get_config_int("use_alpha", FALSE);
      showindcpu = get_config_int("showindividualcpus", TRUE);
      interval = get_config_int("timer_interval", DEFAULT_INTERVAL);
      set_timer_interval(interval);
      Sleep(1);	// just a hunch but want to make sure timers are desynced

      SetTimer(memWindow, TIMER_CPU_READ, CPU_READ_INTERVAL, NULL);
      SetTimer(memWindow, TIMER_TOPMOST, TOPMOST_INTERVAL, NULL);
#ifndef BMEM_NO_SWAPOUT
      SetTimer(memWindow, TIMER_SWAPOUT, SWAPOUT_INTERVAL, NULL);
#endif

      // set our height to be proportional to a static text item

      HWND sizecontroller = GetDlgItem(hDlg, IDC_AUTOHEIGHT);
      GetClientRect(sizecontroller, &origwndrect);	
      // hide size controller
      ShowWindow(sizecontroller, SW_HIDE);

      // figure out our size
      int h = origwndrect.bottom - origwndrect.top + 1;
      h /= 1<<height_div;
      h++;

//--
      // set starting pos and size and AOT
      SetWindowPos(hDlg, HWND_TOPMOST, x, y, w, h, 0);
      keepOnScreen(hDlg);

      if (was_appbar_mode) {
        goAppbar(hDlg, appbar_side);
      }
//--

      // do opacity thingy
      if (use_alpha) {
        SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) | WS_EX_LAYERED);
        setAlpha(hDlg, WINDOW_ALPHA);
      }

      PostMessage(hDlg, WM_SIZE, 0, 0);

#if 0
SystemParametersInfo(SPI_GETWORKAREA, 0, &war, 0);
RECT r = war;
r.top += h/d;
r.top = 0;
SystemParametersInfo(SPI_SETWORKAREA, 0, &r, SPIF_SENDCHANGE);
#endif

      // subclass ourselves to get WM_KEYDOWN
      //prevWndProc = (WNDPROC)SetWindowLong(hDlg, GWL_WNDPROC, (long)newWndProc);
      prevWndProc = (WNDPROC)SetWindowLong(sizecontroller, GWL_WNDPROC, (long)newWndProc);

      return TRUE;
    }
    break;

    case WM_ERASEBKGND: {
    }
    return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hDlg, &ps);

      RECT r;
      GetWindowRect(hDlg, &r);
      r.right -= r.left; r.left = 0;
      r.bottom -= r.top; r.top = 0;

      /* create bitmap into which we'll render */
      const int width = r.right, height = r.bottom;
      HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);	// future optim: cache this
      HDC hMemDC = CreateCompatibleDC(hdc);
      HBITMAP oldbmp = (HBITMAP)SelectObject(hMemDC, bmp);

      // render the background
#define BGBRUSH BLACK_BRUSH
      FillRect(hMemDC, &r, (HBRUSH)GetStockObject(BGBRUSH));

      renderAll(hMemDC, hDlg);

      // blit the completed bitmap
      BitBlt(hdc, 0, 0, r.right, r.bottom, hMemDC, 0, 0, SRCCOPY);

      EndPaint(hDlg, &ps);

      SelectObject(hMemDC, oldbmp);

      // delete graphics objects
      DeleteDC(hMemDC);
      DeleteObject(bmp);

      return 0;
    }
    break;

    case WM_SETCURSOR: {
      if (!appbar_mode) {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hDlg, &p);
        RECT r; GetClientRect(hDlg, &r);
        if (p.x <= r.left + SIZER_WIDTH || p.x >= r.right - SIZER_WIDTH) {
          SetCursor(LoadCursor(NULL, IDC_SIZEWE));
        } else {
          SetCursor(LoadCursor(NULL, IDC_ARROW));
        }
      } else
        SetCursor(LoadCursor(NULL, IDC_ARROW));
      return TRUE;
    }
    break;

    case WM_TIMER: {
      int timerid = wParam;
      switch (timerid) {
        case TIMER_CPU_READ:
          read_cpus();
        break;
        case TIMER_INVALIDATE:
          if (!screensaver_active) InvalidateRect(hDlg, NULL, FALSE);
        break;
        case TIMER_TOPMOST:
          checkScreenSaverRunning();
          if (!popup_showing && !screensaver_active && !detectFullScreenWindow()) {
            // jump to top of screen
            SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
          }
        break;
#ifndef BMEM_NO_SWAPOUT
        case TIMER_SWAPOUT:
          // and try to swap out
          SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
        break;
#endif
      }
    }
    return 0;

    case WM_LBUTTONDOWN: {
      if (appbar_mode) break;

      captured = 1;
      SetCapture(hDlg);

      // save original rect
      GetWindowRect(hDlg, &mousedown_wndrect);

      // save original mouse button down position
      mouse_x = LOWORD(lParam);
      mouse_y = HIWORD(lParam);

      // in screen coords too
      screen_mousedown_pos.x = mouse_x;
      screen_mousedown_pos.y = mouse_y;
      ClientToScreen(hDlg, &screen_mousedown_pos);

      POINT right_p = { mousedown_wndrect.right, 0 };
      ScreenToClient(hDlg, &right_p);
      POINT left_p = { mousedown_wndrect.left, 0 };
      ScreenToClient(hDlg, &left_p);

      if (mouse_x < left_p.x + SIZER_WIDTH) {
        sizing = TRUE;
        sizing_side = -1;
      } else if (mouse_x > right_p.x - SIZER_WIDTH) {
        sizing = TRUE;
        sizing_side = 1;
      } else {
        sizing = FALSE;
        sizing_side = 0;
      }
    }
    break;

    case WM_LBUTTONDBLCLK: {
      height_div ++;
      if (height_div > MAX_HEIGHT_DIV) height_div = NORMAL_DIV;
      set_height_div(height_div);
      RECT curwndrect;
      GetWindowRect(hDlg, &curwndrect);
      const int d = 1<<height_div;
      const int h = /*l8r!!GetAsyncKeyState(VK_SHIFT) ? 2 :*/ (origwndrect.bottom - origwndrect.top)/d+1;
      SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, curwndrect.right - curwndrect.left, h, SWP_NOMOVE | SWP_NOZORDER);
      if (appbar_mode) {
        goAppbar(hDlg, appbar_side);
      } else {
        InvalidateRect(hDlg, NULL, FALSE);
      }
    }
    break;

    case WM_MOUSEMOVE: {
      if (appbar_mode) break;
      if (!captured) break;
      POINT pos;
      GetCursorPos(&pos);//screen coords
      if (!sizing) {
        SetWindowPos(hDlg, NULL, pos.x-mouse_x, pos.y-mouse_y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER);
      } else {
        int origw = mousedown_wndrect.right - mousedown_wndrect.left;
        if (sizing_side == -1) {//left edge
          int deltax = pos.x - screen_mousedown_pos.x;

          int neww = origw - deltax;
          if (neww < MINWIDTH) {
            neww = MINWIDTH;
            pos.x = mousedown_wndrect.right - neww;
          } else if (neww > MAXWIDTH) {
            neww = MAXWIDTH;
          }
          RECT rect;
          GetWindowRect(hDlg, &rect);
          SetWindowPos(hDlg, NULL, pos.x, rect.top, neww, mousedown_wndrect.bottom - mousedown_wndrect.top, SWP_NOZORDER);
        } else if (sizing_side == 1) {// right edge
          POINT p = pos;
          ScreenToClient(hDlg, &p);
          int neww = origw + (p.x-mouse_x);
          if (neww < MINWIDTH) break;
          else if (neww > MAXWIDTH) neww = MAXWIDTH;
          SetWindowPos(hDlg, NULL, 0, 0, neww, mousedown_wndrect.bottom - mousedown_wndrect.top, SWP_NOMOVE | SWP_NOZORDER);
        }
        InvalidateRect(hDlg, NULL, FALSE);
      }
    }
    break;

    case WM_LBUTTONUP: {
      if (appbar_mode) break;
      captured = 0;
      ReleaseCapture();

      keepOnScreen(hDlg);
    }
    break;

    case WM_CONTEXTMENU: {
      HMENU popup = CreatePopupMenu();
      AppendMenu(popup, MFT_STRING, IDC_ABOUT, "&About " APPNAME "...");
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
#ifndef BMEM_NO_TASKMGR
      AppendMenu(popup, MFT_STRING, IDC_TASKMGR, "Launch &Taskmgr...");
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
#endif
#if 0//later
      AppendMenu(popup, MFT_STRING | ((height_div==HALFHEIGHT_DIV) ? MF_CHECKED : 0),
                 IDC_HALFHEIGHT, "&Half height (windowshade)\tDbl click");
      AppendMenu(popup, MFT_STRING | ((height_div==MAX_HEIGHT_DIV) ? MF_CHECKED : 0),
                 IDC_HALFHEIGHT, "&Micro height\tDbl click");
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
#endif
      AppendMenu(popup, MFT_STRING | (showtext ? MF_CHECKED : 0),
                 IDC_SHOWTEXT, "Show &text label");
      AppendMenu(popup, MFT_STRING | (fatfont ? MF_CHECKED : 0) | (showtext ? 0 : MF_GRAYED),
                 IDC_FATFONT, "Use &bold font");
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
      if (get_num_physical_cpus() > 1) {
        AppendMenu(popup, MFT_STRING | (showindcpu ? MF_CHECKED : 0),
                   IDC_SHOWINDIVIDUALCPUS, "Show individual cpu usage graphs");
      }
      AppendMenu(popup, MFT_STRING | (use_alpha ? MF_CHECKED : 0),
                 IDC_USE_ALPHA, "Use desktop alpha transparency");
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
      HMENU dockpopup = CreatePopupMenu();
      AppendMenu(dockpopup, MFT_STRING | ((appbar_side == ABE_TOP && appbar_mode) ? MF_CHECKED : 0)|MFT_RADIOCHECK,
                 IDC_APPBAR_TOP, "top of screen");
      AppendMenu(dockpopup, MFT_STRING | ((appbar_side == ABE_BOTTOM && appbar_mode) ? MF_CHECKED : 0)|MFT_RADIOCHECK,
                 IDC_APPBAR_BOT, "bottom of screen");
#if 0
      AppendMenu(dockpopup, MFT_STRING | ((appbar_side == ABE_LEFT && appbar_mode) ? MF_CHECKED : 0)|MFT_RADIOCHECK,
                 IDC_APPBAR_LEF, "left of screen");
      AppendMenu(dockpopup, MFT_STRING | ((appbar_side == ABE_RIGHT && appbar_mode) ? MF_CHECKED : 0)|MFT_RADIOCHECK,
                 IDC_APPBAR_RIG, "right of screen");
#endif
      AppendMenu(dockpopup, MF_SEPARATOR, -1, NULL);
      AppendMenu(dockpopup, MFT_STRING | (appbar_mode?0:MF_CHECKED)|MFT_RADIOCHECK,
                 IDC_APPBAR_UN, "not docked");
      MENUITEMINFO mii;
      ZeroMemory(&mii, sizeof mii);
      mii.cbSize = sizeof mii;
      mii.fMask = MIIM_DATA | MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
      mii.fType = MFT_STRING;
      mii.fState = MFS_ENABLED;
      mii.hSubMenu = dockpopup;
      mii.dwTypeData = "Dock to";
      mii.cch = STRLEN(mii.dwTypeData);
      InsertMenuItem(popup, GetMenuItemCount(popup), TRUE, &mii);
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);

      AppendMenu(popup, MFT_STRING | (interval == 500 ? MF_CHECKED : 0),
                 IDC_500, "&500 ms (2hz)");
      AppendMenu(popup, MFT_STRING | (interval == 250 ? MF_CHECKED : 0),
                 IDC_250, "&250 ms (4hz)");
      AppendMenu(popup, MFT_STRING | (interval == 100 ? MF_CHECKED : 0),
                 IDC_100, "&100 ms (10hz)");
      AppendMenu(popup, MFT_STRING | (interval == 66 ? MF_CHECKED : 0),
                 IDC_66, "&66 ms (15hz)");
      AppendMenu(popup, MFT_STRING | (interval == 50 ? MF_CHECKED : 0),
                 IDC_50, "&50 ms (20hz) ");
 
      AppendMenu(popup, MF_SEPARATOR, -1, NULL);
      AppendMenu(popup, MFT_STRING, IDC_QUIT, "E&xit");
      POINT p;
      GetCursorPos(&p); 
      popup_showing = 1;
      TrackPopupMenu(popup, TPM_RIGHTBUTTON, p.x, p.y, 0, hDlg, NULL);
      popup_showing = 0;
      DestroyMenu(popup);
    }
    break;

    case WM_COMMAND: {
      switch (GET_WM_COMMAND_ID(wParam, lParam)) {
        case IDC_ABOUT:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hDlg,(DLGPROC)AboutProc);
        break;
#ifndef BMEM_NO_TASKMGR
        case IDC_TASKMGR:
          ShellExecute(NULL, NULL, "taskmgr.exe", NULL, ".", SW_SHOWNORMAL);
        break;
#endif
        case IDC_500: set_timer_interval(500); break;
        case IDC_250: set_timer_interval(250); break;
        case IDC_100: set_timer_interval(100); break;
        case IDC_66: set_timer_interval(66); break;
        case IDC_50: set_timer_interval(50); break;
        case IDC_FATFONT: set_fatfont(!fatfont); break;
        case IDC_SHOWTEXT: set_showtext(!showtext); break;
        case IDC_SHOWINDIVIDUALCPUS: set_showindcpu(!showindcpu); break;
        case IDC_USE_ALPHA:
          set_use_alpha(!use_alpha);
          if (use_alpha) {
            SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) & ~WS_EX_LAYERED);
            SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) | WS_EX_LAYERED);
            setAlpha(hDlg, WINDOW_ALPHA);
          } else {
            SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) & ~WS_EX_LAYERED);
//            SetWindowLong(hDlg, GWL_EXSTYLE, GetWindowLong(hDlg, GWL_EXSTYLE) | WS_EX_LAYERED);
//            setAlpha(hDlg, WINDOW_ALPHA);
          }
        break;
        case IDC_HALFHEIGHT:
          PostMessage(hDlg, WM_LBUTTONDBLCLK, 0, 0);
        break;
        case IDC_APPBAR_TOP:
          goAppbar(hDlg, ABE_TOP);
        break;
        case IDC_APPBAR_BOT:
          goAppbar(hDlg, ABE_BOTTOM);
        break;
        case IDC_APPBAR_LEF:
          goAppbar(hDlg, ABE_LEFT);
        break;
        case IDC_APPBAR_RIG:
          goAppbar(hDlg, ABE_RIGHT);
        break;
        case IDC_APPBAR_UN:
          unAppbar(hDlg);
        break;
        case IDC_QUIT:
          // kill the timer
          DestroyWindow(hDlg);
        return TRUE;
      }
    }
    break;//WM_COMMAND

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x20A
#endif
#ifndef BMEM_NO_WINAMP
    case WM_MOUSEWHEEL:
      sendWinampMsg(message, wParam, lParam);
    break;
#endif

    case WM_SIZE:
      calculateBoxSizes(hDlg);
    break;

    case APPBAR_CALLBACK:
      AppBarCallback(hDlg, wParam, lParam);
    break;

    case WM_DESTROY: {
#if 0
//restore it eh
SystemParametersInfo(SPI_SETWORKAREA, 0, &war, SPIF_SENDCHANGE);
SystemParametersInfo(SPI_SETWORKAREA, 0, NULL, 0);
#endif
      // restore the previous class for the control (just in case)
      HWND sizecontroller = GetDlgItem(hDlg, IDC_AUTOHEIGHT);
      SetWindowLong(sizecontroller, GWL_WNDPROC, (long)prevWndProc);
      prevWndProc = NULL;

      if (appbar_mode) RegisterAccessBar(hDlg, FALSE); // bah bah

      KillTimer(hDlg, TIMER_INVALIDATE);
      memWindow = NULL;

      KillTimer(hDlg, TIMER_CPU_READ);
      KillTimer(hDlg, TIMER_TOPMOST);
#ifndef BMEM_NO_SWAPOUT
      KillTimer(hDlg, TIMER_SWAPOUT);
#endif
    }
    break;
  }
  return FALSE;
}

//----------------------------------------------------------------------
//
// SetPrivilege
//
// Adjusts the token privileges.
//
//----------------------------------------------------------------------
BOOL SetPrivilege( HANDLE hToken, LPCTSTR Privilege, BOOL bEnablePrivilege  )
{
    TOKEN_PRIVILEGES tp;
    LUID luid;
    TOKEN_PRIVILEGES tpPrevious;
    DWORD cbPrevious=sizeof(TOKEN_PRIVILEGES);

    if(!LookupPrivilegeValue( NULL, Privilege, &luid )) return FALSE;

    //
    // first pass.  get current privilege setting
    //
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(
            hToken,
            FALSE,
            &tp,
            sizeof(TOKEN_PRIVILEGES),
            &tpPrevious,
            &cbPrevious
            );

    if (GetLastError() != ERROR_SUCCESS) return FALSE;

    //
    // second pass.  set privilege based on previous setting
    //
    tpPrevious.PrivilegeCount       = 1;
    tpPrevious.Privileges[0].Luid   = luid;

    if(bEnablePrivilege) {
        tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    }
    else {
        tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
            tpPrevious.Privileges[0].Attributes);
    }

    AdjustTokenPrivileges(
            hToken,
            FALSE,
            &tpPrevious,
            cbPrevious,
            NULL,
            NULL
            );

    if (GetLastError() != ERROR_SUCCESS) return FALSE;

    return TRUE;
}

//--------------------------------------------------------------------
//
// LocateNDLLCalls
//
// Loads function entry points in NTDLL
//
//--------------------------------------------------------------------
BOOLEAN LocateNTDLLCalls() {
	if( !(NtQuerySystemInformation = (unsigned long (__stdcall *)(unsigned long,void *,unsigned long,unsigned long *)) GetProcAddress( GetModuleHandle("ntdll.dll"),
			"NtQuerySystemInformation" )) ) {

		return FALSE;
	}

 	if( !(NtSetSystemInformation = (unsigned long (__stdcall *)(unsigned long,void *,unsigned long)) GetProcAddress( GetModuleHandle("ntdll.dll"),
			"NtSetSystemInformation" )) ) {

		return FALSE;
	}
	return TRUE;
}

/* ****************************** WinMain ************************* */

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPTSTR lpCmdLine, int nCmdShow) {
  hInst = hInstance;

  isNT = GetVersion() < 0x80000000;

  // have to be above normal to be able to report on time
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

  if (isNT) {
    HANDLE hToken = NULL;
    // Enable increase quota privilege
    if (!OpenProcessToken(GetCurrentProcess(),
				TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
				&hToken)) {
      isNT = 0;
    } else {
      CloseHandle(hToken);
      LocateNTDLLCalls();
    }
  }

  goDiskIoTrace();

  kernel32 = LoadLibrary("KERNEL32.DLL");
  if (kernel32) {
    globalMemoryStatusEx = (BOOL (__stdcall*)(MEMORYSTATUSEX*))GetProcAddress(kernel32, "GlobalMemoryStatusEx");
    getNativeSystemInfo = (void (__stdcall*)(LPSYSTEM_INFO))GetProcAddress(kernel32, "GetNativeSystemInfo");
    isWow64Process = (BOOL (_stdcall*)(HANDLE, PBOOL))GetProcAddress(kernel32, "IsWow64Process");
  }

  // find system page size
  BOOL x=0;
  SYSTEM_INFO si;
  ZeroMemory(&si, sizeof si);
  if (isWow64Process && (*isWow64Process)(GetCurrentProcess(), &x) && x && getNativeSystemInfo) {
    (*getNativeSystemInfo)(&si);
  } else {
    GetSystemInfo(&si);
  }
  pagesize = si.dwPageSize;

  DialogBox(hInstance, MAKEINTRESOURCE(IDD_BMEM), NULL, (DLGPROC)MemProc);

  stopDiskIoTrace();

  if (user32) FreeLibrary(user32); user32 = NULL;
  if (kernel32) FreeLibrary(kernel32); kernel32 = NULL;
  globalMemoryStatusEx = NULL;
  getNativeSystemInfo = NULL;
  isWow64Process = NULL;

  return 0;
}
