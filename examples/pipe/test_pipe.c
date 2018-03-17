#include <rtdevice.h>
#include <dfs_posix.h>

#define TEST_MKFIFO					1
#define STATIC_THREAD				1
#define READ_PIPE_USE_SELECT		1

#define READ_PIPE_TEST				1
#define WRITE_PIPE_TEST				1

#if READ_PIPE_USE_SELECT
#include <dfs_select.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#if READ_PIPE_USE_SELECT
#define READ_WRITE_PIPE_THREAD_SIZE 512
#else
#define READ_WRITE_PIPE_THREAD_SIZE 384
#endif

#define READ_PIPE_OPEN_FLAG_RD_ONLY				(O_RDONLY)
#define READ_PIPE_OPEN_FLAG_RD_ONLY_NONBLOCK	(O_RDONLY | O_NONBLOCK)
#define WRITE_PIPE_OPEN_FLAG_WR_ONLY			(O_WRONLY)
#define WRITE_PIPE_OPEN_FLAG_WR_ONLY_NONBLOCK	(O_WRONLY | O_NONBLOCK)

#define READ_PIPE_THREAD_NAME       "read_pipe_thread"
#define WRITE_PIPE_THREAD_NAME      "write_pipe_thread"

#if READ_PIPE_USE_SELECT
fd_set activePipesFDs;
int fdmax;
#endif

#if STATIC_THREAD
static rt_uint8_t read_pipe_thread_stack[ READ_WRITE_PIPE_THREAD_SIZE ];
static rt_uint8_t write_pipe_thread_stack[ READ_WRITE_PIPE_THREAD_SIZE ];
static struct rt_thread read_pipe_thread;
static struct rt_thread write_pipe_thread;
#endif /* STATIC_THREAD */

#if TEST_MKFIFO
#define TEST_PIPE_NAME           	"test_pipe"
#define PATH_OF_TEST_PIPE           "/dev/test_pipe"
//#define PATH_OF_TEST_PIPE			"/dev/uart2"

void read_pipe_thread_entry(void* arg)
{
    char r_buf[32];
    int  fd;
    int  ret_size;

#if READ_PIPE_USE_SELECT
    int c;
    struct timeval t;
    int timeout_cnt = 0xffffffff;
#endif

    fd=open(PATH_OF_TEST_PIPE, READ_PIPE_OPEN_FLAG_RD_ONLY_NONBLOCK, 0);
    if(fd==-1)
    {
    	rt_kprintf("open %s for read error\n", PATH_OF_TEST_PIPE);
    	return;
    }
    else
    {
    	rt_kprintf("open pipe for read ok.\n");
    }
#if READ_PIPE_USE_SELECT
    t.tv_sec = 0;
    t.tv_usec = 100000;
    fdmax = fd;
    //rt_thread_delay(5*RT_TICK_PER_SECOND);
#endif
    while(1)
    {
#if READ_PIPE_USE_SELECT
        FD_ZERO(&activePipesFDs);
        FD_SET (fd, &activePipesFDs);
        // First use select to find activity on the sockets
        if ((ret_size = select (fdmax + 1, &activePipesFDs, NULL, NULL, &t))< 0)
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
        }
        else
        {
			// Then process this activity
			if (FD_ISSET(fd, &activePipesFDs))
			{
				//接收客户端管道的数据
				ret_size = read (fd, r_buf, sizeof(r_buf));
				if (ret_size <= 0)
				{
					rt_kprintf("read failed for n <= 0\n");
					if (ret_size < 0)
					{
						rt_kprintf("NPI_LNX_ERROR_IPC_RECV_DATA_CHECK_ERRNO\n");
					}
					else
					{
						rt_kprintf("NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT\n");
						close(fd);
						FD_CLR(fd, &activePipesFDs);
#if 1
					    fd=open(PATH_OF_TEST_PIPE, READ_PIPE_OPEN_FLAG_RD_ONLY_NONBLOCK, 0);
					    if(fd==-1)
					    {
					    	rt_kprintf("open %s for read error\n", PATH_OF_TEST_PIPE);
					    	return;
					    }
					    else
					    {
					    	rt_kprintf("open pipe for read ok.\n");
					        fdmax = fd;
					        rt_thread_delay(1*RT_TICK_PER_SECOND);
					    }
#endif
					}
				}
				else
				{
					rt_kprintf("read some data!\n");
				}
			}
        }
#else
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
#endif
    }
}
void write_pipe_thread_entry(void* arg)
{
    char* w_buf = "hello,world\n";
    int  fd;
    int  ret_size;

    fd=open(PATH_OF_TEST_PIPE, WRITE_PIPE_OPEN_FLAG_WR_ONLY, 0);
    if(fd==-1)
    {
        if(errno==ENXIO)
        {
        	rt_kprintf("open %s for write error; no reading process\n", PATH_OF_TEST_PIPE);
        }
    }
    else
    {
    	rt_kprintf("open pipe for write ok.\n");
    }
    while(1)
    {
        ret_size=write(fd,w_buf,strlen(w_buf));
        if(ret_size==-1)
        {
        	rt_kprintf("pipe write return value is %d.\n", ret_size);
            if(errno==EAGAIN)
            	rt_kprintf("write to fifo error; try later\n");
        }
        else
        {
        	rt_kprintf("real write num is %d\n",ret_size);
        }
        rt_thread_delay(10*RT_TICK_PER_SECOND);
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
#if READ_PIPE_TEST || WRITE_PIPE_TEST
    rt_err_t result;
#if READ_PIPE_TEST
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
    rt_thread_delay(3*RT_TICK_PER_SECOND);
#endif /* READ_PIPE_TEST */
#if WRITE_PIPE_TEST
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
#endif /* WRITE_PIPE_TEST */
#endif /* READ_PIPE_TEST || WRITE_PIPE_TEST */
#else
#if READ_PIPE_TEST || WRITE_PIPE_TEST
    rt_thread_t pipe_write_thread;
    rt_thread_t pipe_read_thread;
#if WRITE_PIPE_TEST
    pipe_write_thread = rt_thread_create(WRITE_PIPE_THREAD_NAME,
                                   write_pipe_thread_entry, RT_NULL,
                                   READ_WRITE_PIPE_THREAD_SIZE, 8, 10);
    if (pipe_write_thread != RT_NULL)
        rt_thread_startup(pipe_write_thread);
#endif /* WRITE_PIPE_TEST */
#if READ_PIPE_TEST
    pipe_read_thread = rt_thread_create(READ_PIPE_THREAD_NAME,
                                   read_pipe_thread_entry, RT_NULL,
                                   READ_WRITE_PIPE_THREAD_SIZE, 9, 11);
    if (pipe_read_thread != RT_NULL)
        rt_thread_startup(pipe_read_thread);
#endif /* READ_PIPE_TEST */
#endif /* READ_PIPE_TEST || WRITE_PIPE_TEST */
#endif /* STATIC_THREAD */
    return;                                       
}
