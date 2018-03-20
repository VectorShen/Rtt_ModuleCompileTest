#include <rtthread.h>
#include <rtdevice.h>
#include <finsh.h>
#include <dfs_posix.h>

#define TIMER       "sf_timer0"
#define DEV_TIMER   "/dev/sf_timer0"

void timer_timeout_cb(void* parameter)
{
    rt_sftimer_timeout_func_param_t* sf_timeout_func_param = \
        (rt_sftimer_timeout_func_param_t*)parameter;
    rt_uint32_t* timeout_times = sf_timeout_func_param->timeout_times;
    (*timeout_times)++;

    rt_kprintf("current timeout_times is %d\n", *timeout_times);
    return;
}

int sftimer(void)
{
    int fd;
    if(rt_sftimer_create(TIMER, timer_timeout_cb, \
        RT_NULL, RT_TICK_PER_SECOND, O_RDWR) == RT_NULL)
    {
        rt_kprintf("rt_sftimer_create failed.\n");
        return -1;
    }
    
    fd = open(DEV_TIMER, O_RDWR, 0);
    if(fd==-1)
    {
        rt_kprintf("open %s failed.\n", DEV_TIMER);
        return -1;
    }
    else
    {
    	printf("sftimer fd is %d.\n", fd);
    }
    
    ioctl(fd, RT_TIMER_CTRL_SET_PERIODIC, RT_NULL);
    ioctl(fd, SFTIMER_CTRL_START, RT_NULL);
    
    rt_thread_delay(10*RT_TICK_PER_SECOND);

    ioctl(fd, SFTIMER_CTRL_STOP, RT_NULL);
    
    close(fd);
    
    return 0;
}

FINSH_FUNCTION_EXPORT(sftimer, "Test hardware timer");
