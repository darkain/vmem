// diskio.h: by Brennan Underwood
// this file is part of BMEM, see license.txt for licensing
#ifndef _DISKIO_H
#define _DISKIO_H

int initDiskio();
int diskio_available();

extern volatile int disk_io, net_io;
void goDiskIoTrace();
void stopDiskIoTrace();

#endif
