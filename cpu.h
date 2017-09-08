#ifndef _BMEM_CPU_H
#define _BMEM_CPU_H

int get_num_cpus();
int get_num_physical_cpus();

bool ht_supported();
bool ht_enabled();	// hyperthreading is enabled

int get_cpu_usage(int cpu);

#endif
