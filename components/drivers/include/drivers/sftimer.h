#ifndef __SFTIMER_H__
#define __SFTIMER_H__

#include <rtthread.h>
#include <rtdevice.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Timer Control Command */
typedef enum
{
    SFTIMER_CTRL_START = 10,                     /* start timer */
    SFTIMER_CTRL_STOP,                      /* stop timer */
    SFTIMER_CTRL_TIMEOUT_TIMES_GET,         /* get a timer feature information */
} rt_sftimer_ctrl_t;

struct rt_sftimer_timeout_func_param
{
    rt_uint32_t* timeout_times;
    void* user_parameter;
};
typedef struct rt_sftimer_timeout_func_param rt_sftimer_timeout_func_param_t;

struct rt_sftimer_device_data
{
    const char* name;
    void (*timeout)(void *parameter);
    rt_sftimer_timeout_func_param_t* parameter;
    rt_tick_t time;
    rt_uint8_t flag;
};
typedef struct rt_sftimer_device_data rt_sftimer_device_data_t;

struct rt_sftimer_device
{
    struct rt_device parent;
    struct rt_timer* timer;
    struct rt_sftimer_device_data* data;
    rt_uint32_t timeout_times;
    //rt_wqueue_t reader_queue;
    struct rt_mutex lock;
};
typedef struct rt_sftimer_device rt_sftimer_t;

rt_sftimer_t *rt_sftimer_create(const char *name, 
    void (*timeout)(void *parameter),
    void* parameter,
    rt_tick_t time,
    rt_uint8_t flag);
    
#ifdef __cplusplus
}
#endif

#endif
