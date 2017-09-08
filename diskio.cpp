// diskio.cpp: by Brennan Underwood
// this file is part of BMEM, see license.txt for licensing

// this file implements disk IO event trapping

#define __TRACE_W2K_COMPATIBLE

#include <windows.h>
#include <windowsx.h>
#include <wmistr.h>
#include <evntrace.h>

#include "diskio.h"

volatile int disk_io, net_io;
static TRACEHANDLE traceHandle=NULL, opentrace=NULL;
static unsigned long threadid=0;
static volatile int dienow;

extern int isNT;

#define MAXSTR 512
static char propsmem[sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAXSTR];
static EVENT_TRACE_PROPERTIES *props=(EVENT_TRACE_PROPERTIES*)propsmem;

#if 0
int initDiskio() {
  if (advapi) return 1;
  HINSTANCE advapi = LoadLibrary("ADVAPI32.DLL");
  if (advapi == NULL) return 0;
  opentrace = GetProcAddress
...
}

void shutdownDiskio() {
}

int diskio_available() {
  initDiskio();
}
#endif

VOID WINAPI tracecb(EVENT_TRACE *trace) {
  disk_io = 1;
//OutputDebugString("DISKIO");
}

unsigned long last;

ULONG WINAPI bufcb(EVENT_TRACE_LOGFILE *logFile) {
//OutputDebugString("BUFCB");
  if (dienow)
    return FALSE;
  return TRUE;
}

static void mystrcpy(char *dest, const char *src) {
   while (*src) *dest++ = *src++;
}

static void _start() {
//DEFINE_GUID ( /* 9e814aad-3204-11d2-9a82-006008a86939 */
  GUID mySystemTraceControlGuid = {
    0x9e814aad,
    0x3204,
    0x11d2,
  {    0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39 } };

    int SizeNeeded = sizeof(EVENT_TRACE_PROPERTIES) + 2 * MAXSTR;

    props->Wnode.BufferSize = SizeNeeded;
    props->Wnode.Guid = mySystemTraceControlGuid;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props->EnableFlags = EVENT_TRACE_FLAG_DISK_IO;/*|EVENT_TRACE_FLAG_NETWORK_TCPIP;*/

    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    props->LogFileNameOffset = props->LoggerNameOffset + MAXSTR;

    char *LoggerName = (LPTSTR)((char*)props + props->LoggerNameOffset);
    char *LogFileName = (LPTSTR)((char*)props + props->LogFileNameOffset);

    lstrcpy(LoggerName, KERNEL_LOGGER_NAME);

    traceHandle = StartTrace(&opentrace, KERNEL_LOGGER_NAME, props);
    if ((HANDLE)traceHandle == INVALID_HANDLE_VALUE) {
//OutputDebugString("ER222222RPR");
    }

    EVENT_TRACE_LOGFILE log;
    ZeroMemory(&log, sizeof log);
    log.LoggerName = KERNEL_LOGGER_NAME;
    log.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    log.EventCallback = tracecb;
    log.BufferCallback = bufcb;
    opentrace = OpenTrace(&log);
    if ((HANDLE)opentrace == INVALID_HANDLE_VALUE) {
//OutputDebugString("ERRPR");
    }
//OutputDebugString("STARTED");
}

void _stop() {
  CloseTrace(opentrace);
  opentrace = NULL;
  // stop the trace
  ControlTrace(traceHandle, NULL, props, EVENT_TRACE_CONTROL_STOP);
  traceHandle = NULL;
}

DWORD WINAPI ThreadProc(void *param) {
  _start();
  do {
    ULONG ret = ProcessTrace(&opentrace, 1, NULL, NULL);
    if (ret != ERROR_SUCCESS) {
//OutputDebugString("ERROR PROCESS");
//ULONG bleh = GetLastError();
//if (ret == ERROR_BAD_LENGTH) OutputDebugString("badlen");
//if (ret == ERROR_INVALID_HANDLE) OutputDebugString("invhandle");
//if (ret == ERROR_NOACCESS) OutputDebugString("NOACCESS");
////__asm int 3;
    }
    Sleep(1);
  } while (!dienow);
  _stop();
  return 1;
}

void goDiskIoTrace() {
  CreateThread(NULL, 0, ThreadProc, 0, 0, &threadid);
}

void stopDiskIoTrace() {
  dienow = 1;
  // we don't wait around for the thread to exit since it's blocked until
  // the next Disk IO event happens, after which it will die
}
