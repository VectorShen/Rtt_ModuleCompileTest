/*********************************************************************
 Filename:			 api_server.c
 Revised:				$Date$
 Revision:			 $Revision$

 Description:		API Server module

 Copyright 2013 - 2014 Texas Instruments Incorporated. All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License").	You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless used solely and exclusively in conjunction with
 a Texas Instruments radio frequency device, which is integrated into
 your product.	Other than for the foregoing purpose, you may not use,
 reproduce, copy, prepare derivative works of, modify, distribute, perform,
 display or sell this Software and/or its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.
 *********************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>
#include <errno.h>

#include "hal_rpc.h"
#include "api_server.h"

#undef SERVER_NAME
#define SERVER_NAME ZSTACKZNP_SRVR
#include "trace.h"
#include <dfs_select.h>
/*********************************************************************
 * MACROS
 */
#if !defined( MAX )
#define MAX(a,b)		((a > b) ? a : b)
#endif

#define NPI_LNX_ERROR_MODULE(a)		 ((uint8)((a & 0xFF00) >> 8))
#define NPI_LNX_ERROR_THREAD(a)		 ((uint8)(a & 0x00FF))

/*********************************************************************
 * CONSTANTS
 */

#if !defined ( APIS_PIPE_QUEUE_SIZE )
#define APIS_PIPE_QUEUE_SIZE 4
#endif

#define APIS_LNX_SUCCESS 0
#define APIS_LNX_FAILURE 1

#define APIS_ERROR_PIPE_GET_ADDRESS_INFO                                                -1
#define APIS_ERROR_IPC_PIPE_SET_REUSE_ADDRESS                                           -2
#define APIS_ERROR_IPC_PIPE_BIND                                                        -3
#define APIS_ERROR_IPC_PIPE_LISTEN                                                      -4
#define APIS_ERROR_IPC_PIPE_SELECT_CHECK_ERRNO                                          -5
#define APIS_ERROR_IPC_PIPE_ACCEPT                                                      -6

#define APIS_ERROR_IPC_ADD_TO_ACTIVE_LIST_NO_ROOM						 				-7
#define APIS_ERROR_IPC_REMOVE_FROM_ACTIVE_LIST_NOT_FOUND								-8
#define APIS_ERROR_IPC_RECV_DATA_DISCONNECT									 			-9
#define APIS_ERROR_IPC_RECV_DATA_CHECK_ERRNO											-10
#define APIS_ERROR_IPC_RECV_DATA_TOO_FEW_BYTES											-11
#define APIS_ERROR_HAL_GPIO_WAIT_SRDY_CLEAR_POLL_TIMEDOUT		 						-12
#define APIS_ERROR_IPC_RECV_DATA_INCOMPATIBLE_CMD_TYPE									-13
#define APIS_ERROR_IPC_SEND_DATA_ALL													-14
#define APIS_ERROR_IPC_SEND_DATA_SPECIFIC_PIPE_REMOVED									-15
#define APIS_ERROR_IPC_SEND_DATA_SPECIFIC										 		-16
#define APIS_ERROR_MALLOC_FAILURE														-17

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
bool APIS_initialized = FALSE;
size_t APIS_threadStackSize = (PTHREAD_STACK_MIN * 3);	 // 16k*3

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

int listenPipeReadHndl;
int listenPipeWriteHndl;

static int apisErrorCode = 0;

char *toAPISLnxLog = NULL;

fd_set activePipesFDs;
fd_set activePipesFDsSafeCopy;
int fdmax;

typedef struct _pipeinfo_t
{
	struct _pipeinfo_t *next;
	bool garbage;
	pthread_mutex_t mutex;
	int serverReadPipe;
	int serverWritePipe;
} Pipe_t;

int api_server_clientsNum = 1;

Pipe_t *activePipeList = NULL;
size_t activePipeCount = 0;
pthread_mutex_t aclMutex = PTHREAD_MUTEX_INITIALIZER;
size_t aclNoDeleteCount = 0;
pthread_cond_t aclDeleteAllowCond = PTHREAD_COND_INITIALIZER;

struct
{
	int list[APIS_PIPE_QUEUE_SIZE];
	int size;
} activePipes;

static pthread_t listeningThreadId;

static char* logPath = NULL;

static pfnAPISMsgCB apisMsgCB = NULL;

static apic16BitLenMsgHdr_t apisHdrBuf;
static char listenReadPipePathName[APIC_READWRITE_PIPE_NAME_LEN];
static char listenReadPipePathBuffer[APIC_READWRITE_PIPE_NAME_LEN];
/*********************************************************************
 * LOCAL PROTOTYPES
 */

void *apislisteningThreadFunc( void *ptr );

static int apisSendData( uint16 len, uint8 *pData, int writePipe );
static int createReadWritePipes( emServerId serverId );
//static void apiServerExit( int exitCode );
static void startupInfo( void );
static void writetoAPISLnxLog( const char* str );
static int addToActiveList( int readPipe, int writePipe );
static int apisPipeHandle( int readPipe );
static int searchWritePipeFromReadPipe( int readPipe);

emServerId id;

/*********************************************************************
 * Public Functions
 */

/*********************************************************************
 * APIS_Init - Start and maintain an API Server
 *
 * public API is defined in api_server.h.
 *********************************************************************/
bool APIS_Init( emServerId serverId, bool verbose, pfnAPISMsgCB pfCB )
{
	id = serverId;
	pthread_attr_t attr;

	apisMsgCB = pfCB;

	if ( verbose )
	{
		startupInfo();
	}

	// prepare thread creation
	if ( pthread_attr_init( &attr ) )
	{
		perror( "pthread_attr_init" );
		return (FALSE);
	}

	if ( pthread_attr_setstacksize( &attr, APIS_threadStackSize ) )
	{
		perror( "pthread_attr_setstacksize" );
		return (FALSE);
	}

	// Create a listening socket
	if ( createReadWritePipes(serverId) < 0 )
	{
		return (FALSE);
	}

	if ( verbose )
	{
		uiPrintfEx(trINFO,  "waiting for connect pipe on #%d...\n", listenPipeReadHndl );
	}

	// iCreate thread for listening
	if ( pthread_create( &listeningThreadId, &attr, apislisteningThreadFunc,
			NULL ) )
	{
		// thread creation failed
		uiPrintfEx(trINFO,  "Failed to create an API Server Listening thread\n" );
		return (FALSE);
	}

	APIS_initialized = TRUE;

	return (TRUE);
}

/*********************************************************************
 * APIS_SendData - Send a packet to a pipe
 *
 * public API is defined in api_server.h.
 *********************************************************************/
void APIS_SendData( int writePipe, uint8 sysID, bool areq, uint8 cmdId,
										uint16 len, uint8 *pData )
{
	size_t msglen = len;
	apic16BitLenMsgHdr_t *pMsg;

	if ( len > (uint16)( len + sizeof(apic16BitLenMsgHdr_t) ) )
	{
		uiPrintfEx(trINFO,  "[ERR] APIS_SendData() called with length exceeding max allowed" );
		return;
	}
	msglen += sizeof(apic16BitLenMsgHdr_t);

	pMsg = malloc( msglen );
	if ( !pMsg )
	{
		// TODO: abort
		uiPrintfEx(trINFO,  "[ERR] APIS_SendData() failed to allocate memory block" );
		return;
	}

	pMsg->lenL = (uint8) len;
	pMsg->lenH = (uint8)( len >> 8 );
	if ( areq )
	{
		pMsg->subSys = (uint8)( RPC_CMD_AREQ | sysID );
	}
	else
	{
		pMsg->subSys = (uint8)( RPC_CMD_SRSP | sysID );
	}
	pMsg->cmdId = cmdId;

	memcpy( pMsg + 1, pData, len );

	apisSendData( (len + sizeof(apic16BitLenMsgHdr_t)), (uint8 *) pMsg,
			writePipe );

	free( pMsg );
}

/*********************************************************************
 * Local Functions
 */

/*********************************************************************
 * @fn			reserveActivePipe
 *
 * @brief	 Reserve an active pipe so that active pipe
 *					list does not change and also the send() to the active
 *					pipe is performed in sequence.
 *
 * @param	 pipe - pipe
 *
 * @return	0 when successful. -1 when failed.
 *
 *********************************************************************/
static int reserveActivePipe( int writePipe )
{
	Pipe_t *entry;

	if ( pthread_mutex_lock( &aclMutex ) != 0 )
	{
		perror( "pthread_mutex_lock" );
		exit( 1 );
	}
	entry = activePipeList;
	while ( entry )
	{
		if ( entry->serverWritePipe == writePipe )
		{
			aclNoDeleteCount++;
			pthread_mutex_unlock( &aclMutex );
			if ( pthread_mutex_lock( &entry->mutex ) != 0 )
			{
				perror( "pthread_mutex_lock" );
				exit( 1 );
			}
			return 0;
		}
		entry = entry->next;
	}
	pthread_mutex_unlock( &aclMutex );
	return -1;
}

/*********************************************************************
 * @fn			unreserveActivePipe
 *
 * @brief	 Free a reserved active pipe.
 *
 * @param	 pipe - pipe
 *
 * @return	None
 *
 *********************************************************************/
static void unreserveActivePipe( int writePipe )
{
	Pipe_t *entry;

	if ( pthread_mutex_lock( &aclMutex ) != 0 )
	{
		perror( "pthread_mutex_lock" );
		exit( 1 );
	}
	entry = activePipeList;
	while ( entry )
	{
		if ( entry->serverWritePipe == writePipe )
		{
			pthread_mutex_unlock( &entry->mutex );
			if ( --aclNoDeleteCount == 0 )
			{
				pthread_cond_signal( &aclDeleteAllowCond );
			}
			break;
		}
		entry = entry->next;
	}
	pthread_mutex_unlock( &aclMutex );
}

/*********************************************************************
 * @fn			dropActivePipe
 *
 * @brief	 Mark an active pipe as a garbage
 *
 * @param	 pipe - pipe
 *
 * @return	None
 *
 *********************************************************************/
static void dropActivePipe( int pipe )
{
	Pipe_t *entry;

	if ( pthread_mutex_lock( &aclMutex ) != 0 )
	{
		perror( "pthread_mutex_lock" );
		exit( 1 );
	}
	entry = activePipeList;
	while ( entry )
	{
		if ( entry->serverReadPipe == pipe || entry->serverWritePipe == pipe)
		{
			entry->garbage = TRUE;
            close( entry->serverReadPipe );
			close( entry->serverWritePipe );
			break;
		}
		entry = entry->next;
	}
	pthread_mutex_unlock( &aclMutex );
}

/*********************************************************************
 * @fn			aclGarbageCollect
 *
 * @brief	 Garbage collect connections from active pipe list
 *
 * @param	 None
 *
 * @return	None
 *
 *********************************************************************/
static void aclGarbageCollect( void )
{
	Pipe_t **entryPtr;
	Pipe_t *deletedlist = NULL;

	if ( pthread_mutex_lock( &aclMutex ) != 0 )
	{
		perror( "pthread_mutex_lock" );
		exit( 1 );
	}

	for ( ;; )
	{
		if ( !aclNoDeleteCount )
		{
			break;
		}
		pthread_cond_wait( &aclDeleteAllowCond, &aclMutex );
	}

	entryPtr = &activePipeList;
	while ( *entryPtr )
	{
		if ( (*entryPtr)->garbage )
		{
			Pipe_t *deleted = *entryPtr;
			*entryPtr = deleted->next;
			close( deleted->serverReadPipe );
            close( deleted->serverWritePipe );
			FD_CLR( deleted->serverReadPipe, &activePipesFDs );
			pthread_mutex_destroy( &deleted->mutex );
			deleted->next = deletedlist;
			deletedlist = deleted;
			continue;
		}
		entryPtr = &(*entryPtr)->next;
	}
	pthread_mutex_unlock( &aclMutex );

	// Indicate pipe is done
	while ( deletedlist )
	{
		Pipe_t *deleted = deletedlist;
		deletedlist = deletedlist->next;

		if ( apisMsgCB )
		{
			apisMsgCB( deleted->serverWritePipe, 0, 0, 0xFFFFu, NULL, SERVER_DISCONNECT );
		}
		free( deleted );
		activePipeCount--;
	}
}

/*********************************************************************
 * @fn			apisSendData
 *
 * @brief	 Send Packet
 *
 * @param	 len - length to send
 * @param	 pData - pointer to buffer to send
 * @param	 pipe - pipe to send message (for synchronous
 *											 response) otherwise -1 for all connections
 *
 * @return	status
 *
 *********************************************************************/
static int apisSendData( uint16 len, uint8 *pData, int writePipe )
{
	int bytesSent = 0;
	int ret = APIS_LNX_SUCCESS;

	printf("\napisSendData to pipeNum %d.\n", writePipe);

	// Send to all connections?
	if ( writePipe < 0 )
	{
		// retain the list
		Pipe_t *entry;

		if ( pthread_mutex_lock( &aclMutex ) != 0 )
		{
			perror( "pthread_mutex_lock" );
			exit( 1 );
		}

		entry = activePipeList;

		// Send data to all connections, except listener
		while ( entry )
		{
            // Repeat till all data is sent out
            if ( pthread_mutex_lock( &entry->mutex ) != 0 )
            {
                perror( "pthread_mutex_lock" );
                exit( 1 );
            }

            for ( ;; )
            {
				bytesSent = write( entry->serverWritePipe, pData, len );
				if ( bytesSent < 0 )
				{
					char *errorStr = (char *) malloc( 30 );
					sprintf( errorStr, "send %d, %d", entry->serverWritePipe, errno );
					perror( errorStr );
					// Remove from list if detected bad file descriptor
					if ( errno == EBADF )
					{
						uiPrintfEx(trINFO,  "Removing pipe #%d\n", entry->serverWritePipe );
						entry->garbage = TRUE;
					}
					else
					{
						apisErrorCode = APIS_ERROR_IPC_SEND_DATA_ALL;
						ret = APIS_LNX_FAILURE;
					}
				}
				else if ( bytesSent < len )
				{
					pData += bytesSent;
					len -= bytesSent;
					continue;
				}
				break;
            }
            pthread_mutex_unlock( &entry->mutex );
			entry = entry->next;
		}
		pthread_mutex_unlock( &aclMutex );
	}
	else
	{
		// Protect thread against asynchronous deletion
		if ( reserveActivePipe( writePipe ) == 0 )
		{

			// Repeat till all data is sent
			for ( ;; )
			{
				// Send to specific pipe only
				bytesSent = write( writePipe, pData, len );

				uiPrintfEx(trINFO, "...sent %d bytes to Client\n", bytesSent );

				if ( bytesSent < 0 )
				{
					perror( "send" );
					// Remove from list if detected bad file descriptor
					if ( errno == EBADF )
					{
						uiPrintfEx(trINFO,  "Removing pipe #%d\n", writePipe );
						dropActivePipe( writePipe );
						// Pipe closed. Remove from set
						apisErrorCode =
								APIS_ERROR_IPC_SEND_DATA_SPECIFIC_PIPE_REMOVED;
						ret = APIS_LNX_FAILURE;
					}
				}
				else if ( bytesSent < len )
				{
					pData += bytesSent;
					len -= bytesSent;
					continue;
				}
				break;
			}
			unreserveActivePipe( writePipe );
		}
		else
		{
			printf("reserveActivePipe failed for writePipe is %d.\n", writePipe);
		}
	}

	return (ret);
}

/*********************************************************************
 * @fn			createReadWritePipes
 *
 * @brief	 Create an API Server Listening socket
 *
 * @param	 ptr -
 *
 * @return	none
 *
 *********************************************************************/
static int createReadWritePipes( emServerId serverId )
{
	apisErrorCode = 0;

	//mkfifo and open pipes
	switch(serverId)
    {
        case SERVER_ZLSZNP_ID:
            strncpy(listenReadPipePathName,ZLSZNP_LISTEN_PIPE_CLIENT2SERVER,strlen(ZLSZNP_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_NWKMGR_ID:
            strncpy(listenReadPipePathName,NWKMGR_LISTEN_PIPE_CLIENT2SERVER,strlen(NWKMGR_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_OTASERVER_ID:
            strncpy(listenReadPipePathName,OTASERVER_LISTEN_PIPE_CLIENT2SERVER,strlen(OTASERVER_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_GATEWAY_ID:
            strncpy(listenReadPipePathName,GATEWAY_LISTEN_PIPE_CLIENT2SERVER,strlen(GATEWAY_LISTEN_PIPE_CLIENT2SERVER));
            break;
        default:
            //error
            break;
    }
	if ((mkfifo (listenReadPipePathName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		printf ("cannot create fifo %s\n", listenReadPipePathName);
	}
	memset(listenReadPipePathBuffer, '\0', APIC_READWRITE_PIPE_NAME_LEN);
	sprintf(listenReadPipePathBuffer, "%s%s", FIFO_PATH_PREFIX, listenReadPipePathName);
	//闂傚倸鍊搁悧蹇涘磻閵堝憘鐔煎幢濡偐鐣介梺缁樺姈濞兼瑩鍩呴悽纰夋嫹濞堝灝娅欓柟椋庡厴閹綊宕堕鍕優濡炪倖鎸婚幃鍌炲蓟閸℃稑绠奸柛鏇ㄥ幘椤︻噣姊洪崨濠傚闁荤啙鍥舵晣闁归棿鐒﹂悞濂告煙閹规劕鐓愰柣锝変憾閺岀喖宕归锝呯缂備浇椴搁崹褰掑箟閻楀牊濯撮柧蹇氼潐閹綁鏌ｉ悩鍙夌┛鐎规洜鏁婚、姗�骞栨担鍦姸闂佸湱鍋撳娆愬緞瀹ュ鐓涢柨鐔烘櫕閹瑰嫰宕崟顓炲笓闂備浇妗ㄩ懗鑸垫櫠濡わ拷閻ｅ灚绗熼敓钘夌暦濡警鍚嬮柛鈩兩戦崕銉︾箾鏉堝墽绉堕柟鍑ゆ嫹
	listenPipeReadHndl = open (listenReadPipePathBuffer, ZB_LISTEN_PIPE_OPEN_FLAG, 0);
	if (listenPipeReadHndl == -1)
	{
		printf ("open %s for read error\n", listenReadPipePathBuffer);
		exit (-1);
	}

	// Clear file descriptor sets
	FD_ZERO( &activePipesFDs );
	FD_ZERO( &activePipesFDsSafeCopy );

	// Add the listener to the set
	FD_SET( listenPipeReadHndl, &activePipesFDs );
	fdmax = listenPipeReadHndl;

	toAPISLnxLog = (char *) malloc( AP_MAX_BUF_LEN );

	return (apisErrorCode);
}

/*********************************************************************
 * @fn			apislisteningThreadFunc
 *
 * @brief	 API Server listening thread
 *
 * @param	 ptr -
 *
 * @return	none
 *
 *********************************************************************/
void *apislisteningThreadFunc( void *ptr )
{
	bool done = FALSE;
	int ret;
	int c;
    int n;
	struct timeval *pTimeout = NULL;
	char listen_buf[SERVER_LISTEN_BUF_SIZE];

	char tmpReadPipeName[TMP_PIPE_NAME_SIZE];
	char tmpWritePipeName[TMP_PIPE_NAME_SIZE];
    char readPipePathName[APIS_READWRITE_PIPE_NAME_LEN];
	char writePipePathName[APIS_READWRITE_PIPE_NAME_LEN];
    char assignedId[APIS_ASSIGNED_ID_BUF_LEN];
	int tmpReadPipe;
	int tmpWritePipe;

    memset(writePipePathName,'\0',APIS_READWRITE_PIPE_NAME_LEN);
    memset(readPipePathName,'\0',APIS_READWRITE_PIPE_NAME_LEN);
    switch(id)
    {
        case SERVER_NPI_IPC_ID:
            strncpy(writePipePathName,NPI_IPC_LISTEN_PIPE_SERVER2CLIENT,strlen(NPI_IPC_LISTEN_PIPE_SERVER2CLIENT));
            strncpy(readPipePathName,NPI_IPC_LISTEN_PIPE_CLIENT2SERVER,strlen(NPI_IPC_LISTEN_PIPE_CLIENT2SERVER));
			break;
        case SERVER_ZLSZNP_ID:
            strncpy(writePipePathName,ZLSZNP_LISTEN_PIPE_SERVER2CLIENT,strlen(ZLSZNP_LISTEN_PIPE_SERVER2CLIENT));
            strncpy(readPipePathName,ZLSZNP_LISTEN_PIPE_CLIENT2SERVER,strlen(ZLSZNP_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_NWKMGR_ID:
            strncpy(writePipePathName,NWKMGR_LISTEN_PIPE_SERVER2CLIENT,strlen(NWKMGR_LISTEN_PIPE_SERVER2CLIENT));
            strncpy(readPipePathName,NWKMGR_LISTEN_PIPE_CLIENT2SERVER,strlen(NWKMGR_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_OTASERVER_ID:
            strncpy(writePipePathName,OTASERVER_LISTEN_PIPE_SERVER2CLIENT,strlen(OTASERVER_LISTEN_PIPE_SERVER2CLIENT));
            strncpy(readPipePathName,OTASERVER_LISTEN_PIPE_CLIENT2SERVER,strlen(OTASERVER_LISTEN_PIPE_CLIENT2SERVER));
            break;
        case SERVER_GATEWAY_ID:
            strncpy(writePipePathName,GATEWAY_LISTEN_PIPE_SERVER2CLIENT,strlen(GATEWAY_LISTEN_PIPE_SERVER2CLIENT));
            strncpy(readPipePathName,GATEWAY_LISTEN_PIPE_CLIENT2SERVER,strlen(GATEWAY_LISTEN_PIPE_CLIENT2SERVER));   
            break;
    }

	trace_init_thread("LSTN");
	//
	do
	{
		activePipesFDsSafeCopy = activePipesFDs;

		// First use select to find activity on the sockets
		if ( select( fdmax + 1, &activePipesFDsSafeCopy, NULL, NULL,
				pTimeout ) == -1 )
		{
			if ( errno != EINTR )
			{
				perror( "select" );
				apisErrorCode = APIS_ERROR_IPC_PIPE_SELECT_CHECK_ERRNO;
				ret = APIS_LNX_FAILURE;
				break;
			}
			continue;
		}

		// Then process this activity
		for ( c = 0; c <= fdmax; c++ )
		{
			if ( FD_ISSET( c, &activePipesFDsSafeCopy ) )
			{
                if( c == listenPipeReadHndl )
                {
					//闂備浇顫夋禍浠嬪磿閺屻儱鏋侀柛锔诲弿閹风兘鐛崹顔句痪闂佺硶鏅滅粙鎾跺垝閳哄拋鏁嶆繝濠傞閹線姊绘笟鍥у闁瑰憡鍎抽埢鎾诲箣閿曪拷閺嬩線鏌ｅΔ锟介悧鍡欑矈閿燂拷
					memset(listen_buf,'\0',SERVER_LISTEN_BUF_SIZE);
					n = read( listenPipeReadHndl, listen_buf, SERVER_LISTEN_BUF_SIZE );
					if ( n <= 0 )
					{
						if ( n < 0 )
						{
							perror( "read" );
                            apisErrorCode = APIS_ERROR_IPC_RECV_DATA_CHECK_ERRNO;
                            ret = APIS_LNX_FAILURE;
						}
						else
						{
							uiPrintfEx(trINFO, "Will disconnect #%d\n", listenPipeReadHndl );
							printf("srvwapper server will reconnect\n");
							apisErrorCode = APIS_ERROR_IPC_RECV_DATA_DISCONNECT;
							//ret = APIS_LNX_FAILURE;
							close(listenPipeReadHndl);
							FD_CLR(listenPipeReadHndl, &activePipesFDs);
    						listenPipeReadHndl = open (listenReadPipePathName, O_RDONLY | O_NONBLOCK, 0);
    						if (listenPipeReadHndl == -1)
    						{
        						printf ("open %s for read error\n", listenReadPipePathName);
        						exit (-1);
    						}
                           	FD_SET( listenPipeReadHndl, &activePipesFDs );
                            if ( listenPipeReadHndl > fdmax )
                            {
                            	fdmax = listenPipeReadHndl;
                         	}
						}
					}
					else
					{
                        ret = APIS_LNX_SUCCESS;
						listen_buf[n]='\0';
						printf("read %d bytes from listenPipeReadHndl of string is %s.\n",n,listen_buf);
                        switch(id)
                        {
                            case SERVER_ZLSZNP_ID:
                                if(strncmp(listen_buf,ZLSZNP_LISTEN_PIPE_CHECK_STRING,strlen(ZLSZNP_LISTEN_PIPE_CHECK_STRING)))
                                {
                                    ret = APIS_LNX_FAILURE;
                                }
                                break;
                            case SERVER_NWKMGR_ID:
                                if(strncmp(listen_buf,NWKMGR_LISTEN_PIPE_CHECK_STRING,strlen(NWKMGR_LISTEN_PIPE_CHECK_STRING)))
                                {
                                    ret = APIS_LNX_FAILURE;
                                }
                                break;
                            case SERVER_OTASERVER_ID:
                                if(strncmp(listen_buf,OTASERVER_LISTEN_PIPE_CHECK_STRING,strlen(OTASERVER_LISTEN_PIPE_CHECK_STRING)))
                                {
                                    ret = APIS_LNX_FAILURE;
                                }
                                break;
                            case SERVER_GATEWAY_ID:
                                if(strncmp(listen_buf,GATEWAY_LISTEN_PIPE_CHECK_STRING,strlen(GATEWAY_LISTEN_PIPE_CHECK_STRING)))
                                {
                                    ret = APIS_LNX_FAILURE;
                                }
                                break;
                            case SERVER_NPI_IPC_ID:
                                if(strncmp(listen_buf,NPI_IPC_LISTEN_PIPE_CHECK_STRING,strlen(NPI_IPC_LISTEN_PIPE_CHECK_STRING)))
                                {
                                    ret = APIS_LNX_FAILURE;
                                }
                                break;
                        }
						if( ret == APIS_LNX_SUCCESS )
						{
							//闂備礁鎼�氱兘宕归柆宥庢晢婵犲﹤鎳忛崗婊勩亜閺冨倸浜鹃柣锝変憾閹鐛崹顔句痪闂佺硶鏅滅粙鎾跺垝閳哄拋鏁嶆繝濠傞閹線姊绘笟鍥у闁归�涜兌閹广垹鈽夐姀鐘殿唺闂侀潧顧�闂勫嫬鈻撻崼鏇熺厵闁谎冩啞鐎氾拷
							//闂備胶鎳撻悘姘跺箰閸濄儲顫曢柟杈剧畱缁�鍐╃箾閸℃ê鐒鹃柛姘炬嫹闂傚倷绶￠崜姘跺箹椤愩倖顫曟繝闈涱儏缁�鍐╃箾閸℃绠扮�殿喗濞婇弻鈩冩媴閸濆嫷鏆悗瑙勬尫缁舵岸寮鍥︽勃闁稿﹦鍠庨悵顖炴⒑閻熸壆浠涢柟绋挎憸濡叉劙骞嬮敂钘夊壆缂備礁顑堝▔鏇狅拷鐟邦槺缁辨捇宕掑☉姘吂閻熸粍濯藉▔鏇犲垝婵犳碍鏅搁柣妯哄船椤ユ岸姊哄Ч鍥у閻庢凹鍨堕、姘閺夋垹顦ㄥ┑鐐叉閸旀洘瀵兼惔銊ョ骇闁冲搫鍊搁ˉ瀣亜閹存粎绐旈柡浣哥Ф娴狅箓鎸婃径妯诲闂備胶顭堝ù姘枍閺夛拷 select闂備焦鐪归崝宀�锟芥凹鍘鹃弫顕�宕奸弴鐐殿槴闂佸壊鍋呭ú鏍р枍閿燂拷
                            //闂傚倸鍊搁崯鎶筋敋瑜旈妴渚�鏁愭径濠勵吋闂佽鍎崇壕顓犲婵犳碍鐓ユ繛鎴烆焽閻掑憡淇婇懠棰濆殭妞ゎ偁鍨介敐鐐侯敇閻樺灚鍠掗梻浣告啞閼归箖鎯屾担鐑樺床婵☆垵鍋愰惌娆撴倵閿濆骸骞樼紒鐘劦閹泛鈽夊Ο鍨伃闂佸湱鍎甸弲鐘荤嵁閸儱顫呴柕鍫濇４缁鳖亝淇婇銏犵盎闁瑰嚖鎷�
							printf("writePipePathName is %s.\n", writePipePathName);
							listenPipeWriteHndl=open(writePipePathName, O_WRONLY, 0);
							if(listenPipeWriteHndl==-1)
							{
								if(errno==ENXIO)
								{
									printf("open error; no reading process\n");	
									ret = APIS_LNX_FAILURE;
									break;
								}
							}
                            sprintf(assignedId,"%d",api_server_clientsNum);

							//闂備礁鎼ú銈夋偤閵娾晛钃熷┑鐘插娑撳秹鏌ㄩ悢鍝勑″瑙勬礋閺岋繝宕掑顓犵厬濠电偛妫庨崹钘夌暦閵夆晜鏅搁柨鐕傛嫹
							memset(tmpReadPipeName, '\0', TMP_PIPE_NAME_SIZE);
							memset(tmpWritePipeName, '\0', TMP_PIPE_NAME_SIZE);
							sprintf(tmpReadPipeName, "%s%d", readPipePathName, api_server_clientsNum);
							sprintf(tmpWritePipeName, "%s%d", writePipePathName, api_server_clientsNum);

                            api_server_clientsNum++;
                            
							//闂傚倸鍊搁悧蹇涘磻閵堝憘鐔煎幢濡偐鐣介梺缁樺姈瑜板啴鎮橀敍鍕舵嫹閻愮懓锟芥稑鈻嶉敐澶樻晣闁归棿鐒﹂悞濂告煥閻曞倹瀚�
							if((mkfifo(tmpReadPipeName,O_CREAT|O_EXCL)<0)&&(errno!=EEXIST))
							{
								printf("cannot create fifo %s\n", tmpReadPipeName);
							}		
							if((mkfifo(tmpWritePipeName,O_CREAT|O_EXCL)<0)&&(errno!=EEXIST))
							{
								printf("cannot create fifo %s\n", tmpWritePipeName);
							}	
							//闂傚倸鍊搁悧蹇涘磻閵堝憘鐔煎幢濡偐鐣介梺缁樺姈濞兼瑩鍩呴悽纰夋嫹濞堝灝娅欓柟椋庡厴閹綊宕堕鍕優濡炪倖鎸婚幃鍌炲蓟閸℃稒鏅搁柨鐕傛嫹
							tmpReadPipe = open(tmpReadPipeName, O_RDONLY|O_NONBLOCK, 0);
							if(tmpReadPipe == -1)
							{
								printf("open %s for read error\n", tmpReadPipeName);
								ret = APIS_LNX_FAILURE;
								break;
							}
							
                            //闂備礁鎲￠崝鏍偡閵夆晛鐭楅柛鈩冪♁閸庡酣鏌熺�涙绠栭柛搴㈡⒒缁辨帡鎮崨顖滅槇婵炲瓨绮ｉ幏锟�
							write(listenPipeWriteHndl, assignedId, strlen(assignedId));

							//闂傚倸鍊搁崯鎶筋敋瑜旈妴渚�鏁愭径濠勵吋闂佽鍎崇壕顓犲婵犳碍鐓曢柟閭﹀幘閹冲懏銇勯幘瀛樺�愰柡灞芥閺佹捇鏁撻敓锟�
							tmpWritePipe = open(tmpWritePipeName, O_WRONLY, 0);
							if(tmpWritePipe == -1)
							{
								printf("open %s for write error\n", tmpWritePipeName);
								ret = APIS_LNX_FAILURE;
								break;
							}
							//闂佽崵濮村ú鈺咁敋瑜斿畷顖炲箻閸撲椒绗夐梺璺ㄥ枔婵櫕绔熼弴銏＄厵閻犲洩灏欑粻娲煕椤愵偂绨界紒顕呭弮閺佸啴鏁撻懞銉﹀弿闁绘劕鎼粈鍌炴煏婢跺牆鍔氶柣鎾茬箻ctivelist濠电偞鍨堕幖顐﹀箯閿燂拷
							ret = addToActiveList(tmpReadPipe, tmpWritePipe);
							if ( ret != APIS_LNX_SUCCESS )
							{
								// Adding to the active connection list failed.
								// Close the accepted connection.
								close( tmpReadPipe );
								close( tmpWritePipe );
							}
							else
							{
								FD_SET( tmpReadPipe, &activePipesFDs );
								if ( tmpReadPipe > fdmax )
								{
									fdmax = tmpReadPipe;
								}

								apisMsgCB( searchWritePipeFromReadPipe(tmpReadPipe), 0, 0, 0, NULL, SERVER_CONNECT );
							}
							//闂備胶顭堢换鎴炵箾婵犲伣娑㈠箻椤旇棄鍓梺鐟扮摠缁诲啴宕曢弮鍫熺厸闁告劑鍔岄。铏箾閸喎鐏寸�规洘锚閳诲酣骞嬮鐐村�庨梻鍌欑贰閸撱劑骞忛敓锟�
							close(listenPipeWriteHndl);
						}
                        else
                        {
                            //error handle
                            printf("ret is failure\n");
                        }
					}
                }
                else
                {
                    ret = apisPipeHandle( c );
                    if ( ret == APIS_LNX_SUCCESS )
                    {
                        // Everything is ok
                    }
                    else
                    {
                        uint8 childThread;
                        switch ( apisErrorCode )
                        {
                            case APIS_ERROR_IPC_RECV_DATA_DISCONNECT:
                                uiPrintfEx(trINFO,  "Removing pipe #%d\n", c );

                                // Pipe closed. Remove from set
                                FD_CLR( c, &activePipesFDs );

                                // We should now set ret to APIS_LNX_SUCCESS, but there is still one fatal error
                                // possibility so simply set ret = to return value from removeFromActiveList().
                                dropActivePipe( c );

                                sprintf( toAPISLnxLog, "\t\tRemoved pipe #%d", c );
                                writetoAPISLnxLog( toAPISLnxLog );
                                break;

                            default:
                                if ( apisErrorCode == APIS_LNX_SUCCESS )
                                {
                                    // Do not report and abort if there is no real error.
                                    ret = APIS_LNX_SUCCESS;
                                }
                                else
                                {
                                    //							debug_
                                    uiPrintfEx(trINFO,  "[ERR] apisErrorCode 0x%.8X\n", apisErrorCode );

                                    // Everything about the error can be found in the message, and in npi_ipc_errno:
                                    childThread = apisHdrBuf.cmdId;

                                    sprintf( toAPISLnxLog, "Child thread with ID %d"
                                            " in module %d reported error",
                                            NPI_LNX_ERROR_THREAD( childThread ),
                                            NPI_LNX_ERROR_MODULE( childThread ));
                                    writetoAPISLnxLog( toAPISLnxLog );
                                }
                                break;
                        }
                    }
                }
			}
		}

		// Collect garbages
		// Note that for thread-safety garbage connections are collected
		// only from this single thread
		aclGarbageCollect();

	} while ( !done );

	return (ptr);
}

/*********************************************************************
 * @fn			startupInfo
 *
 * @brief	 Print/Display the pipe information
 *
 * @param	 none
 *
 * @return	none
 *
 *********************************************************************/
static void startupInfo( void )
{

}

/*********************************************************************
 * @fn			apiServerExit
 *
 * @brief	 Exit the API Server
 *
 * @param	 exitCode - reason for exit
 *
 * @return	none
 *
 *********************************************************************/
 /*
static void apiServerExit( int exitCode )
{
	// doesn't work yet, TBD
	//exit ( apisErrorCode );
}
*/
/*********************************************************************
 * @fn			writetoAPISLnxLog
 *
 * @brief	 Write a log file entry
 *
 * @param	 str - String to write
 *
 * @return	none
 *
 *********************************************************************/
static void writetoAPISLnxLog( const char* str )
{
	int APISLnxLogFd, i = 0;
	char *fullStr;
	char *inStr;

	time_t timeNow;
	struct tm * timeNowinfo;

	if ( logPath )
	{
		fullStr = (char *) malloc( 255 );
		inStr = (char *) malloc( 255 );

		time( &timeNow );
		timeNowinfo = localtime( &timeNow );

		sprintf( fullStr, "[%s", asctime( timeNowinfo ) );
		sprintf( inStr, "%s", str );

		// Remove \n characters
		fullStr[strlen( fullStr ) - 2] = 0;
		for ( i = strlen( str ) - 1; i > MAX( strlen( str ), 4 ); i-- )
		{
			if ( inStr[i] == '\n' )
			{
				inStr[i] = 0;
			}
		}
		sprintf( fullStr, "%s] %s", fullStr, inStr );

		// Add global error code
		sprintf( fullStr, "%s. Error: %.8X\n", fullStr, apisErrorCode );

		// Write error message to /dev/SSLnxLog
		APISLnxLogFd = open( logPath, (O_WRONLY | O_APPEND | O_CREAT), S_IRWXU );
		if ( APISLnxLogFd > 0 )
		{
			write( APISLnxLogFd, fullStr, strlen( fullStr ) + 1 );
		}
		else
		{
			uiPrintfEx(trINFO,  "Could not write \n%s\n to %s. Error: %.8X\n", str, logPath,
					errno );
			perror( "open" );
		}

		close( APISLnxLogFd );
		free( fullStr );
		free( inStr );
	}
}

static int searchWritePipeFromReadPipe( int readPipe)
{
    Pipe_t *entry;
    entry = activePipeList;
    while ( entry )
    {
        if ( entry->serverReadPipe == readPipe )
        {
			return entry->serverWritePipe;
        }
        entry = entry->next;
    }
    return -1;	
}

/*********************************************************************
 * @fn			addToActiveList
 *
 * @brief	Add a pipe to the active list
 *
 * @param	 c - pipe handle
 *
 * @return	APIS_LNX_SUCCESS or APIS_LNX_FAILURE
 *
 *********************************************************************/
static int addToActiveList( int readPipe, int writePipe )
{
	printf("addToActiveList read && write %d,%d.\n", readPipe, writePipe);
	if ( pthread_mutex_lock( &aclMutex ) != 0 )
	{
		perror( "pthread_mutex_lock" );
		exit( 1 );
	}
	if ( activePipeCount <= APIS_PIPE_QUEUE_SIZE )
	{
		// Entry at position activePipes.size is always the last available entry
		Pipe_t *newinfo = malloc( sizeof(Pipe_t) );
		newinfo->next = activePipeList;
		newinfo->garbage = FALSE;
		pthread_mutex_init( &newinfo->mutex, NULL );
		newinfo->serverReadPipe = readPipe;
		newinfo->serverWritePipe = writePipe;
		activePipeList = newinfo;

		// Increment size
		activePipeCount++;

		pthread_mutex_unlock( &aclMutex );

		return APIS_LNX_SUCCESS;
	}
	else
	{
		pthread_mutex_unlock( &aclMutex );

		// There's no more room in the list
		apisErrorCode = APIS_ERROR_IPC_ADD_TO_ACTIVE_LIST_NO_ROOM;

		return (APIS_LNX_FAILURE);
	}
}

/*********************************************************************
 * @fn			apisPipeHandle
 *
 * @brief	 Handle receiving a message
 *
 * @param	 c - pipe handle to remove
 *
 * @return	APIS_LNX_SUCCESS or APIS_LNX_FAILURE
 *
 *********************************************************************/
static int apisPipeHandle( int readPipe )
{
	int n;
	int ret = APIS_LNX_SUCCESS;

	// Handle the pipe
	uiPrintfEx(trINFO, "Receive message...\n" );

	// Receive only NPI header first. Then then number of bytes indicated by length.
	n = read( readPipe, &apisHdrBuf, sizeof(apisHdrBuf) );
	if ( n <= 0 )
	{
		if ( n < 0 )
		{
			perror( "read" );
            apisErrorCode = APIS_ERROR_IPC_RECV_DATA_CHECK_ERRNO;
            ret = APIS_LNX_FAILURE;
		}
		else
		{
			uiPrintfEx(trINFO, "Will disconnect #%d\n", readPipe );
			apisErrorCode = APIS_ERROR_IPC_RECV_DATA_DISCONNECT;
			ret = APIS_LNX_FAILURE;
		}
	}
	else if ( n == sizeof(apisHdrBuf) )
	{
		uint8 *buf = NULL;
		uint16 len = apisHdrBuf.lenL;
		len |= (uint16) apisHdrBuf.lenH << 8;

		// Now read out the payload of the NPI message, if it exists
		if ( len > 0 )
		{
			buf = malloc( len );

			if ( !buf )
			{
				uiPrintfEx(trINFO,  "[ERR] APIS receive thread memory allocation failure" );
				apisErrorCode = APIS_ERROR_MALLOC_FAILURE;
				return APIS_LNX_FAILURE;
			}

			n = read( readPipe, buf, len );
			if ( n != len )
			{
				uiPrintfEx(trINFO,  "[ERR] Could not read out the NPI payload."
						" Requested %d, but read %d!\n", len, n );

				apisErrorCode = APIS_ERROR_IPC_RECV_DATA_TOO_FEW_BYTES;
				ret = APIS_LNX_FAILURE;

				if ( n < 0 )
				{
					perror( "read" );
					// Disconnect this
					apisErrorCode = APIS_ERROR_IPC_RECV_DATA_DISCONNECT;
					ret = APIS_LNX_FAILURE;
				}
			}
			else
			{
				ret = APIS_LNX_SUCCESS;
			}
		}

		/*
		 * Take the message from the client and pass it to be processed
		 */
		if ( ret == APIS_LNX_SUCCESS )
		{
			if ( len != 0xFFFF )
			{
				if ( apisMsgCB )
				{
					apisMsgCB( searchWritePipeFromReadPipe(readPipe), apisHdrBuf.subSys, apisHdrBuf.cmdId, len, buf,
							SERVER_DATA );
				}
			}
			else
			{
				// len == 0xFFFF is used for pipe removal
				// hence is not supported.
				uiPrintfEx(trINFO,  "[WARN] APIS received message"
						" with size 0xFFFFu was discarded silently\n" );
			}
		}

		if ( buf )
		{
			free( buf );
		}
	}

	if ( (ret == APIS_LNX_FAILURE)
			&& (apisErrorCode == APIS_ERROR_IPC_RECV_DATA_DISCONNECT) )
	{
		uiPrintfEx(trINFO, "Done with %d\n", readPipe );
	}
	else
	{
		uiPrintfEx(trINFO, "!Done\n" );
	}

	return ret;
}

/*********************************************************************
 *********************************************************************/
