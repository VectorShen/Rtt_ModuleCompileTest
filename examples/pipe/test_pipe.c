#include <rtdevice.h>
#include <dfs_posix.h>

#define TEST_MKFIFO					1
#define STATIC_THREAD				1

#define READ_WRITE_PIPE_THREAD_SIZE 384
#define READ_PIPE_THREAD_NAME       "read_pipe_thread"
#define WRITE_PIPE_THREAD_NAME      "write_pipe_thread"

#if STATIC_THREAD
static rt_uint8_t read_pipe_thread_stack[ READ_WRITE_PIPE_THREAD_SIZE ];
static rt_uint8_t write_pipe_thread_stack[ READ_WRITE_PIPE_THREAD_SIZE ];
static struct rt_thread read_pipe_thread;
static struct rt_thread write_pipe_thread;
#endif /* STATIC_THREAD */

#if TEST_MKFIFO
#define TEST_PIPE_NAME           	"test_pipe"
#define PATH_OF_TEST_PIPE           "/dev/test_pipe"

void read_pipe_thread_entry(void* arg)
{
    char r_buf[32];
    int  fd;
    int  ret_size;

    fd=open(PATH_OF_TEST_PIPE,O_RDONLY,0);
    if(fd==-1)
    {
    	rt_kprintf("open %s for read error\n", PATH_OF_TEST_PIPE);
    	return;
    }
    while(1)
    {
        rt_memset(r_buf,0,sizeof(r_buf));
        ret_size=read(fd,r_buf,sizeof(r_buf));
        if(ret_size<=0)
        {
            if(errno==EAGAIN)
            	rt_kprintf("no data avlaible\n");
        }
        else
        {
            r_buf[ret_size] = '\0';
            rt_kprintf("real read bytes %d and is %s\n",ret_size, r_buf);
        }
    }
}
void write_pipe_thread_entry(void* arg)
{
    char* w_buf = "hello,world\n";
    int  fd;
    int  ret_size;

    fd=open(PATH_OF_TEST_PIPE,O_WRONLY,0);
    if(fd==-1)
    {
        if(errno==ENXIO)
        {
        	rt_kprintf("open %s for write error; no reading process\n", PATH_OF_TEST_PIPE);
        }
    }
    while(1)
    {
        ret_size=write(fd,w_buf,strlen(w_buf));
        if(ret_size==-1)
        {
            if(errno==EAGAIN)
            	rt_kprintf("write to fifo error; try later\n");
        }
        else
        {
        	rt_kprintf("real write num is %d\n",ret_size);
        }
        rt_thread_delay(3*RT_TICK_PER_SECOND);
    }
}
#else
static int pipe_fds[2];

void read_pipe_thread_entry(void* arg)
{
    char r_buf[32];
    int  ret_size;

    while(1)
    {
        rt_memset(r_buf,0,sizeof(r_buf));
        rt_kprintf("pipe reading......\n");
        ret_size=read(pipe_fds[0],r_buf,sizeof(r_buf));
        if(ret_size<=0)
        {
            if(errno==EAGAIN)
            	rt_kprintf("no data avlaible\n");
        }
        else
        {
            r_buf[ret_size] = '\0';
            rt_kprintf("real read bytes %d and is %s\n",ret_size, r_buf);
        }
    }
}

void write_pipe_thread_entry(void* arg)
{
    char* w_buf = "hello,world\n";
    int  ret_size;

    while(1)
    {
        rt_kprintf("pipe writing......\n");
        ret_size=write(pipe_fds[1],w_buf,strlen(w_buf));
        if(ret_size==-1)
        {
            if(errno==EAGAIN)
            	rt_kprintf("write to fifo error; try later\n");
        }
        else 
        {
        	rt_kprintf("real write num is %d\n",ret_size);
        }
        rt_thread_delay(5*RT_TICK_PER_SECOND);
    }
}
#endif

void rt_test_read_write_pipe(void)
{
#if TEST_MKFIFO
    if((mkfifo(TEST_PIPE_NAME,O_CREAT|O_EXCL)<0)&&(errno!=EEXIST))
    {
    	rt_kprintf("cannot create fifoserver\n");
    }
    else
    {
    	rt_kprintf("mkfifo of pipe name %s is done!\n", TEST_PIPE_NAME);
    }
#else
    if(pipe(pipe_fds)==-1)
    {
        rt_kprintf("pipe function create pipes failed.\n");
        return;
    }

    rt_kprintf("pipe pipes ok with fd[0] is %d and fd[1] is %d.\n",\
    	pipe_fds[0], pipe_fds[1]);
#endif

#if STATIC_THREAD
    rt_err_t result;

    result = rt_thread_init(&read_pipe_thread,
    						READ_PIPE_THREAD_NAME,
							read_pipe_thread_entry,
                            RT_NULL,
                            (rt_uint8_t*)&read_pipe_thread_stack[0],
                            sizeof(read_pipe_thread_stack),
                            20,
                            5);
    if (result == RT_EOK)
    {
        rt_thread_startup(&read_pipe_thread);
    }

    result = rt_thread_init(&write_pipe_thread,
    						WRITE_PIPE_THREAD_NAME,
							write_pipe_thread_entry,
                            RT_NULL,
                            (rt_uint8_t*)&write_pipe_thread_stack[0],
                            sizeof(write_pipe_thread_stack),
                            20,
                            5);
    if (result == RT_EOK)
    {
        rt_thread_startup(&write_pipe_thread);
    }
#else
    rt_thread_t pipe_write_thread;
    rt_thread_t pipe_read_thread;
    pipe_write_thread = rt_thread_create(WRITE_PIPE_THREAD_NAME,
                                   write_pipe_thread_entry, RT_NULL,
                                   READ_WRITE_PIPE_THREAD_SIZE, 8, 10);
    if (pipe_write_thread != RT_NULL)
        rt_thread_startup(pipe_write_thread);

    pipe_read_thread = rt_thread_create(READ_PIPE_THREAD_NAME,
                                   read_pipe_thread_entry, RT_NULL,
                                   READ_WRITE_PIPE_THREAD_SIZE, 9, 11);
    if (pipe_read_thread != RT_NULL)
        rt_thread_startup(pipe_read_thread);
#endif

    return;                                       
}
