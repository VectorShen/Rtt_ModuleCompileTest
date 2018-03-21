#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>             /* Definition of uint64_t */
#include <clock_time.h>
#include <dfs_posix.h>

#define handle_error(msg) \
               do { rt_kprintf(msg); exit(-1); } while (0)

char* test_timerfd_args[4]=
{
    "test_timerfd_main","3","1","10"
};

static void print_elapsed_time (void)
{
    static struct timespec start;
    struct timespec curr;
    static int first_call = 1;
    int secs, nsecs;

    if (first_call)
    {
        first_call = 0;
        if (clock_gettime (CLOCK_REALTIME, &start) == -1)
            handle_error("error clock_gettime.\n");
    }

    if (clock_gettime (CLOCK_REALTIME, &curr) == -1)
        handle_error("error clock_gettime.\n");

    secs = curr.tv_sec - start.tv_sec;
    nsecs = curr.tv_nsec - start.tv_nsec;
    if (nsecs < 0)
    {
        secs--;
        nsecs += 1000000000;
    }
    rt_kprintf ("%d.%03d: \n", secs, (nsecs + 500000) / 1000000);
}

int test_timerfd_main (int argc, char *argv[])
{
    struct itimerspec new_value;
    int max_exp, fd;
    struct timespec now;
    rt_uint32_t exp, tot_exp;
    ssize_t s;

    if ((argc != 2) && (argc != 4))
    {
        rt_kprintf("%s init-secs [interval-secs max-exp]\n", argv[0]);
        exit (-1);
    }

    if (clock_gettime (CLOCK_REALTIME, &now) == -1)
        handle_error("error clock_gettime.\n");

    /* Create a CLOCK_REALTIME absolute timer with initial
       expiration and interval as specified in command line */

    new_value.it_value.tv_sec = now.tv_sec + atoi (argv[1]);
    new_value.it_value.tv_nsec = now.tv_nsec;
    if (argc == 2)
    {
        new_value.it_interval.tv_sec = 0;
        max_exp = 1;
    }
    else
    {
        new_value.it_interval.tv_sec = atoi (argv[2]);
        max_exp = atoi (argv[3]);
    }
    new_value.it_interval.tv_nsec = 0;

    fd = timerfd_create (CLOCK_REALTIME, 0);
    if (fd == -1)
        handle_error("error timerfd_create.\n");

    if (timerfd_settime (fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
        handle_error("error timerfd_settime.\n");

    print_elapsed_time ();
    rt_kprintf ("timer started\n");

    for (tot_exp = 0; tot_exp < max_exp;)
    {
        s = read (fd, &exp, sizeof (rt_uint32_t));
        if (s != sizeof (rt_uint32_t))
            handle_error ("erro read.\n");

        tot_exp += exp;
        print_elapsed_time ();
        rt_thread_delay(RT_TICK_PER_SECOND*2);
        rt_kprintf ("read: %d; total=%d\n",
        	exp, tot_exp);
    }

    ioctl(fd, SFTIMER_CTRL_STOP, RT_NULL);
    close(fd);

    return 0;
}
