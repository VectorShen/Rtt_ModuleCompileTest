/*
 * File      : sftimer.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2012, RT-Thread Development Team
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
 * Date           Author         Notes
 * 2015-08-31     heyuanjie87    first version
 */

#include <rtthread.h>
#include <rtdevice.h>

#if defined(RT_USING_POSIX)
#include <dfs_file.h>
#include <dfs_posix.h>
#include <dfs_poll.h>

static int sftimer_fops_open(struct dfs_fd *fd)
{
    rt_device_t device;
    rt_sftimer_t *timer;

    timer = (rt_sftimer_t *)fd->data;
    if (!timer) 
        return -1;

    fd->type = FT_USER;

    device = &(timer->parent);
    rt_mutex_take(&(timer->lock), RT_WAITING_FOREVER);

    if (device->ref_count == 0)
    {
        timer->timer = rt_timer_create(timer->data->name,
            timer->data->timeout,
            timer->data->parameter,
            timer->data->time,
            timer->data->flag);
        if(timer->timer == RT_NULL)
        {
            rt_mutex_release(&(timer->lock));
            return -1;
        }
    }

    device->ref_count++;

    rt_mutex_release(&(timer->lock));

    return 0;
}

static int sftimer_fops_close(struct dfs_fd *fd)
{
    rt_device_t device;
    rt_sftimer_t *timer;

    timer = (rt_sftimer_t *)fd->data;
    if (!timer) 
        return -1;

    device = &(timer->parent);
    rt_mutex_take(&(timer->lock), RT_WAITING_FOREVER);

    if (device->ref_count == 1)
    {
        rt_timer_delete(timer->timer);
        timer->timer = RT_NULL;
    }
    device->ref_count --;

    rt_mutex_release(&(timer->lock));

    return 0;
}

static int sftimer_fops_ioctl(struct dfs_fd *fd, int cmd, void *args)
{
    rt_sftimer_t *timer;
    int ret = 0;

    timer = (rt_sftimer_t *)fd->data;

    switch (cmd)
    {
        case SFTIMER_CTRL_START:
            rt_timer_start(timer->timer);
            break;
        case SFTIMER_CTRL_STOP:
            rt_timer_stop(timer->timer);
            break;
        case SFTIMER_CTRL_TIMEOUT_TIMES_GET:
            *((rt_uint32_t*)args) = timer->timeout_times;
            break;
        default:
            rt_timer_control(timer->timer, cmd, args);
            break;
    }

    return ret;
}

static int sftimer_fops_read(struct dfs_fd *fd, void *buf, size_t count)
{
    int res = 0;
    rt_sftimer_t *timer = (rt_sftimer_t *)fd->data;
    RT_ASSERT(timer != RT_NULL);

    if(fd->flags & O_NONBLOCK)
        res = -EAGAIN;
    else
    {
        *(rt_uint32_t*)buf = timer->timeout_times;
        res = sizeof(rt_uint32_t);
        if(timer->timeout_times>0)
        {
            timer->timeout_times = 0;
        }
    }

    return res;
}

static int sftimer_fops_write(struct dfs_fd *fd, const void *buf, size_t count)
{
    int ret = 0;

    return ret;
}

static int sftimer_fops_poll(struct dfs_fd *fd, rt_pollreq_t *req)
{
    int mask = 0;
    int flags = 0;
    rt_device_t device;
    rt_sftimer_t *timer;

    timer = (rt_sftimer_t *)device;
    RT_ASSERT(timer != RT_NULL);

    device = &(timer->parent);

    flags = fd->flags & O_ACCMODE;
    if(flags == O_RDONLY || flags == O_RDWR)
    {
        rt_poll_add(&(device->wait_queue), req);
        if(timer->timeout_times > 0)
        {
            mask |= POLLIN;
        }
    }    

    return mask;
}

static const struct dfs_file_ops sftimer_fops =
{
    sftimer_fops_open,
    sftimer_fops_close,
    sftimer_fops_ioctl,
    sftimer_fops_read,
    sftimer_fops_write,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    sftimer_fops_poll,
};
#endif /* end of RT_USING_POSIX */

rt_sftimer_t *rt_sftimer_create(const char *name, 
    void (*timeout)(void *parameter),
    void* parameter,
    rt_tick_t time,
    rt_uint8_t flag)
{
    rt_sftimer_t *sftimer;
    rt_sftimer_device_data_t *rt_sftimer_device_data;
    rt_device_t dev;

    sftimer = rt_malloc(sizeof(rt_sftimer_t));
    if (sftimer == RT_NULL) 
    {
        return RT_NULL;
    }

    rt_sftimer_device_data = rt_malloc(sizeof(rt_sftimer_device_data_t));
    if(rt_sftimer_device_data == RT_NULL)
    {
        rt_free(sftimer);
        return RT_NULL;
    }
    rt_memset(rt_sftimer_device_data, 0, sizeof(rt_sftimer_device_data_t));
    rt_sftimer_device_data->name = name;
    rt_sftimer_device_data->timeout = timeout;
    rt_sftimer_device_data->parameter = (rt_sftimer_timeout_func_param_t*)rt_malloc(sizeof(rt_sftimer_timeout_func_param_t));
    if(rt_sftimer_device_data->parameter == RT_NULL)
    {
        rt_free(sftimer);
        rt_free(rt_sftimer_device_data);
        return RT_NULL;
    }
    rt_sftimer_device_data->parameter->timeout_times = &sftimer->timeout_times;
    rt_sftimer_device_data->parameter->user_parameter = parameter;
    rt_sftimer_device_data->time = time;
    rt_sftimer_device_data->flag = flag;

    rt_memset(sftimer, 0, sizeof(rt_sftimer_t));
    sftimer->data = rt_sftimer_device_data;
    rt_mutex_init(&(sftimer->lock), name, RT_IPC_FLAG_FIFO);

    //dev->user_data = sftimer;

    dev = &(sftimer->parent);
    dev->type = RT_Device_Class_Timer;
    dev->init        = RT_NULL;
    dev->open        = RT_NULL;
    dev->read        = RT_NULL;
    dev->write       = RT_NULL;
    dev->close       = RT_NULL;
    dev->control     = RT_NULL;

    dev->rx_indicate = RT_NULL;
    dev->tx_complete = RT_NULL;

    if (rt_device_register(&(sftimer->parent), name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE) != 0)
    {
        rt_free(sftimer);
        return RT_NULL;
    }
#ifdef RT_USING_POSIX
    dev->fops = (void*)&sftimer_fops;
#endif

    return sftimer;
}
