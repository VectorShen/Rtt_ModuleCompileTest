#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <libc/libc_fcntl.h>

#define FIFO_PATH_PREFIX						"/dev/"
#define FIFO_PATH_BUFFER_LEN					20

#define NPI_IPC_LISTEN_PIPE_SERVER2CLIENT   	"npi_ipc_s2c"
#define NPI_IPC_LISTEN_PIPE_CLIENT2SERVER   	"npi_ipc_c2s"

#define NPI_IPC_LISTEN_PIPE_CHECK_STRING    	"connect_to_npi_ipc"

#define ZLSZNP_LISTEN_PIPE_SERVER2CLIENT    	"zlsznp_s2c"
#define ZLSZNP_LISTEN_PIPE_CLIENT2SERVER    	"zlsznp_c2s"

#define ZLSZNP_LISTEN_PIPE_CHECK_STRING     	"connect_to_zlsznp"

#define NWKMGR_LISTEN_PIPE_SERVER2CLIENT    	"nwkmgr_s2c"
#define NWKMGR_LISTEN_PIPE_CLIENT2SERVER    	"nwkmgr_c2s"

#define NWKMGR_LISTEN_PIPE_CHECK_STRING     	"connect_to_nwkmgr"

#define OTASERVER_LISTEN_PIPE_SERVER2CLIENT 	"otaserver_s2c"
#define OTASERVER_LISTEN_PIPE_CLIENT2SERVER 	"otaserver_c2s"

#define OTASERVER_LISTEN_PIPE_CHECK_STRING  	"connect_to_otaserver"

#define GATEWAY_LISTEN_PIPE_SERVER2CLIENT   	"gateway_s2c"
#define GATEWAY_LISTEN_PIPE_CLIENT2SERVER   	"gateway_c2s"

#define GATEWAY_LISTEN_PIPE_CHECK_STRING    	"connect_to_gateway"

#define SERVER_NPI_IPC                      	"srv_npi_ipc"
#define SERVER_ZLSZNP                       	"srv_zlsznp"
#define SERVER_NWKMGR                       	"srv_nwkmgr"
#define SERVER_OTASERVER                    	"srv_otaserver"
#define SERVER_GATEWAY                      	"srv_gateway"

#define TMP_PIPE_NAME_SIZE				        50
#define TMP_ASSIGNED_ID_STRING_LEN              3
#define TMP_PIPE_CHECK_STRING_LEN               30
#define SERVER_LISTEN_BUF_SIZE			        30

#define ZB_LISTEN_PIPE_OPEN_FLAG				(O_RDWR | O_NONBLOCK)
#define ZB_CLIENT_LISTEN_PIPE_READ_FLAG			(O_RDWR)
#define ZB_READ_PIPE_OPEN_FLAG					(O_RDWR | O_NONBLOCK)

typedef enum
{
    SERVER_NPI_IPC_ID,
    SERVER_ZLSZNP_ID,
    SERVER_NWKMGR_ID,
    SERVER_OTASERVER_ID,
    SERVER_GATEWAY_ID
}emServerId;

#endif /* _CONFIG_H_ */
