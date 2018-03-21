/*
 * File      : timerfd.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2012-2017, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 */
#include <rthw.h>
#include <rtdevice.h>
#include <stdint.h>

#include <dfs_file.h>
#include <dfs_posix.h>
#include <dfs_poll.h>
#include <ipc/timerfd.h>

static int timerfd_timer_nums = 0;

static void timer_timeout_cb(void* parameter)
{
    rt_sftimer_timeout_func_param_t* sf_timeout_func_param = \
        (rt_sftimer_timeout_func_param_t*)parameter;
    rt_uint32_t* timeout_times = sf_timeout_func_param->timeout_times;
    (*timeout_times)++;

    rt_kprintf("current timeout_times is %d\n", *timeout_times);
    return;
}

int timerfd_create(int clockid, int flags)
{
	char timer_name_buf[10];
	char timer_dev_buf[20];
	int fd;

    switch(clockid)
    {
        case CLOCK_REALTIME:
        case CLOCK_MONOTONIC:

            break;
        default:
            return -1;
    }
    
    switch(flags)
    {
        case TFD_CLOEXEC:
        case TFD_NONBLOCK:
        
            break;
        default:
            break;;
    }

    sprintf(timer_name_buf, "%s%d", TIMERFD_TIMER_NAMES, timerfd_timer_nums++);
    if(rt_sftimer_create(timer_name_buf, timer_timeout_cb,
    	RT_NULL, RT_TICK_PER_SECOND, O_RDWR) == RT_NULL)
    {
        rt_kprintf("rt_sftimer_create failed.\n");
    	return -1;
    }

    sprintf(timer_dev_buf, "/dev/%s", timer_name_buf);
    fd = open(timer_dev_buf, O_RDWR | O_NONBLOCK, 0);
    if(fd==-1)
    {
        rt_kprintf("open %s failed.\n", timer_dev_buf);
        return -1;
    }
    else
    {
    	printf("sftimer fd is %d.\n", fd);
    }
    return fd;
}

int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
{
	rt_int32_t new_timer_ticks;
	rt_int32_t new_timer_ms;
	rt_int32_t new_interval_ms;

	switch(flags)
	{
		case TFD_TIMER_ABSTIME:

			break;
		case 0:

			break;
	}

	if(new_value != RT_NULL)
	{
		if(new_value->it_interval.tv_sec != 0 || new_value->it_interval.tv_nsec != 0)
		{
			ioctl(fd, RT_TIMER_CTRL_SET_PERIODIC, RT_NULL);
			new_timer_ms = new_value->it_interval.tv_sec*1000 + \
				new_value->it_interval.tv_nsec/1000000;
			new_interval_ms = new_value->it_value.tv_sec*1000 + \
				new_value->it_value.tv_nsec/1000000;
			new_timer_ticks = rt_tick_from_millisecond(new_timer_ms);
			ioctl(fd, RT_TIMER_CTRL_SET_TIME, &new_timer_ticks);
		    ioctl(fd, SFTIMER_CTRL_START, RT_NULL);
		}
		else if(new_value->it_value.tv_sec == 0 && new_value->it_value.tv_nsec == 0)
		{
		    ioctl(fd, SFTIMER_CTRL_STOP, RT_NULL);
		}
	}

	if(old_value != RT_NULL)
	{

	}
	return 0;
}

int timerfd_gettime(int fd, struct itimerspec *curr_value)
{
	rt_int32_t timer_ticks;
	rt_int32_t secs;
	rt_int32_t ms;

	ioctl(fd, RT_TIMER_CTRL_GET_TIME, &timer_ticks);
	if(timer_ticks == 0)
	{
		rt_kprintf("The timer of fd=%d is removed.\n", fd);
	}
	else
	{
		ms = rt_millisecond_from_tick(timer_ticks);
		curr_value->it_value.tv_sec = ms / 1000;
		curr_value->it_value.tv_nsec = (ms - curr_value->it_value.tv_sec) * 1000000;
	}
	return 0;
}
