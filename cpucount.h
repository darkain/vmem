#ifndef _BMEM_CPUCOUNT_H
#define _BMEM_CPUCOUNT_H

#define SINGLE_CORE_AND_HT_ENABLED					1
#define SINGLE_CORE_AND_HT_DISABLED					2
#define SINGLE_CORE_AND_HT_NOT_CAPABLE					4
#define MULTI_CORE_AND_HT_NOT_CAPABLE					5
#define MULTI_CORE_AND_HT_ENABLED					6
#define MULTI_CORE_AND_HT_DISABLED					7
#define USER_CONFIG_ISSUE						8

unsigned char CPUCount(unsigned int *, unsigned int *, unsigned int *);

#endif
