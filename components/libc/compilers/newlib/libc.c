/*
 * File     : libc.c
 * Brief    : gcc libc header file
 *
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006 - 2017, RT-Thread Development Team
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
 * Date           Author       Notes
 * 2017/10/15     bernard      the first version
 */
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>

#include "libc.h"

#ifdef RT_USING_PTHREADS
#include <pthread.h>
#endif

#if 0
struct dfs_fd* dfs_stdin  = RT_NULL;
struct dfs_fd* dfs_stdout = RT_NULL;
struct dfs_fd* dfs_stderr = RT_NULL;
#endif

int libc_system_init(void)
{
#if defined(RT_USING_DFS) & defined(RT_USING_DFS_DEVFS)
    rt_device_t dev_console;

    dev_console = rt_console_get_device();
    if (dev_console)
    {
    #if defined(RT_USING_POSIX)
        libc_stdio_set_console(dev_console->parent.name, O_RDWR);
    #else
        libc_stdio_set_console(dev_console->parent.name, O_WRONLY);
    #endif
    }
#endif

    /* set PATH and HOME */
    putenv("PATH=/bin");
    putenv("HOME=/home");

#if defined RT_USING_PTHREADS && !defined RT_USING_COMPONENTS_INIT
    pthread_system_init();
#endif

    return 0;
}
INIT_COMPONENT_EXPORT(libc_system_init);

#if 0
//增加的一些文件操作
FILE * rtt_fopen(const char * path, const char * mode)
{
  	int flags = 0;
    int fd, result;
	FILE *d = RT_NULL;
 	RT_ASSERT(mode!=RT_NULL);
	if(!rt_strcmp(mode,C_FILE_O_TEXT_EXIST_FILE_FOR_RDONLY))
	{
		flags = O_RDONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_TRUNCATE_ZERO_OR_CREATE_FOR_WRONLY))
	{
	  	flags = O_TRUNC | O_CREAT | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_CREATE_FOR_WRONLY))
	{
	  	flags = O_CREAT | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_APPEND_FOR_WRONLY))
	{
        flags = O_APPEND | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_EXIST_FILE_FOR_RDONLY))
	{
        flags = O_BINARY | O_RDONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_WRONLY))
	{
        flags = O_BINARY | O_TRUNC | O_CREAT | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_CREATE_FOR_WRONLY))
	{
        flags = O_BINARY | O_CREAT | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_APPEND_FOR_WRONLY))
	{
        flags = O_BINARY | O_APPEND | O_WRONLY;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_EXIST_FILE_FOR_RDWR))
	{
        flags = O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR))
	{
        flags = O_TRUNC | O_CREAT | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_CREATE_FOR_RDWR))
	{
        flags = O_CREAT | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_TEXT_APPEND_FOR_RDWR))
	{
        flags = O_APPEND | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_EXIST_FILE_FOR_RDWR_1) || \
        !rt_strcmp(mode,C_FILE_O_BINARY_EXIST_FILE_FOR_RDWR_2))
	{
        flags = O_BINARY | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR_1) || \
        !rt_strcmp(mode,C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR_2))
	{
        flags = O_BINARY | O_TRUNC | O_CREAT | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_CREATE_FOR_RDWR_1) || \
        !rt_strcmp(mode,C_FILE_O_BINARY_CREATE_FOR_RDWR_2))
	{
        flags = O_BINARY | O_CREAT | O_RDWR;
	}
	else if(!rt_strcmp(mode,C_FILE_O_BINARY_APPEND_FOR_RDWR_1) || \
        !rt_strcmp(mode,C_FILE_O_BINARY_APPEND_FOR_RDWR_2))
	{
        flags = O_BINARY | O_APPEND | O_RDWR;
	}

    /* allocate a fd */
    fd = fd_new();
    if (fd < 0)
    {
        rt_set_errno(-ENOMEM);

        return NULL;
    }
    d = fd_get(fd);

    result = dfs_file_open(d, path, flags);
    if (result < 0)
    {
        /* release the ref-count of fd */
        fd_put(d);
        fd_put(d);

        rt_set_errno(result);

        return RT_NULL;
    }

    /* release the ref-count of fd */
    fd_put(d);

    return d;
}

char *rtt_fgets(char *buf, int bufsize, FILE *stream)
{
  	int readCounter = 0;
    int result;
    int realReadSize = bufsize - 1;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);

        return NULL;
    }

	while(readCounter<realReadSize)
	{
		result = dfs_file_read(stream, buf+readCounter, 1);
		if (result < 0)
		{
			rt_set_errno(result);
            return NULL;
		}
		else
		{
            //windows '\r\n' linux '\n' MacOS '\n'
		  	if(buf[readCounter] == '\n')
			{
				break;
			}
		}
		readCounter++;
	}
    buf[++readCounter] = '\0';
    return buf;
}

int rtt_fclose( FILE *fp )
{
    int result;

    if (fp == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    result = dfs_file_close(fp);

    if (result < 0)
    {
        rt_set_errno(result);
        return -1;
    }

    fd_put(fp);
    return 0;
}

int rtt_fflush(FILE *stream)
{
    int ret;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    ret = dfs_file_flush(stream);

    return ret;
}

size_t rtt_fread ( void *buffer, size_t size, size_t count, FILE *stream)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);

        return -1;
    }

    result = dfs_file_read(stream, buffer, size*count);
    if (result < 0)
    {
        rt_set_errno(result);

        return 0;
    }

    return result;
}

size_t rtt_fwrite(const void* buffer, size_t size, size_t count, FILE* stream)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);

        return -1;
    }

    result = dfs_file_write(stream, buffer, size*count);
    if (result < 0)
    {
        rt_set_errno(result);

        return -1;
    }

    return result;
}

int rtt_fseek(FILE *stream, long offset, int fromwhere)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);

        return -1;
    }

    switch (fromwhere)
    {
		case SEEK_SET:
			break;

		case SEEK_CUR:
			offset += stream->pos;
			break;

		case SEEK_END:
			offset += stream->size;
			break;

		default:
			rt_set_errno(-EINVAL);

			return -1;
    }

    if (offset < 0)
    {
        rt_set_errno(-EINVAL);

        return -1;
    }
    result = dfs_file_lseek(stream, offset);
    if (result < 0)
    {
        rt_set_errno(result);
        return -1;
    }

    return offset;
}

long rtt_ftell(FILE *stream)
{
    if (stream == NULL)
    {
        rt_set_errno(-EBADF);

        return -1;
    }
	return stream->pos;
}

void rtt_rewind(FILE *stream)
{
    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return;
    }
    rtt_fseek(stream, 0, SEEK_SET);
}

int rtt_fprintf(FILE* stream, char* fmt, ...)
{
    int ret = 0;
    int ret1 = 0;
    char buffer[FPRINTF_MAX_BUFFER_SIZE];
    va_list args;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    va_start(args, fmt);
    ret = rt_vsnprintf(buffer, FPRINTF_MAX_BUFFER_SIZE, fmt, args);
    va_end(args);

    ret1 = dfs_file_write(stream,buffer,ret);
    if (ret1 < 0)
    {
        rt_set_errno(ret1);
        return -1;
    }

    if(stdout!=RT_NULL)
    {
        if(stream==stdout)
        {
            rt_kprintf("%s",buffer);
            goto END_OF_OUTPUT;
        }
    }
    if(stderr!=RT_NULL)
    {
        if(stream==stderr)
        {
            rt_kprintf("%s",buffer);
            goto END_OF_OUTPUT;
        }
    }

END_OF_OUTPUT:
    return ret;
}

int rtt_putc(int ch, FILE *stream)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    result = dfs_file_write(stream, &ch, 1);
    if (result < 0)
    {
        rt_set_errno(result);
        return -1;
    }

    if(stdout!=RT_NULL)
    {
        if(stream==stdout)
        {
            rt_kprintf("%c",ch);
            goto END_OF_OUTPUT;
        }
    }
    if(stderr!=RT_NULL)
    {
        if(stream==stderr)
        {
            rt_kprintf("%c",ch);
            goto END_OF_OUTPUT;
        }
    }

END_OF_OUTPUT:
    return 0;
}

int rtt_fputc(int ch, FILE *stream)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    result = dfs_file_write(stream, &ch, 1);
    if (result < 0)
    {
        rt_set_errno(result);
        return -1;
    }

    if(stdout!=RT_NULL)
    {
        if(stream==stdout)
        {
            rt_kprintf("%c",ch);
            goto END_OF_OUTPUT;
        }
    }
    if(stderr!=RT_NULL)
    {
        if(stream==stderr)
        {
            rt_kprintf("%c",ch);
            goto END_OF_OUTPUT;
        }
    }

END_OF_OUTPUT:
    return 0;
}

int rtt_fputs(const char *s, FILE *stream)
{
    int result;

    if (stream == NULL)
    {
        rt_set_errno(-EBADF);
        return -1;
    }

    result = dfs_file_write(stream, s, strlen(s));
    if (result < 0)
    {
        rt_set_errno(result);
        return -1;
    }

    if(stdout!=RT_NULL)
    {
        if(stream==stdout)
        {
            rt_kprintf("%s",s);
            goto END_OF_OUTPUT;
        }
    }
    if(stderr!=RT_NULL)
    {
        if(stream==stderr)
        {
            rt_kprintf("%s",s);
            goto END_OF_OUTPUT;
        }
    }

END_OF_OUTPUT:
    return 0;
}
#endif