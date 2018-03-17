#ifndef FCNTL_H__
#define FCNTL_H__

#ifdef RT_USING_DFS
#include <dfs_posix.h>
#endif

#if 0

#include <rtconfig.h>
#include <dfs_file.h>

#define DFS_STDIN_FILE_NAME         "/stdin.txt"
#define DFS_STDOUT_FILE_NAME        "/stdout.txt"
#define DFS_STDERR_FILE_NAME        "/stderr.txt"

extern struct dfs_fd* dfs_stdin ;
extern struct dfs_fd* dfs_stdout;
extern struct dfs_fd* dfs_stderr;

#undef stdin
#undef stdout
#undef stderr

#define stdin   dfs_stdin
#define stdout  dfs_stdout
#define stderr  dfs_stderr

/*
r              open text file for reading
w              truncate to zero length or create text file for writing
wx             create text file for writing
a              append; open or create text file for writing at end-of-file
rb             open binary file for reading
wb             truncate to zero length or create binary file for writing
wbx            create binary file for writing
ab             append; open or create binary file for writing at end-of-file
r+             open text file for update (reading and writing)
w+             truncate to zero length or create text file for update
w+x            create text file for update
a+             append; open or create text file for update, writing at end-of-file
r+b  or rb+    open binary file for update (reading and writing)
w+b  or wb+    truncate to zero length or create binary file for update
w+bx or wb+x   create binary file for update
a+b  or ab+    append; open or create binary file for update, writing at end-of-file
*/

#define C_FILE_O_TEXT_EXIST_FILE_FOR_RDONLY                 "r"
#define C_FILE_O_TEXT_TRUNCATE_ZERO_OR_CREATE_FOR_WRONLY    "w"
#define C_FILE_O_TEXT_CREATE_FOR_WRONLY                     "wx"
#define C_FILE_O_TEXT_APPEND_FOR_WRONLY                     "a"
#define C_FILE_O_BINARY_EXIST_FILE_FOR_RDONLY               "rb"
#define C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_WRONLY  "wb"
#define C_FILE_O_BINARY_CREATE_FOR_WRONLY                   "wbx"
#define C_FILE_O_BINARY_APPEND_FOR_WRONLY                   "ab"
#define C_FILE_O_TEXT_EXIST_FILE_FOR_RDWR                   "r+"
#define C_FILE_O_TEXT_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR      "w+"
#define C_FILE_O_TEXT_CREATE_FOR_RDWR                       "w+x"
#define C_FILE_O_TEXT_APPEND_FOR_RDWR                       "a+"
#define C_FILE_O_BINARY_EXIST_FILE_FOR_RDWR_1               "r+b"
#define C_FILE_O_BINARY_EXIST_FILE_FOR_RDWR_2               "rb+"
#define C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR_1  "w+b"
#define C_FILE_O_BINARY_TRUNCATE_ZERO_OR_CREATE_FOR_RDWR_2  "wb+"
#define C_FILE_O_BINARY_CREATE_FOR_RDWR_1                   "w+bx"
#define C_FILE_O_BINARY_CREATE_FOR_RDWR_2                   "wb+x"
#define C_FILE_O_BINARY_APPEND_FOR_RDWR_1                   "a+b"
#define C_FILE_O_BINARY_APPEND_FOR_RDWR_2                   "ab+"

#define FPRINTF_MAX_BUFFER_SIZE         128

typedef struct dfs_fd RTT_FILE;

#undef FILE
#define FILE 	RTT_FILE

FILE * rtt_fopen(const char * path, const char * mode);
char *rtt_fgets(char *buf, int bufsize, FILE *stream);
int rtt_fclose( FILE *fp );
int rtt_fflush(FILE *stream);
size_t rtt_fread ( void *buffer, size_t size, size_t count, FILE *stream);
size_t rtt_fwrite(const void* buffer, size_t size, size_t count, FILE* stream);
int rtt_fseek(FILE *stream, long offset, int fromwhere);
long rtt_ftell(FILE *stream);
void rtt_rewind(FILE *stream);
int rtt_fprintf(FILE* stream, char* fmt, ...);
int rtt_putc(int ch, FILE *stream);
int rtt_fputc(int ch, FILE *stream);
int rtt_fputs(const char *s, FILE *stream);

#define fopen   rtt_fopen
#define fgets   rtt_fgets
#define fclose  rtt_fclose
#define fflush  rtt_fflush
#define fread   rtt_fread
#define fwrite  rtt_fwrite
#define fseek   rtt_fseek
#define ftell   rtt_ftell
#define rewind  rtt_rewind
#define fprintf rtt_fprintf
#define putc    rtt_putc
#define fputc   rtt_fputc
#define fputs   rtt_fputs

//int setvbuf(FILE * __restrict, char *__restrict, int, size_t);
#endif

#endif
