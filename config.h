#ifndef _BMEM_CONFIG_H
#define _BMEM_CONFIG_H

#define BMEM_NO_TASKMGR

// define to compile out winamp key/mousewheel forwarding
//#define BMEM_NO_WINAMP

// define to keep bmem from trying to use the least possible RAM
//#define BMEM_NO_SWAPOUT

#define CPU_READ_INTERVAL	50	// ms
#define TOPMOST_INTERVAL	15*1000	// ms
#define SWAPOUT_INTERVAL	30*1000	// ms

#endif
