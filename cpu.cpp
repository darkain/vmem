#include <windows.h>

#include "config.h"
#include "cpu.h"
#include "cpucount.h"
//#include "ipdh.h"

#include <stdio.h>
#include <Pdh.h>

typedef PDH_STATUS (WINAPI *PDH_OPEN)(LPCTSTR, DWORD_PTR, HQUERY);
typedef PDH_STATUS (WINAPI *PDH_ADD)(HQUERY, LPCTSTR, DWORD_PTR, HCOUNTER);
typedef PDH_STATUS (WINAPI *PDH_CLOSE)(HQUERY);
typedef PDH_STATUS (WINAPI *PDH_FORMAT)(HCOUNTER, DWORD, LPDWORD, PPDH_FMT_COUNTERVALUE);
typedef PDH_STATUS (WINAPI *PDH_COLLECT)(HQUERY);

#pragma comment(lib, "pdh.lib")



void WINAPI GetCounterValues(LPTSTR serverName, LPTSTR objectName,
                             LPTSTR counterList, LPTSTR instanceList)
{
    PDH_STATUS s;

    HQUERY hQuery;

    PDH_COUNTER_PATH_ELEMENTS *cpe = NULL;
    PDH_COUNTER_PATH_ELEMENTS *cpeBeg;

    DWORD  nCounters;
    DWORD  nInstances;

    HCOUNTER *hCounter = NULL;
    HCOUNTER *hCounterPtr;

    char *counterPtr, *instancePtr;

    char szFullPath[MAX_PATH];
    DWORD cbPathSize;
    DWORD   i, j;

    BOOL  ret = FALSE;

    PDH_FMT_COUNTERVALUE counterValue;

    // Scan through the counter names to find the number of counters.
    nCounters = 0;
    counterPtr = counterList;
    while (*counterPtr)
    {
        counterPtr += strlen(counterPtr) + 1;
        nCounters++;
    }

    // Scan through the instance names to find the number of instances.
    nInstances = 0;
    instancePtr = instanceList;
    while (*instancePtr)
    {
        instancePtr += strlen(instancePtr) + 1;
        nInstances++;
    }

    if (!nCounters || !nInstances) return;

    __try
    {
        cpe = (PDH_COUNTER_PATH_ELEMENTS *)HeapAlloc(GetProcessHeap(), 0, 
                    sizeof(PDH_COUNTER_PATH_ELEMENTS) * nCounters * nInstances);
        hCounter = (HCOUNTER *)HeapAlloc(GetProcessHeap(), 0, 
                    sizeof(HCOUNTER) * nCounters * nInstances);

        if (!cpe || !hCounter) __leave;

        // Only do this once to create a query.
        if ((s = PdhOpenQuery(NULL, 0, &hQuery)) != ERROR_SUCCESS)
        {
            fprintf(stderr, "POQ failed %08x\n", s);
            __leave;
        }

        // For each instance name in the list, construct a counter path.
        cpeBeg = cpe;
        hCounterPtr = hCounter;
        for (i = 0, counterPtr = counterList; i < nCounters;
                i++, counterPtr += strlen(counterPtr) + 1)
        {
            for (j = 0, instancePtr = instanceList; j < nInstances;
                    j++,
                    instancePtr += strlen(instancePtr) + 1,
                    cpeBeg++,
                    hCounterPtr++)
            {
                cbPathSize = sizeof(szFullPath);

                cpeBeg->szMachineName = serverName;
                cpeBeg->szObjectName = objectName;
                cpeBeg->szInstanceName = instancePtr;
                cpeBeg->szParentInstance = NULL;
                cpeBeg->dwInstanceIndex = -1;
                cpeBeg->szCounterName = counterPtr;

                if ((s = PdhMakeCounterPath(cpeBeg,
                    szFullPath, &cbPathSize, 0)) != ERROR_SUCCESS)
                {
                    fprintf(stderr,"MCP failed %08x\n", s);
                    __leave;
                }

                // Add the counter path to the query.
                if ((s = PdhAddCounter(hQuery, szFullPath, 0, hCounterPtr))
                        != ERROR_SUCCESS)
                {
                    fprintf(stderr, "PAC failed %08x\n", s);
                    __leave;
                }
            }
        }

        for (i = 0; i < 2; i++)
        {
            Sleep(100);

            // Collect data as often as you need to.
            if ((s = PdhCollectQueryData(hQuery)) != ERROR_SUCCESS)
            {
                fprintf(stderr, "PCQD failed %08x\n", s);
                __leave;
            }

            if (i == 0) continue;

            // Display the performance data value corresponding to each instance.
            cpeBeg = cpe;
            hCounterPtr = hCounter;
            for (i = 0, counterPtr = counterList; i < nCounters;
                    i++, counterPtr += strlen(counterPtr) + 1)
            {
                for (j = 0, instancePtr = instanceList; j < nInstances;
                        j++,
                        instancePtr += strlen(instancePtr) + 1,
                        cpeBeg++,
                        hCounterPtr++)
                {
                    if ((s = PdhGetFormattedCounterValue(*hCounterPtr,
                        PDH_FMT_DOUBLE,
                        NULL, &counterValue)) != ERROR_SUCCESS)
                    {
                        fprintf(stderr, "PGFCV failed %08x\n", s);
                        continue;
                    }
                    printf("%s\\%s\\%s\t\t : [%3.3f]\n",
                        cpeBeg->szObjectName,
                        cpeBeg->szCounterName,
                        cpeBeg->szInstanceName,
                        counterValue.doubleValue);
                }
            }
        }

        // Remove all the performance counters from the query.
        hCounterPtr = hCounter;
        for (i = 0; i < nCounters; i++)
        {
            for (j = 0; j < nInstances; j++)
            {
                PdhRemoveCounter(*hCounterPtr);
            }
        }
    }
    __finally
    {
        HeapFree(GetProcessHeap(), 0, cpe);
        HeapFree(GetProcessHeap(), 0, hCounter);

        // Only do this cleanup once.
        PdhCloseQuery(hQuery);
    }

    return;
}




void WINAPI EnumerateExistingInstances(LPTSTR serverName, LPTSTR objectName)
{
	TCHAR       mszEmptyList[2] = {0,0};  // An empty list contains 2 NULL characters
    LPTSTR      mszInstanceList = NULL;
    LPTSTR      szInstanceName;
    DWORD       cchInstanceList;
    LPTSTR      mszCounterList = NULL;
    DWORD       cchCounterList;
    PDH_STATUS  pdhStatus;

    __try
    {
        mszCounterList = NULL;
        cchCounterList = 0;
        // Refresh the list of performance objects available on the specified
        // computer by making a call to PdhEnumObjects with bRefresh set to TRUE.
        pdhStatus = PdhEnumObjects(NULL, serverName, mszCounterList,
            &cchCounterList, PERF_DETAIL_WIZARD, TRUE);

        mszCounterList = NULL;
        cchCounterList = 0;
        // Determine the required size for the buffer containing counter names
        // and instance names by calling PdhEnumObjectItems.
        cchInstanceList = sizeof(mszEmptyList);
        pdhStatus = PdhEnumObjectItems(NULL, serverName,
            objectName, mszCounterList, 
            &cchCounterList, mszEmptyList,
            &cchInstanceList, PERF_DETAIL_WIZARD, 0);

        if (pdhStatus == ERROR_SUCCESS)
            return;  // The list is empty so do nothing.
//        else if (pdhStatus != PDH_MORE_DATA)
//        {
//            fprintf(stderr, "PEOI failed %08x\n", pdhStatus);
//            return;
//        }

        // Allocate a buffer for the counter names.
        mszCounterList = (LPTSTR)HeapAlloc(GetProcessHeap(),
            HEAP_ZERO_MEMORY, cchCounterList);
        if (!mszCounterList)
        {
            fprintf(stderr, "HA failed %08x\n", GetLastError());
            return;
        }

        // Allocate a buffer for the instance names.
        mszInstanceList = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            cchInstanceList);
        if (!mszInstanceList)
        {
            fprintf(stderr, "HA failed %08x\n", GetLastError());
            return;
        }

        __try
        {
            // Enumerate to get the list of Counters and Instances provided by
            // the specified object on the specified computer.
            pdhStatus = PdhEnumObjectItems(NULL, serverName,
                objectName, mszCounterList, 
                &cchCounterList, mszInstanceList,
                &cchInstanceList, PERF_DETAIL_WIZARD, 0);
            if (pdhStatus != ERROR_SUCCESS)
            {
                fprintf(stderr, "PEOI failed %08x\n", pdhStatus);
                return;
            }

            // Display the items from the buffer.
            szInstanceName = mszInstanceList;
            while (*szInstanceName)
            {
                printf("%s\n", szInstanceName);
                szInstanceName += strlen(szInstanceName) + 1;
            }

            GetCounterValues(serverName, objectName, mszCounterList,
                mszInstanceList);
        }
        __finally
        {
        }

    }
    __finally
    {
        // Free the buffers.
        if (mszInstanceList) HeapFree(GetProcessHeap(), 0, mszInstanceList);
        if (mszCounterList) HeapFree(GetProcessHeap(), 0, mszCounterList);
    }

    return;   
}





int pdh_err = 0;
/*
PDH_OPEN PdhOpenQuery;
PDH_ADD PdhAddCounter;
PDH_CLOSE PdhCloseQuery;
PDH_FORMAT PdhGetFormattedCounterValue;
PDH_COLLECT PdhCollectQueryData;
*/
HMODULE PdhLib;
HQUERY   PdhQuery[256];
HCOUNTER PdhCounter[256];

HQUERY ReadyboostSizeQuery;
HCOUNTER ReadyBoostSizeCounter;

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

  if (ret > MAX_CPUS) return MAX_CPUS;

  return ret;
}

bool ht_supported() {
  return htsupported;
}

bool ht_enabled() {
  return htenabled;
}


static void init_pdh() {
	/*
  if (!PdhLib) {
    PdhLib=LoadLibrary("pdh.dll");
    if (PdhLib) {
      PdhOpenQuery = (PDH_OPEN)GetProcAddress (PdhLib,"PdhOpenQueryA");
      PdhAddCounter = (PDH_ADD)GetProcAddress(PdhLib, "PdhAddCounterA");
      PdhCloseQuery = (PDH_CLOSE)GetProcAddress (PdhLib,"PdhCloseQuery");
      PdhGetFormattedCounterValue = (PDH_FORMAT)GetProcAddress(PdhLib, "PdhGetFormattedCounterValue");
      PdhCollectQueryData = (PDH_COLLECT)GetProcAddress(PdhLib, "PdhCollectQueryData");

      if (!PdhOpenQuery || !PdhAddCounter || !PdhCloseQuery || !PdhGetFormattedCounterValue || !PdhCollectQueryData)
        pdh_err = 33;	// magic num to indicate error
    } else pdh_err = 99;	// magic num to indicate error
  }
  */
}

int get_cpu_usage(int cpu) {
  init_cpu();

  if (cpu < 0 || cpu >= num_cpus) return -1;
  int ret=50;
  DWORD Reserved,dataType,dataLen=4096;

  OSVERSIONINFO os = { sizeof(OSVERSIONINFO), };
  GetVersionEx(&os);
  if (os.dwPlatformId != VER_PLATFORM_WIN32_NT) {
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
  } else {
    if (pdh_err) return pdh_err;
    init_pdh();
    if (!PdhQuery[cpu]) if (PdhOpenQuery( NULL, 0, &PdhQuery[cpu] ) != ERROR_SUCCESS) return 73;

	PDH_STATUS err = 0;
    if (!PdhCounter[cpu]) {
      char str[128] = "";
      wsprintf(str, "\\Processor Information(0,%d)\\%% Processor Time", cpu);
      err = PdhAddCounter( PdhQuery[cpu], str, 0, &PdhCounter[cpu] );
//	  EnumerateExistingInstances(NULL, "Processor");
    }

    PDH_FMT_COUNTERVALUE fmtValue;
    err = PdhCollectQueryData(PdhQuery[cpu]);
    err = PdhGetFormattedCounterValue(PdhCounter[cpu], PDH_FMT_LONG, 0, &fmtValue);
    ret = fmtValue.longValue;
  }

  return ret;
}

#if 0//doesn't work, don't know why (yet)
int get_readyboost_size() {
  OSVERSIONINFO os = { sizeof(OSVERSIONINFO), };
  GetVersionEx(&os);
  if (os.dwPlatformId != VER_PLATFORM_WIN32_NT) return 0;

  if (pdh_err) return pdh_err;
  init_pdh();

  if (!ReadyboostSizeQuery) {
    const PDH_STATUS r = PdhOpenQuery(NULL, 0, &ReadyboostSizeQuery);
    if (r != ERROR_SUCCESS) {
      pdh_err = 73;
      return 0;
    }
  }

  if (!ReadyBoostSizeCounter) {
#if 0
      char str[4096];
      wsprintf(str, "\\Processor(%d)\\%% Processor Time", cpu);
      PdhAddCounter( PdhQuery[cpu], str, 0, &PdhCounter[cpu] );
#else
#define RBQ "\\ReadyBoost Cache(*)\\Total cache size bytes"
//#define RBQ "\\Processor(_Total)\\% Processor Time"
      const PDH_STATUS r = PdhAddCounter(ReadyboostSizeQuery, RBQ, 0, &ReadyBoostSizeCounter);
      if (r != ERROR_SUCCESS) {
        pdh_err = 737;
        return 0;
      }
#endif
    }

    PDH_FMT_COUNTERVALUE fmtValue;
    PdhCollectQueryData(ReadyboostSizeQuery);
    PdhGetFormattedCounterValue(ReadyBoostSizeCounter, PDH_FMT_LONG, 0, &fmtValue);

  int r = fmtValue.longValue * 1024*1024;

    return r;
}
#endif

// ---- begin GetLogicalProcessorInformation ----

typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);

unsigned long cpu_core_map[MAX_CPUS];
int num_cpu_core_map;
bool cpu_core_map_valid;

static int countBits(unsigned long x) {
  int ret = 0;
  for (int i = 0; i < sizeof(x)*8; i++) {
    const unsigned long bit = 1<<i;
    if (x & bit) ret++;
  }
  return ret;
}

void get_cpu_layout() {
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
  DWORD returnLength = 0;
  DWORD byteOffset = 0;

  LPFN_GLPI glpi = (LPFN_GLPI) GetProcAddress(
                            GetModuleHandle(TEXT("kernel32")),
                            "GetLogicalProcessorInformation");
  if (NULL == glpi) {
    return;
  }

  for (;;) {
    DWORD rc = glpi(buffer, &returnLength);

    if (FALSE == rc) {
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        if (buffer) free(buffer);

        buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);

        if (NULL == buffer) {
          return;
        }
      } else {
        if (buffer) free(buffer);
        return;
      }
    } else {
      break;
    }
  }

  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;

  while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
    switch (ptr->Relationship) {
#if 0
      case RelationNumaNode:
        // Non-NUMA systems report a single record of this type.
      break;
#endif
      case RelationProcessorCore: {
        // A hyperthreaded core supplies more than one logical processor.
        if (ptr->ProcessorCore.Flags)
          cpu_core_map[num_cpu_core_map++] = ptr->ProcessorMask;
      }
      break;
#if 0
      case RelationCache:
        // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
        Cache = &ptr->Cache;
        if (Cache->Level == 1) {
        } else if (Cache->Level == 2) {
        } else if (Cache->Level == 3) {
        }
      break;

      case RelationProcessorPackage:
        // Logical processors share a physical package.
      break;
#endif
      default:
      break;
    }
    byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    ptr++;
  }

  free(buffer);

  // ensure we got exact same # of core maps as physical processors
  if (num_cpu_core_map != get_num_physical_cpus()) return;

  // sanity check core map
  int num_cores=0;
  for (int i = 0; i < num_cpu_core_map; i++) {
    const unsigned long map = cpu_core_map[i];
    if (map == 0) return;	// no blanks
    num_cores += countBits(map);
  }
  // ensure we got exact same # of core map entries as virtual processors
  if (num_cores != get_num_cpus()) return;

  cpu_core_map_valid = true;
}
