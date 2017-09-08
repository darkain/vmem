#ifndef _BMEM_CONFIG_H
#define _BMEM_CONFIG_H

#define BMEM_NO_TASKMGR

// define to compile out winamp key/mousewheel forwarding
#define BMEM_NO_WINAMP

// define to keep bmem from trying to use the least possible RAM
#define BMEM_NO_SWAPOUT

#define CPU_READ_INTERVAL	50	// ms
#define TOPMOST_INTERVAL	15*1000	// ms
#define SWAPOUT_INTERVAL	30*1000	// ms

#define MAX_CPUS	32	 // >32 will require new win64 apis

//CUT was #define NCPUHISTORY 24
#define NCPUHISTORY 48
#define NMHZHISTORY 10
#define CPU_INTERVAL 1.5	// in seconds, must fit into NCPUHISTORY wrt CPU_READ_INTERVAL

#endif
