#ifndef _SYS_UNISTD_H
#define _SYS_UNISTD_H

#include <rtthread.h>

#define _FREAD      0x0001  /* read enabled */
#define _FWRITE     0x0002  /* write enabled */
#define _FAPPEND    0x0008  /* append (writes guaranteed at the end) */
#define _FMARK      0x0010  /* internal; mark during gc() */
#define _FDEFER     0x0020  /* internal; defer for next gc pass */
#define _FASYNC     0x0040  /* signal pgrp when data ready */
#define _FSHLOCK    0x0080  /* BSD flock() shared lock present */
#define _FEXLOCK    0x0100  /* BSD flock() exclusive lock present */
#define _FCREAT     0x0200  /* open with file create */
#define _FTRUNC     0x0400  /* open with truncation */
#define _FEXCL      0x0800  /* error on open if file exists */
#define _FNBIO      0x1000  /* non blocking I/O (sys5 style) */
#define _FSYNC      0x2000  /* do all writes synchronously */
#define _FNONBLOCK  0x4000  /* non blocking I/O (POSIX style) */
#define _FNDELAY    _FNONBLOCK  /* non blocking I/O (4.2 style) */
#define _FNOCTTY    0x8000  /* don't assign a ctty on this open */

#define O_RDONLY    0       /* +1 == FREAD */
#define O_WRONLY    1       /* +1 == FWRITE */
#define O_RDWR      2       /* +1 == FREAD|FWRITE */
#define O_APPEND    _FAPPEND
#define O_CREAT     _FCREAT
#define O_TRUNC     _FTRUNC
#define O_EXCL      _FEXCL
#define O_SYNC      _FSYNC

#define FASYNC		_FASYNC		
#define FNDELAY		_FNDELAY

#endif /* _SYS_UNISTD_H */
