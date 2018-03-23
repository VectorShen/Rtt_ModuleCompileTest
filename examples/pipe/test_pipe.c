#include <rtdevice.h>
#include <dfs_posix.h>

#define TEST_MKFIFO					1
#define STATIC_THREAD				1
#define READ_PIPE_USE_SELECT		1

#define READ_PIPE_TEST				1
#define WRITE_PIPE_TEST				1
#define SOCKET_PIPE_TEST			1	//1

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
#define READ_PIPE_OPEN_FLAG_RDWR				(O_RDWR)
#define READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK		(O_RDWR | O_NONBLOCK)
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

#ifdef SOCKET_PIPE_TEST
#define SOCKET_PIPE_LISTEN_SERVER2CLIENT	"listen_pipe_s2c"
#define SOCKET_PIPE_LISTEN_CLIENT2SERVER	"listen_pipe_c2s"
#define SOCKET_PIPE_SERVER2CLIENT			"socket_pipe_s2c"
#define SOCKET_PIPE_CLIENT2SERVER			"socket_pipe_c2s"
#define FIFO_PATH_PREFIX					"/dev/"
#define SERVER_LISTEN_BUF_SIZE			   	30
#define LISTEN_PIPE_CHECK_STRING			"check_listen_pipe"
#define FIFO_PATH_BUFFER_LEN				30

#define REGULAR_READ_WRITE_STRING_BUF_LEN	30
#define SERVER_REGULAR_WRITE_STRING			"server_says_hello"
#define CLIENT_REGULAR_WRITE_STRING			"client_says_hello"

#define TMP_ASSIGNED_ID_STRING_LEN         	3
#define TMP_PIPE_NAME_SIZE				   	40
#endif /* SOCKET_PIPE_TEST */

#if SOCKET_PIPE_TEST
void socket_pipe_server_entry(void* arg)
{
	int c;
	int n;
	int clientsNum = 0;
	int listenPipeReadHndl;
	int listenPipeWriteHndl;
	int server_write_fd;
	int server_read_fd;
	fd_set activePipesFDs;
	fd_set activePipesFDsSafeCopy;
	int fdmax;
	char server_read_buf[REGULAR_READ_WRITE_STRING_BUF_LEN];
	char listen_buf[SERVER_LISTEN_BUF_SIZE];
	char tmpReadPipeName[TMP_PIPE_NAME_SIZE];
	char tmpWritePipeName[TMP_PIPE_NAME_SIZE];
	int tmpReadPipe;
	int tmpWritePipe;
    char assignedId[TMP_ASSIGNED_ID_STRING_LEN];

    char server_read_fifo_path[FIFO_PATH_BUFFER_LEN];
    char server_write_fifo_path[FIFO_PATH_BUFFER_LEN];
    struct timeval t;

	memset(server_read_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(server_read_fifo_path, "%s%s", FIFO_PATH_PREFIX, SOCKET_PIPE_LISTEN_CLIENT2SERVER);

	rt_kprintf("socket_pipe_server_entry: listenPipeReadHndl is %s.\n", server_read_fifo_path);

	listenPipeReadHndl = open(server_read_fifo_path, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
	if (listenPipeReadHndl == -1)
	{
		rt_kprintf ("socket_pipe_server_entry: open %s for read error\n", server_read_fifo_path);
		return;
	}
	else
	{
		rt_kprintf("socket_pipe_server_entry: open client_listen_read_fifo ok with fd = %d.\n", listenPipeReadHndl);
	}

	FD_ZERO(&activePipesFDs);
	FD_SET (listenPipeReadHndl, &activePipesFDs);

	fdmax = listenPipeReadHndl;

	rt_kprintf("socket_pipe_server_entry: waiting for first connection on #%d...\n", listenPipeReadHndl);

	while (1)
	{
		if (select (fdmax + 1, &activePipesFDs, NULL, NULL, NULL) == -1)
		{
			if (errno != EINTR)
			{
				rt_kprintf("socket_pipe_server_entry: error select\n");
				break;
			}
			continue;
		}

		for (c = 0; c <= fdmax; c++)
		{
			if (FD_ISSET(c, &activePipesFDs))
			{
				if(c==listenPipeReadHndl)
				{
					n = read (listenPipeReadHndl, listen_buf, SERVER_LISTEN_BUF_SIZE);
					if (n <= 0)
					{
						rt_kprintf("socket_pipe_server_entry: read failed for n <= 0\n");
						if (n < 0)
						{
							rt_kprintf ("socket_pipe_server_entry: error read");
						}
						else
						{
							rt_kprintf("socket_pipe_server_entry: NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT\n");
							close(listenPipeReadHndl);
							FD_CLR(listenPipeReadHndl, &activePipesFDs);
							listenPipeReadHndl = open (server_read_fifo_path, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
							if (listenPipeReadHndl == -1)
							{
								rt_kprintf ("open %s for read error\n", server_read_fifo_path);
								return;
							}
							FD_SET (listenPipeReadHndl, &activePipesFDs);
							if (listenPipeReadHndl > fdmax)
							{
								fdmax = listenPipeReadHndl;
							}
						}
					}
					else
					{
						listen_buf[n] = '\0';
						rt_kprintf("socket_pipe_server_entry: read %d bytes from listenPipeReadHndl of string is %s.\n",n,listen_buf);
						if (!strncmp(listen_buf, LISTEN_PIPE_CHECK_STRING, strlen(LISTEN_PIPE_CHECK_STRING)))
						{
							memset(server_write_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
							sprintf(server_write_fifo_path, "%s%s", FIFO_PATH_PREFIX, SOCKET_PIPE_LISTEN_SERVER2CLIENT);
							rt_kprintf("socket_pipe_server_entry: client_write_fifo_path is %s.\n", server_write_fifo_path);
							listenPipeWriteHndl = open (server_write_fifo_path, WRITE_PIPE_OPEN_FLAG_WR_ONLY, 0);
							if (listenPipeWriteHndl == -1)
							{
								if (errno == ENXIO)
								{
									rt_kprintf("socket_pipe_server_entry: open error; no reading process\n");
									break;
								}
							}

							sprintf(assignedId,"%d",clientsNum);
							memset (tmpReadPipeName, '\0', TMP_PIPE_NAME_SIZE);
							memset (tmpWritePipeName, '\0', TMP_PIPE_NAME_SIZE);
							sprintf(tmpReadPipeName, "%s%d", SOCKET_PIPE_CLIENT2SERVER, clientsNum);
							sprintf(tmpWritePipeName, "%s%d", SOCKET_PIPE_SERVER2CLIENT, clientsNum);

							write(listenPipeWriteHndl, assignedId, strlen(assignedId));

							if ((mkfifo (tmpReadPipeName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
							{
								rt_kprintf ("socket_pipe_server_entry: cannot create fifo %s\n", tmpReadPipeName);
							}
							if ((mkfifo (tmpWritePipeName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
							{
								rt_kprintf ("socket_pipe_server_entry: cannot create fifo %s\n", tmpWritePipeName);
							}

							memset (tmpReadPipeName, '\0', TMP_PIPE_NAME_SIZE);
							memset (tmpWritePipeName, '\0', TMP_PIPE_NAME_SIZE);
							sprintf(tmpReadPipeName, "%s%s%d", FIFO_PATH_PREFIX, SOCKET_PIPE_CLIENT2SERVER, clientsNum);
							sprintf(tmpWritePipeName, "%s%s%d", FIFO_PATH_PREFIX, SOCKET_PIPE_SERVER2CLIENT, clientsNum);

							tmpReadPipe = open (tmpReadPipeName, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
							if (tmpReadPipe == -1)
							{
								rt_kprintf ("socket_pipe_server_entry: open %s for read error\n", tmpReadPipeName);
								break;
							}

							tmpWritePipe = open (tmpWritePipeName, WRITE_PIPE_OPEN_FLAG_WR_ONLY, 0);
							if (tmpWritePipe == -1)
							{
								rt_kprintf ("socket_pipe_server_entry: open %s for write error\n", tmpWritePipeName);
								break;
							}

							server_write_fd = tmpWritePipe;
							server_read_fd = tmpReadPipe;

							FD_SET (server_read_fd, &activePipesFDs);
							if(server_read_fd>fdmax)
							{
								fdmax = server_read_fd;
							}

							close (listenPipeWriteHndl);
							clientsNum++;

							rt_thread_delay(RT_TICK_PER_SECOND);
							write(server_write_fd, SERVER_REGULAR_WRITE_STRING, strlen(SERVER_REGULAR_WRITE_STRING));
						}
					}
				}
				else
				{
					n = read (c, server_read_buf, REGULAR_READ_WRITE_STRING_BUF_LEN);
					if (n <= 0)
					{
						rt_kprintf("socket_pipe_server_entry: read failed for n <= 0\n");
						if (n < 0)
						{
							rt_kprintf ("socket_pipe_server_entry: error read\n");
						}
						else
						{
							rt_kprintf ("socket_pipe_server_entry: error disconnect\n");
							close(c);
							FD_CLR(c, &activePipesFDs);
							server_read_fd = open (tmpReadPipeName, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
							if (server_read_fd == -1)
							{
								rt_kprintf ("socket_pipe_server_entry: open socket_pipe %s for read error\n", tmpReadPipeName);
								return;
							}
							FD_SET (server_read_fd, &activePipesFDs);
							if (server_read_fd > fdmax)
							{
								fdmax = server_read_fd;
							}
						}
					}
					else
					{
						server_read_buf[n] = '\0';
						rt_kprintf("socket_pipe_server_entry: server read from client:%s.\n", server_read_buf);
						rt_thread_delay(RT_TICK_PER_SECOND);
						n = write(server_write_fd, SERVER_REGULAR_WRITE_STRING, strlen(SERVER_REGULAR_WRITE_STRING));
						rt_kprintf("socket_pipe_server_entry: server write to client:%d %s.\n", n, SERVER_REGULAR_WRITE_STRING);
					}
				}
			}
		}
	}
#if 0
	FD_ZERO(&activePipesFDs);
	FD_SET (server_read_fd, &activePipesFDs);
	fdmax = server_read_fd;

	while(1)
	{
		if (select (fdmax + 1, &activePipesFDs, NULL, NULL, NULL) == -1)
		{
			if (errno != EINTR)
			{
				rt_kprintf("socket_pipe_server_entry: error select\n");
				break;
			}
			continue;
		}

		for (c = 0; c <= fdmax; c++)
		{
			if (FD_ISSET(c, &activePipesFDs))
			{
				if (c == server_read_fd)
				{
                    n = read (c, server_read_buf, REGULAR_READ_WRITE_STRING_BUF_LEN);
                    if (n <= 0)
                    {
						rt_kprintf("socket_pipe_client_entry: read failed for n <= 0\n");
                        if (n < 0)
                        {
                        	rt_kprintf ("socket_pipe_client_entry: error read\n");
                        }
                        else
                        {
                        	rt_kprintf ("socket_pipe_server_entry: error disconnect\n");
							close(c);
                            FD_CLR(c, &activePipesFDs);
                            server_read_fd = open (tmpReadPipeName, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
    						if (server_read_fd == -1)
    						{
        						rt_kprintf ("socket_pipe_server_entry: open socket_pipe %s for read error\n", tmpReadPipeName);
       		 					return;
						    }
                            FD_SET (server_read_fd, &activePipesFDs);
                            if (server_read_fd > fdmax)
                            {
                            	fdmax = server_read_fd;
                        	}
                        }
                    }
					else
					{
						server_read_buf[n] = '\0';
						rt_kprintf("socket_pipe_server_entry: server read from client:%s.\n", server_read_buf);
						rt_thread_delay(RT_TICK_PER_SECOND);
						n = write(server_write_fd, SERVER_REGULAR_WRITE_STRING, strlen(SERVER_REGULAR_WRITE_STRING));
						rt_kprintf("socket_pipe_server_entry: server write to client:%d %s.\n", n, SERVER_REGULAR_WRITE_STRING);
					}
				}
			}
		}
	}
#endif
}

void socket_pipe_client_entry(void* arg)
{
	int n;
	int c;
    char client_read_fifo_path[FIFO_PATH_BUFFER_LEN];
    char client_write_fifo_path[FIFO_PATH_BUFFER_LEN];
	char readPipePathName[FIFO_PATH_BUFFER_LEN];
	char writePipePathName[FIFO_PATH_BUFFER_LEN];
    char assignedIdBuf[TMP_ASSIGNED_ID_STRING_LEN];
    int readWriteNum;
	char client_read_buf[REGULAR_READ_WRITE_STRING_BUF_LEN];
    int tmpReadPipe;
    int tmpWritePipe;

    fd_set activePipesFDs;
	int fdmax;

    memset(assignedIdBuf,'\0',TMP_ASSIGNED_ID_STRING_LEN);

	memset(readPipePathName,'\0',FIFO_PATH_BUFFER_LEN);
	memset(writePipePathName,'\0',FIFO_PATH_BUFFER_LEN);

	strncpy(readPipePathName,SOCKET_PIPE_LISTEN_SERVER2CLIENT,strlen(SOCKET_PIPE_LISTEN_SERVER2CLIENT));
	strncpy(writePipePathName,SOCKET_PIPE_LISTEN_CLIENT2SERVER,strlen(SOCKET_PIPE_LISTEN_CLIENT2SERVER));

	memset(client_write_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(client_write_fifo_path, "%s%s", FIFO_PATH_PREFIX, writePipePathName);
	memset(client_read_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(client_read_fifo_path, "%s%s", FIFO_PATH_PREFIX, readPipePathName);

	rt_kprintf("socket_pipe_client_entry: socket_listen readPipePathName is %s\n",client_read_fifo_path);
	rt_kprintf("socket_pipe_client_entry: socket_listen writePipePathName is %s\n",client_write_fifo_path);

    tmpWritePipe = open(client_write_fifo_path, WRITE_PIPE_OPEN_FLAG_WR_ONLY, 0);
    if(tmpWritePipe == -1)
    {
        rt_kprintf("socket_pipe_client_entry: open socket_listen write pipe failed.\n");
    }

    n = write (tmpWritePipe,LISTEN_PIPE_CHECK_STRING,strlen(LISTEN_PIPE_CHECK_STRING));
	rt_kprintf("socket_pipe_client_entry: write to socket_listen write pipe checkString %d.\n", n);

    tmpReadPipe = open(client_read_fifo_path, READ_PIPE_OPEN_FLAG_RDWR, 0);
    if(tmpReadPipe == -1)
    {
        rt_kprintf("socket_pipe_client_entry: open socket_listen read pipe failed\n");
    }

	readWriteNum = read(tmpReadPipe, assignedIdBuf, TMP_ASSIGNED_ID_STRING_LEN);
	if(readWriteNum<=0)
	{
		rt_kprintf("socket_pipe_client_entry: read from socket_listen read pipe with num is %d.\n", readWriteNum);
	}

	close(tmpWritePipe);
	close(tmpReadPipe);

	memset(readPipePathName, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(readPipePathName, "%s%s", SOCKET_PIPE_SERVER2CLIENT, assignedIdBuf);
	memset(writePipePathName, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(writePipePathName, "%s%s", SOCKET_PIPE_CLIENT2SERVER, assignedIdBuf);

	if ((mkfifo (readPipePathName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		rt_kprintf ("socket_pipe_client_entry: cannot create fifo %s\n", readPipePathName);
	}
	if ((mkfifo (writePipePathName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		rt_kprintf ("socket_pipe_client_entry: cannot create fifo %s\n", writePipePathName);
	}

	memset(client_read_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(client_read_fifo_path, "%s%s", FIFO_PATH_PREFIX, readPipePathName);
	memset(client_write_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(client_write_fifo_path, "%s%s", FIFO_PATH_PREFIX, writePipePathName);

	rt_kprintf("socket_pipe_client_entry: socket_pipe readPipePathName is %s\n",client_read_fifo_path);
	rt_kprintf("socket_pipe_client_entry: socket_pipe writePipePathName is %s\n",client_write_fifo_path);

	tmpReadPipe = open (client_read_fifo_path, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
	if (tmpReadPipe == -1)
	{
    	rt_kprintf ("socket_pipe_client_entry: open %s for read error\n", client_read_fifo_path);
    	return;
	}

	rt_kprintf("socket_pipe_client_entry: socket_pipe read fd is %d.\n", tmpReadPipe);

	tmpWritePipe = open (client_write_fifo_path, WRITE_PIPE_OPEN_FLAG_WR_ONLY, 0);
	if (tmpWritePipe == -1)
	{
    	rt_kprintf ("socket_pipe_client_entry: open %s for write error\n", client_write_fifo_path);
    	return;
	}

	rt_kprintf("socket_pipe_client_entry: socket_pipe write fd is %d.\n", tmpWritePipe);

	FD_ZERO(&activePipesFDs);
	FD_SET (tmpReadPipe, &activePipesFDs);
	fdmax = tmpReadPipe;

	while(1)
	{
		if (select (fdmax + 1, &activePipesFDs, NULL, NULL, NULL) == -1)
		{
			if (errno != EINTR)
			{
				rt_kprintf("error select\n");
				break;
			}
			continue;
		}

		// Then process this activity
		for (c = 0; c <= fdmax; c++)
		{
			if (FD_ISSET(c, &activePipesFDs))
			{
				if (c == tmpReadPipe)
				{
                    n = read (c, client_read_buf, REGULAR_READ_WRITE_STRING_BUF_LEN);
                    if (n <= 0)
                    {
						rt_kprintf("socket_pipe_client_entry: read failed for n <= 0\n");
                        if (n < 0)
                        {
                        	rt_kprintf ("socket_pipe_client_entry: error read\n");
                        }
                        else
                        {
                        	rt_kprintf ("socket_pipe_client_entry: error disconnect\n");
							close(c);
                            FD_CLR(c, &activePipesFDs);
                            tmpReadPipe = open (client_read_fifo_path, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
    						if (tmpReadPipe == -1)
    						{
        						rt_kprintf ("socket_pipe_client_entry: open socket_pipe %s for read error\n", client_read_fifo_path);
       		 					return;
						    }
                            FD_SET (tmpReadPipe, &activePipesFDs);
                            if (tmpReadPipe > fdmax)
                            {
                            	fdmax = tmpReadPipe;
                        	}
                        }
                    }
					else
					{
						client_read_buf[n] = '\0';
						rt_kprintf("socket_pipe_client_entry: client read from server:%s.\n", client_read_buf);
						rt_thread_delay(RT_TICK_PER_SECOND);
						n = write(tmpWritePipe, CLIENT_REGULAR_WRITE_STRING, strlen(CLIENT_REGULAR_WRITE_STRING));
						rt_kprintf("socket_pipe_client_entry: client write to server:%d %s.\n", n, CLIENT_REGULAR_WRITE_STRING);
					}
				}
			}
		}
	}
}
#else

#if TEST_MKFIFO
#define TEST_PIPE_NAME           	"test_pipe"
#define PATH_OF_TEST_PIPE           "/dev/test_pipe"

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

    fd=open(PATH_OF_TEST_PIPE, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
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
					    fd=open(PATH_OF_TEST_PIPE, READ_PIPE_OPEN_FLAG_RDWR_NONBLOCK, 0);
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
					r_buf[ret_size] = '\0';
					rt_kprintf("read some data:%s\n", r_buf);
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

#endif /* SOCKET_PIPE_TEST */

void rt_test_read_write_pipe(void)
{
#if SOCKET_PIPE_TEST
	//mkfifo and open pipes
	if ((mkfifo (SOCKET_PIPE_LISTEN_CLIENT2SERVER, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		rt_kprintf ("cannot create fifo %s\n", SOCKET_PIPE_LISTEN_CLIENT2SERVER);
	}
	if ((mkfifo (SOCKET_PIPE_LISTEN_SERVER2CLIENT, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		rt_kprintf ("cannot create fifo %s\n", SOCKET_PIPE_LISTEN_SERVER2CLIENT);
	}
    rt_thread_t pipe_server_thread;
    rt_thread_t pipe_client_thread;
    pipe_server_thread = rt_thread_create("pipe_server_thread",
    		socket_pipe_server_entry, RT_NULL,
			1024, 8, 10);
    if (pipe_server_thread != RT_NULL)
        rt_thread_startup(pipe_server_thread);
    pipe_client_thread = rt_thread_create("pipe_client_thread",
    		socket_pipe_client_entry, RT_NULL,
			1024, 8, 10);
    if (pipe_client_thread != RT_NULL)
        rt_thread_startup(pipe_client_thread);
#else
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
#endif /* SOCKET_PIPE_TEST */
    return;                                       
}
