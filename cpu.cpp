#include <windows.h>

#include "cpu.h"
#include "cpucount.h"
#include "ipdh.h"

// from nsbeep
typedef PDH_STATUS (WINAPI *PDH_OPEN)(LPCTSTR, DWORD_PTR, HQUERY);
typedef PDH_STATUS (WINAPI *PDH_ADD)(HQUERY, LPCTSTR, DWORD_PTR, HCOUNTER);
typedef PDH_STATUS (WINAPI *PDH_CLOSE)(HQUERY);
typedef PDH_STATUS (WINAPI *PDH_FORMAT)(HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
typedef PDH_STATUS (WINAPI *PDH_COLLECT)(HQUERY);

int pdh_err;
PDH_OPEN PdhOpenQuery;
PDH_ADD PdhAddCounter;
PDH_CLOSE PdhCloseQuery;
PDH_FORMAT PdhGetFormattedCounterValue;
PDH_COLLECT PdhCollectQueryData;
HMODULE PdhLib;
HQUERY   PdhQuery[256];
HCOUNTER PdhCounter[256];

static int num_cpus;
static bool htsupported, htenabled;

static void init_cpu() {
  static int inited;
  if (inited) return;
  inited = 1;

  SYSTEM_INFO si;
  ZeroMemory(&si, sizeof si);
  GetSystemInfo(&si);
  num_cpus = si.dwNumberOfProcessors;
}

int get_num_cpus() {
  init_cpu();
  return num_cpus;
}

int get_num_physical_cpus() {
  static int ret=0;
  if (ret != 0) return ret;

  unsigned int  TotAvailLogical   = 0,  // Number of available logical CPU per CORE
                TotAvailCore  = 0,      // Number of available cores per physical processor
                PhysicalNum   = 0;      // Total number of physical processors
  unsigned char StatusFlag = CPUCount(&TotAvailLogical, &TotAvailCore, &PhysicalNum);
#define MAX(a,b) ((a)>(b)?(a):(b))
  //was ret = PhysicalNum;
  ret = MAX(PhysicalNum, TotAvailCore);
  htsupported = htenabled = false;
  switch (StatusFlag) {
    case SINGLE_CORE_AND_HT_ENABLED:
    case MULTI_CORE_AND_HT_ENABLED:
      htsupported = true;
      htenabled = true;
    break;
    case SINGLE_CORE_AND_HT_DISABLED:
    case MULTI_CORE_AND_HT_DISABLED:
      htsupported = true;
    break;
    case SINGLE_CORE_AND_HT_NOT_CAPABLE:
    case MULTI_CORE_AND_HT_NOT_CAPABLE:
      // no hyperthreading at all, just trust OS count
      ret = get_num_cpus();
    break;
  }

  return ret;
}

bool ht_supported() {
  return htsupported;
}

bool ht_enabled() {
  return htenabled;
}

int get_cpu_usage(int cpu) {
  init_cpu();

  if (cpu < 0 || cpu >= num_cpus) return -1;
  int ret=50;
  DWORD Reserved,dataType,dataLen=4096;

  OSVERSIONINFO os = { sizeof(OSVERSIONINFO), };
  GetVersionEx(&os);
  if (pdh_err) return 69;
  if (os.dwPlatformId == VER_PLATFORM_WIN32_NT) {
    if (!PdhLib) PdhLib=LoadLibrary("pdh.dll");
    if (PdhLib) {
      if (!PdhOpenQuery) {
        PdhOpenQuery = (PDH_OPEN)GetProcAddress (PdhLib,"PdhOpenQueryA");
        PdhAddCounter = (PDH_ADD)GetProcAddress(PdhLib, "PdhAddCounterA");
        PdhCloseQuery = (PDH_CLOSE)GetProcAddress (PdhLib,"PdhCloseQuery");
        PdhGetFormattedCounterValue = (PDH_FORMAT)GetProcAddress(PdhLib, "PdhGetFormattedCounterValue");
        PdhCollectQueryData = (PDH_COLLECT)GetProcAddress(PdhLib, "PdhCollectQueryData");
      }
      if (PdhOpenQuery && PdhAddCounter && PdhCloseQuery && PdhGetFormattedCounterValue && PdhCollectQueryData) {

        if (PdhQuery[cpu] || PdhOpenQuery( NULL, 0, &PdhQuery[cpu] ) == ERROR_SUCCESS) {
          if (!PdhCounter[cpu]) {
            char blef[4096];
            wsprintf(blef, "\\Processor(%d)\\%% Processor Time", cpu);
            PdhAddCounter( PdhQuery[cpu], blef, 0, &PdhCounter[cpu] );
          }
	      
          PDH_FMT_COUNTERVALUE fmtValue;
          PdhCollectQueryData( PdhQuery[cpu] );
          PdhGetFormattedCounterValue(PdhCounter[cpu], PDH_FMT_LONG, 0, &fmtValue);
          ret = fmtValue.longValue;
        }
      } else pdh_err=1;
    } else pdh_err=1;
  } else {
    HKEY regperfcpu;
    HKEY startregperfcpu;
    char data[4096];
    
    if (RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StatData", &regperfcpu) == ERROR_SUCCESS) {
      if (RegOpenKey(HKEY_DYN_DATA, "PerfStats\\StartStat", &startregperfcpu) == ERROR_SUCCESS) {
        if (RegQueryValueEx(startregperfcpu, "KERNEL\\CPUUsage", &Reserved, &dataType, (UCHAR *) data, &dataLen) == ERROR_SUCCESS &&
            RegQueryValueEx(regperfcpu, "KERNEL\\CPUUsage", &Reserved, &dataType, (UCHAR *) data, &dataLen) == ERROR_SUCCESS)
          ret=*data;
        // got it.
        RegCloseKey(startregperfcpu);
      }
      RegCloseKey(regperfcpu);
    }
  }

  return ret;
}
