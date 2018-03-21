#ifndef TIMER_FD_H__
#define TIMER_FD_H__

/**
 * Pipe Device
 */
#include <rtthread.h>
#include <rtdevice.h>

#include <sys/time.h>
#include <sys/timespec.h>
#include <clock_time.h>

#define TIMERFD_TIMER_NAMES     "timerfd"

/* Bits to be set in the FLAGS parameter of `timerfd_create'.  */
enum
{
    TFD_CLOEXEC = 02000000,
#define TFD_CLOEXEC TFD_CLOEXEC
    TFD_NONBLOCK = 00004000
#define TFD_NONBLOCK TFD_NONBLOCK
};

/* Bits to be set in the FLAGS parameter of `timerfd_settime'.  */                                        
enum                                                                                                      
{                                                                                                       
    TFD_TIMER_ABSTIME = 1 << 0                                                                            
#define TFD_TIMER_ABSTIME TFD_TIMER_ABSTIME                                                               
};                                                                                                      

int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
int timerfd_gettime(int fd, struct itimerspec *curr_value);

#endif /* TIMER_FD_H__ */
