#include <rtthread.h>
#include <rtdevice.h>
#include <finsh.h>
#include <dfs_posix.h>
#include <dfs_select.h>

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
	fd_set selectFDs;
	int fdmax;
    int fd;
    struct timeval t;
    int timeout_cnt = 200;
    rt_uint32_t times;
    int read_size;
    int ret_size;

    if(rt_sftimer_create(TIMER, timer_timeout_cb, \
        RT_NULL, 2*RT_TICK_PER_SECOND, O_RDWR) == RT_NULL)
    {
        rt_kprintf("rt_sftimer_create failed.\n");
        return -1;
    }
    
    fd = open(DEV_TIMER, O_RDWR | O_NONBLOCK, 0);
    if(fd==-1)
    {
        rt_kprintf("open %s failed.\n", DEV_TIMER);
        return -1;
    }
    else
    {
    	printf("sftimer fd is %d.\n", fd);
    }
    fdmax = fd;
    t.tv_sec = 0;
    t.tv_usec = 200000;
    
    ioctl(fd, RT_TIMER_CTRL_SET_PERIODIC, RT_NULL);
    ioctl(fd, SFTIMER_CTRL_START, RT_NULL);
    
    while(1)
    {
        FD_ZERO(&selectFDs);
        FD_SET (fd, &selectFDs);
        // First use select to find activity on the sockets
        if ((ret_size = select (fdmax + 1, &selectFDs, NULL, NULL, &t))< 0)
        {
            if (errno != EINTR)
            {
                rt_kprintf("select error\n");
                break;
            }
            continue;
        }
        else if(ret_size == 0)
        {
            /* timeout */
            timeout_cnt--;
            if(timeout_cnt == 0)
            {
            	rt_kprintf("select timeout.\n");
                break;
            }
            rt_kprintf("select with ret_size = 0.\n");
        }
        else
        {
			// Then process this activity
			if (FD_ISSET(fd, &selectFDs))
			{
				read_size = read(fd, &times, sizeof(rt_uint32_t));
				if(read_size<=0)
				{
					rt_kprintf("error.\n");
				}
				else
				{
					rt_kprintf("read times is %d.\n", times);
				}
			}
        }
    }

    rt_thread_delay(10*RT_TICK_PER_SECOND);

    ioctl(fd, SFTIMER_CTRL_STOP, RT_NULL);
    
    close(fd);
    
    return 0;
}

FINSH_FUNCTION_EXPORT(sftimer, "Test hardware timer");
