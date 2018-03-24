/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 * 2013-07-12     aozima       update for auto initial.
 */

/**
 * @addtogroup STM32
 */
/*@{*/

#include <board.h>
#include <rtthread.h>

#include "shell.h"

#ifdef RT_USING_DFS
/* dfs filesystem:ELM filesystem init */
#include <dfs_elm.h>
/* dfs Filesystem APIs */
#include <dfs_fs.h>
#endif

#ifdef RT_USING_RTGUI
#include <rtgui/rtgui.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>
#include <rtgui/driver.h>
#include <rtgui/calibration.h>
#endif

#if 0
#include "led.h"

ALIGN(RT_ALIGN_SIZE)
static rt_uint8_t led_stack[ 512 ];
static struct rt_thread led_thread;
static void led_thread_entry(void* parameter)
{
    unsigned int count=0;

    rt_hw_led_init();

    while (1)
    {
        /* led1 on */
#ifndef RT_USING_FINSH
        rt_kprintf("led on, count : %d\r\n",count);
#endif
        count++;
        rt_hw_led_on(0);
        rt_thread_delay( RT_TICK_PER_SECOND/2 ); /* sleep 0.5 second and switch to other thread */

        /* led1 off */
#ifndef RT_USING_FINSH
        rt_kprintf("led off\r\n");
#endif
        rt_hw_led_off(0);
        rt_thread_delay( RT_TICK_PER_SECOND/2 );
    }
}
#endif

#ifdef RT_USING_RTGUI
rt_bool_t cali_setup(void)
{
    rt_kprintf("cali setup entered\n");
    return RT_FALSE;
}

void cali_store(struct calibration_data *data)
{
    rt_kprintf("cali finished (%d, %d), (%d, %d)\n",
               data->min_x,
               data->max_x,
               data->min_y,
               data->max_y);
}
#endif /* RT_USING_RTGUI */

#ifdef RT_USING_PROTOBUF_C
#ifdef RT_USING_PROTOBUF_C_TEST
extern int amessage_main (void);
#endif /* RT_USING_PROTOBUF_C_TEST */
#endif /* RT_USING_PROTOBUF_C */

#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_VERSION_QUERY
extern char* version_query_argvs[2];
extern int version_query_main (int argc, char *argv[]);
#endif /* RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_VERSION_QUERY */
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_NPI_IPC
#if 0
extern char* npi_rtt_ipc_argvs[2];
extern int npi_rtt_ipc_main(int argc, char ** argv);
#else
extern char* npi_rtt_ipc_argvs[3];
extern void npi_rtt_ipc_main(void* args);
#ifdef RT_USING_COMPONENTS_DRIVERS_PIPE_TEST_NPI_IPC
void npi_rtt_ipc_pipe_main(void* args);
#endif
#endif
#endif /* RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_NPI_IPC */
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_ZLSZNP
extern char* zlsznp_srv_args[3];
extern int srv_main (int argc, char *argv[]);
#endif
#endif /* RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY */

#ifdef RT_USING_EXAMPLES
#ifdef RT_USING_SFTIMER_TEST
extern int sftimer(void);
#endif /* RT_USING_SFTIMER_TEST */
#endif /* RT_USING_EXAMPLES */

#ifdef RT_USING_TIMERFD
#ifdef RT_USING_TIMERFD_TEST
extern char* test_timerfd_args[4];
extern int test_timerfd_main (int argc, char *argv[]);
#endif
#endif
void rt_init_thread_entry(void* parameter)
{
#ifdef RT_USING_COMPONENTS_INIT
    /* initialization RT-Thread Components */
    rt_components_init();
    rt_components_test();
#endif

#ifdef RT_USING_PROTOBUF_C
#ifdef RT_USING_PROTOBUF_C_TEST
    amessage_main();
#endif
#endif

    /* Filesystem Initialization */
#if defined(RT_USING_DFS) && defined(RT_USING_DFS_ELMFAT)
    /* mount sd card fat partition 1 as root directory */
    if (dfs_mount("sd0", "/", "elm", 0, 0) == 0)
    {
        rt_kprintf("File System initialized!\n");
    }
    else
        rt_kprintf("File System initialzation failed!\n");
#endif  /* RT_USING_DFS */

#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_VERSION_QUERY
	version_query_main(2, version_query_argvs);
#endif /* RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_VERSION_QUERY */
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_NPI_IPC
	//npi_rtt_ipc_main(2, npi_rtt_ipc_argvs);
    rt_thread_t npi_ipc_thread;
    npi_ipc_thread = rt_thread_create("npi_ipc_thread",
    		npi_rtt_ipc_main, npi_rtt_ipc_argvs,
          	2048, 9, 20);
    if(npi_ipc_thread!=RT_NULL)
    {
    	rt_thread_startup(npi_ipc_thread);
    }
#if 0	//def RT_USING_COMPONENTS_DRIVERS_PIPE_TEST_NPI_IPC
    rt_thread_delay(RT_TICK_PER_SECOND*10);
    rt_thread_t npi_ipc_pipe_thread;
    npi_ipc_pipe_thread = rt_thread_create("npi_ipc_pipe_thread",
    		npi_rtt_ipc_pipe_main, RT_NULL,
          	2048, 9, 20);
    if(npi_ipc_pipe_thread!=RT_NULL)
    {
    	rt_thread_startup(npi_ipc_pipe_thread);
    }
#endif
#endif
#ifdef RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY_ZLSZNP
    rt_thread_delay(RT_TICK_PER_SECOND*3);
	srv_main (3, zlsznp_srv_args);
#endif
#endif /* RT_USING_WIRELESS_ZIGBEE_TI_ZSTACK_GATEWAY */
#ifdef RT_USING_EXAMPLES
#ifdef RT_USING_SFTIMER_TEST
	sftimer();
#endif /* RT_USING_SFTIMER */
#endif /* RT_USING_EXAMPLES */

#ifdef RT_USING_TIMERFD
#ifdef RT_USING_TIMERFD_TEST
	test_timerfd_main (4, test_timerfd_args);
#endif
#endif

#ifdef RT_USING_RTGUI
    {
        extern void rt_hw_lcd_init();
        extern void rtgui_touch_hw_init(void);

        rt_device_t lcd;

        /* init lcd */
        rt_hw_lcd_init();

        /* init touch panel */
        rtgui_touch_hw_init();

        /* find lcd device */
        lcd = rt_device_find("lcd");

        /* set lcd device as rtgui graphic driver */
        rtgui_graphic_set_device(lcd);

#ifndef RT_USING_COMPONENTS_INIT
        /* init rtgui system server */
        rtgui_system_server_init();
#endif

        calibration_set_restore(cali_setup);
        calibration_set_after(cali_store);
        calibration_init();
    }
#endif /* #ifdef RT_USING_RTGUI */
    while(1)
    {
    	rt_thread_delay(RT_TICK_PER_SECOND);
    }
    rt_kprintf(FINSH_PROMPT);
}

int rt_application_init(void)
{
    rt_thread_t init_thread;

#if 0
    rt_err_t result;

    /* init led thread */
    result = rt_thread_init(&led_thread,
                            "led",
                            led_thread_entry,
                            RT_NULL,
                            (rt_uint8_t*)&led_stack[0],
                            sizeof(led_stack),
                            20,
                            5);
    if (result == RT_EOK)
    {
        rt_thread_startup(&led_thread);
    }
#endif

#if (RT_THREAD_PRIORITY_MAX == 32)
    init_thread = rt_thread_create("init",
                                   rt_init_thread_entry, RT_NULL,
                                   (10240+5120), 8, 20);
#else
    init_thread = rt_thread_create("init",
                                   rt_init_thread_entry, RT_NULL,
                                   2048, 80, 20);
#endif

    if (init_thread != RT_NULL)
        rt_thread_startup(init_thread);

    return 0;
}

/*@}*/
