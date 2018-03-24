/**************************************************************************************************
 Filename:       npi_lnx_ipc.c
 Revised:        $Date: 2012-03-21 17:37:33 -0700 (Wed, 21 Mar 2012) $
 Revision:       $Revision: 246 $

 Description:    This file contains Linux platform specific NPI socket server
 implementation

 Copyright (C) {2012} Texas Instruments Incorporated - http://www.ti.com/

 Beej's Guide to Unix IPC was used in the development of this software:
 http://beej.us/guide/bgipc/output/html/multipage/intro.html#audience
 A small portion of the code from the associated code was also used. This
 code is Public Domain.


 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the
 distribution.

 Neither the name of Texas Instruments Incorporated nor the names of
 its contributors may be used to endorse or promote products derived
 from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************************************/

/**************************************************************************************************
 *                                           Includes
 **************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// For stress testing data dump
#include <fcntl.h>
#include <time.h>

#include <sys/time.h>

#include "config.h"

/* NPI includes */
#include "common/npi_lnx.h"
#include "common/npi_lnx_error.h"
#include "common/npi_lnx_ipc_rpc.h"

#include "npi_lnx_ipc.h"
#include "trace.h"

#undef SERVER_NAME
#define SERVER_NAME	NPI_OS_IPC_SRV

#if (defined NPI_SPI) && (NPI_SPI == TRUE)
#include "npi_lnx_spi.h"
#include "hal_spi.h"
#endif

#if (defined NPI_I2C) && (NPI_I2C == TRUE)
#include "npi_lnx_i2c.h"
#include "hal_i2c.h"
#endif

#if (defined NPI_UART) && (NPI_UART == TRUE)
#include "npi_lnx_uart.h"
#endif

// The following is only necessary because we always read out GPIO configuration
#include "hal_gpio.h"

#if (!defined NPI_SPI) || (NPI_SPI == FALSE)
#if (!defined NPI_I2C) || (NPI_I2C == FALSE)
#if (!defined NPI_UART) || (NPI_UART == FALSE)
#error "neither NPI_I2C, NPI_SPI, NPI_UART defined to TRUE, at least one mandatory. verify your makefile"
#endif
#endif
#endif

#ifdef __BIG_DEBUG__
#define debug_printf(fmt, ...) 	printf( fmt, ##__VA_ARGS__)
#else
#define debug_printf(fmt, ...) 	st (if (__BIG_DEBUG_ACTIVE == TRUE) printf( fmt, ##__VA_ARGS__);)
#endif

#define time_printf(fmt, ...) 	st (if (__DEBUG_TIME_ACTIVE == TRUE) printf( fmt, ##__VA_ARGS__);)

/**************************************************************************************************
 *                                        Externals
 **************************************************************************************************/

/**************************************************************************************************
 *                                        Defines
 **************************************************************************************************/
#define NPI_SERVER_PIPE_QUEUE_SIZE        		4

#define MAX(a,b)								((a > b) ? a : b)

/**************************************************************************************************
 *                                           Constant
 **************************************************************************************************/

const char* sectionNamesArray[3][2] =
{
	{
		"GPIO_SRDY.GPIO",
		"GPIO_SRDY.LEVEL_SHIFTER"
	},
	{
		"GPIO_MRDY.GPIO",
		"GPIO_MRDY.LEVEL_SHIFTER"
	},
	{
		"GPIO_RESET.GPIO",
		"GPIO_RESET.LEVEL_SHIFTER"
	},
};

enum
{
	NPI_UART_FN_ARR_IDX,  	//0
	NPI_SPI_FN_ARR_IDX,		//1
	NPI_I2C_FN_ARR_IDX,		//2
};

const pNPI_OpenDeviceFn NPI_OpenDeviceFnArr[] =
{
#if (defined NPI_UART) && (NPI_UART == TRUE)
    NPI_UART_OpenDevice,
#else
    NULL,
#endif
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_OpenDevice,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NPI_I2C_OpenDevice
#else
    NULL,
#endif
};

const pNPI_CloseDeviceFn NPI_CloseDeviceFnArr[] =
{
#if (defined NPI_UART) && (NPI_UART == TRUE)
    NPI_UART_CloseDevice,
#else
    NULL,
#endif
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_CloseDevice,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NPI_I2C_CloseDevice
#else
    NULL,
#endif
};
const pNPI_SendAsynchDataFn NPI_SendAsynchDataFnArr[] =
{
#if (defined NPI_UART) && (NPI_UART == TRUE)
    NPI_UART_SendAsynchData,
#else
    NULL,
#endif
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_SendAsynchData,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NPI_I2C_SendAsynchData
#else
    NULL,
#endif
};
const pNPI_SendSynchDataFn NPI_SendSynchDataFnArr[] =
{
#if (defined NPI_UART) && (NPI_UART == TRUE)
    NPI_UART_SendSynchData,
#else
    NULL,
#endif
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_SendSynchData,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NPI_I2C_SendSynchData
#else
    NULL,
#endif
};

const pNPI_ResetSlaveFn NPI_ResetSlaveFnArr[] =
{
    NULL,
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_ResetSlave,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NPI_I2C_ResetSlave,
#else
    NULL,
#endif
};

const pNPI_SynchSlaveFn NPI_SynchSlaveFnArr[] =
{
    NULL,
#if (defined NPI_SPI) && (NPI_SPI == TRUE)
    NPI_SPI_SynchSlave,
#else
    NULL,
#endif
#if (defined NPI_I2C) && (NPI_I2C == TRUE)
    NULL,
#else
    NULL,
#endif
};

/**************************************************************************************************
 *                                        Type definitions
 **************************************************************************************************/

/**************************************************************************************************
 *                                        Global Variables
 **************************************************************************************************/

int npi_ipc_errno;
int __BIG_DEBUG_ACTIVE = FALSE;
int __DEBUG_TIME_ACTIVE = FALSE;		// Do not enable by default.

/**************************************************************************************************
 *                                        Local Variables
 **************************************************************************************************/

// Pipe handles
int listenPipeReadHndl;
int listenPipeWriteHndl;

int clientsNum = 0;

// Socket connection file descriptors
fd_set activePipesFDs;
int fdmax;
struct
{
	int list[NPI_SERVER_PIPE_QUEUE_SIZE][2];
	int size;
} activePipes;

// NPI IPC Server buffers
char npi_ipc_buf[2][sizeof(npiMsgData_t)];

// Variables for Configuration
RTT_FILE *serialCfgFd;
char* pStrBufRoot;
char* devPath;
char* logPath;
halGpioCfg_t** gpioCfg;

#if 0
char* npi_rtt_ipc_argvs[2] =
{
	"npi_rtt_ipc_main", "/NPI_Gateway.cfg"
};
#else
char* npi_rtt_ipc_argvs[3] =
{
	"2",
	"npi_rtt_ipc_main",
	"/NPI_Gateway.cfg"
};
#endif

/**************************************************************************************************
 *                                     Local Function Prototypes
 **************************************************************************************************/

// NPI Callback Related
int NPI_AsynchMsgCback(npiMsgData_t *pMsg);

int SerialConfigParser(RTT_FILE* serialCfgFd, const char* section,
	const char* key, char* resString);

void NPI_LNX_IPC_Exit(int ret);

static uint8 devIdx = 0;

int NPI_LNX_IPC_SendData(uint8 len, int writePipe);
int NPI_LNX_IPC_ConnectionHandle(int readPipe);

int searchWritePipeFromActiveList(int readPipe);
int removeFromActiveList(int pipe);
int addToActiveList (int readPipe, int writePipe);
static int npi_ServerCmdHandle(npiMsgData_t *npi_ipc_buf);

/**************************************************************************************************
 * @fn          halDelay
 *
 * @brief       Delay for milliseconds.
 *              Do not invoke with zero.
 *              Do not invoke with greater than 500 msecs.
 *              Invoking with very high frequency and/or with long delays will start to
 *              significantly impact the real time performance of TimerA tasks because this will
 *              invisibly overrun the period when the TimerA count remaining, when this function
 *              is invoked, is less than the delay requested.
 *
 * input parameters
 *
 * @param       msecs - Milliseconds to delay in low power mode.
 * @param       sleep - Enforces blocking delay in low power mode if set.
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 **************************************************************************************************
 */
void halDelay(uint8 msecs, uint8 sleep)
{
	if (sleep)
	{
		//    usleep(msecs * 1000);
	}
}

/**************************************************************************************************
 *
 * @fn          NPI Linux IPC Socket Server
 *
 * @brief       This is the main function
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 *
 **************************************************************************************************/
//int npi_rtt_ipc_main(int argc, char ** argv)
void npi_rtt_ipc_main(void* args)
{
	int argc;
	char** tmpArgv = (char**)args;
	char** argv= tmpArgv+1;
	argc = atoi(tmpArgv[0]);
	int c;
	int n;
	int ret = NPI_LNX_SUCCESS;
	char listen_buf[SERVER_LISTEN_BUF_SIZE];
	char tmpReadPipeName[TMP_PIPE_NAME_SIZE];
	char tmpWritePipeName[TMP_PIPE_NAME_SIZE];
	int tmpReadPipe;
	int tmpWritePipe;
    char assignedId[NPI_IPC_ASSIGNED_ID_BUF_LEN];
	fd_set activePipesFDsSafeCopy;

    char npi_ipc_read_fifo_path[FIFO_PATH_BUFFER_LEN];
    char npi_ipc_write_fifo_path[FIFO_PATH_BUFFER_LEN];
	/**********************************************************************
	 * First step is to Configure the serial interface
	 **********************************************************************/

	// Variables for Configuration. Some are declared global to be used in unified graceful
	// exit function.
	char* strBuf;
	uint8 gpioIdx = 0;
	char* configFilePath;

	if (argc==1)
	{
		configFilePath = "/NPI_Gateway.cfg";
	}
	else if (argc==2)
	{
		configFilePath = argv[1];
	}
	else if (argc==3)
	{
		configFilePath = argv[1];
	}
	else
	{
		uiPrintfEx(trINFO, "Too many arguments\n");
	}

	// Allocate memory for string buffer and configuration buffer
	strBuf = (char*) malloc(128);
	memset(strBuf, 0, 128);
	pStrBufRoot = strBuf;
	devPath = (char*) malloc(128);
	logPath = (char*) malloc(128);
	memset(devPath, 0, 128);
	memset(logPath, 0, 128);
	gpioCfg = (halGpioCfg_t**) malloc(3 * sizeof(halGpioCfg_t*));
	uiPrintfEx(trINFO, "gpioCfg \t\t\t\t%p\n",
			(void *)&(gpioCfg));
	for (gpioIdx = 0; gpioIdx < 3; gpioIdx++)
	{
		gpioCfg[gpioIdx] = (halGpioCfg_t*) malloc(sizeof(halGpioCfg_t));
		memset(gpioCfg[gpioIdx], 0, sizeof(halGpioCfg_t));
		uiPrintfEx(trINFO, "gpioCfg[%d] \t\t\t\t%p\n",
				gpioIdx, (void *)&(gpioCfg[gpioIdx]));
		uiPrintfEx(trINFO, "gpioCfg[%d].gpio \t\t\t%p\n",
				gpioIdx, (void *)&(gpioCfg[gpioIdx]->gpio));
		uiPrintfEx(trINFO, "gpioCfg[%d].levelshifter \t\t%p\n",
				gpioIdx, (void *)&(gpioCfg[gpioIdx]->levelshifter));
	}

	// Open file for parsing
	serialCfgFd = rtt_fopen(configFilePath, "r");
	if (serialCfgFd == NULL)
	{
		//                            debug_
		uiPrintfEx(trINFO, "Could not open file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_OPEN_REMOTI_RNP_CFG;
		NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
	}

	// Get device type
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "DEVICE", "deviceKey", strBuf)))
	{
		uiPrintfEx(trINFO, "Could not find 'deviceKey' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_KEY;
		NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
	}

	// Copy from buffer to variable
	devIdx = strBuf[0] - '0';
	//            debug_
	uiPrintfEx(trINFO, "deviceKey = %i  (%s)\n", devIdx, strBuf);

	// Get path to the device
	strBuf = pStrBufRoot;
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "DEVICE", "devPath", strBuf)))
	{
		uiPrintfEx(trINFO, "Could not find 'devPath' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_PATH;
		NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
	}
	// Copy from buffer to variable
	memcpy(devPath, strBuf, strlen(strBuf));
	uiPrintfEx(trINFO, "devPath = '%s'\n", devPath);

	// Get path to the log file
	strBuf = pStrBufRoot;
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "LOG", "log", strBuf)))
	{
		uiPrintfEx(trINFO, "Could not find 'log' inside config file '%s'\n", configFilePath);
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_LOG_PATH;
		NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
	}
	// Copy from buffer to variable
	memcpy(logPath, strBuf, strlen(strBuf));
	uiPrintfEx(trINFO, "logPath = '%s'\n", logPath);

	// GPIO configuration
	if ((devIdx == 1) || (devIdx == 2))
	{
		for (gpioIdx = 0; gpioIdx < 3; gpioIdx++)
		{
			// Get SRDY, MRDY or RESET GPIO
			uiPrintfEx(trINFO, "gpioCfg[gpioIdx]->gpio \t\t\t%p\n",
					(void *)&(gpioCfg[gpioIdx]->gpio));

			// Get SRDY, MRDY or RESET GPIO value
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][0],
					"value", strBuf)))
			{
			// Copy from buffer to variable
				uiPrintfEx(trINFO, "strBuf \t\t\t\t\t%p\n",
						(void *)&strBuf);
				uiPrintfEx(trINFO, "gpioCfg[gpioIdx]->gpio.value \t\t%p\n",
						(void *)&(gpioCfg[gpioIdx]->gpio.value));
				memcpy(gpioCfg[gpioIdx]->gpio.value, strBuf, strlen(strBuf));
				uiPrintfEx(trINFO, "gpioCfg[%i]->gpio.value = '%s'\n",
						gpioIdx, gpioCfg[gpioIdx]->gpio.value);
			}
			else
			{
				uiPrintfEx(trINFO, "[CONFIG] ERROR , key 'value' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][0]);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, devIdx);
				NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
			}

			// Get SRDY, MRDY or RESET GPIO direction
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][0],
					"direction", strBuf)))
			{
			// Copy from buffer to variable
				uiPrintfEx(trINFO, "strBuf \t\t\t\t\t%p\n",
						(void *)&strBuf);
				uiPrintfEx(trINFO, "gpioCfg[gpioIdx]->gpio.direction \t%p\n",
						(void *)&(gpioCfg[gpioIdx]->gpio.direction));
				memcpy(gpioCfg[gpioIdx]->gpio.direction, strBuf,
						strlen(strBuf));
				uiPrintfEx(trINFO, "gpioCfg[%i]->gpio.direction = '%s'\n",
						gpioIdx, gpioCfg[gpioIdx]->gpio.direction);
			}
			else
			{
				uiPrintfEx(trINFO, "[CONFIG] ERROR , key 'direction' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][0]);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, devIdx);
				NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
			}

#ifdef SRDY_INTERRUPT
			// Get SRDY, MRDY or RESET GPIO edge
			if (gpioIdx == 0)
			{
				strBuf = pStrBufRoot;
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, sectionNamesArray[gpioIdx][0],
						"edge", strBuf)))
				{
					// Copy from buffer to variable
					uiPrintfEx(trINFO, "strBuf \t\t\t\t\t%p\n",
							(void *)&strBuf);
					uiPrintfEx(trINFO, "gpioCfg[gpioIdx]->gpio.edge \t%p\n",
							(void *)&(gpioCfg[gpioIdx]->gpio.edge));
					memcpy(gpioCfg[gpioIdx]->gpio.edge, strBuf, strlen(strBuf));
					uiPrintfEx(trINFO, "gpioCfg[%i]->gpio.edge = '%s'\n",
							gpioIdx, gpioCfg[gpioIdx]->gpio.edge);
				}
				else
				{
					uiPrintfEx(trINFO, "[CONFIG] ERROR , key 'edge' is missing for mandatory GPIO %s\n", sectionNamesArray[gpioIdx][0]);
					npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOTI_RNP_CFG_PARSER_DEVICE_GPIO(gpioIdx, 0, devIdx);
					NPI_LNX_IPC_Exit(NPI_LNX_FAILURE);
				}
			}
#endif
			// Get SRDY, MRDY or RESET GPIO Active High/Low
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][1], "active_high_low",
					strBuf)))
			{
				// Copy from buffer to variable
				gpioCfg[gpioIdx]->gpio.active_high_low = strBuf[0] - '0';
				uiPrintfEx(trINFO, "gpioCfg[%i]->gpio.active_high_low = %d\n",
							gpioIdx, gpioCfg[gpioIdx]->gpio.active_high_low);
			}
			else
				uiPrintfEx(trINFO, "[CONFIG] Warning , key 'active_high_low' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][0]);

			// Get SRDY, MRDY or RESET Level Shifter
			uiPrintfEx(trINFO, "gpioCfg[gpioIdx]->levelshifter \t\t\t%p\n",
					(void *)&(gpioCfg[gpioIdx]->levelshifter));

			// Get SRDY, MRDY or RESET Level Shifter value
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][1], "value", strBuf)))
			{
				// Copy from buffer to variable
				memcpy(gpioCfg[gpioIdx]->levelshifter.value, strBuf,
						strlen(strBuf));
				uiPrintfEx(trINFO, "gpioCfg[%i]->levelshifter.value = '%s'\n",
							gpioIdx, gpioCfg[gpioIdx]->levelshifter.value);
			}
			else
				uiPrintfEx(trINFO, "[CONFIG] Warning , key 'value' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][1]);

			// Get SRDY, MRDY or RESET Level Shifter direction
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][1], "direction", strBuf)))
			{
				// Copy from buffer to variable
				memcpy(gpioCfg[gpioIdx]->levelshifter.direction, strBuf,
						strlen(strBuf));
				uiPrintfEx(trINFO, "gpioCfg[%i]->levelshifter.direction = '%s'\n",
							gpioIdx, gpioCfg[gpioIdx]->levelshifter.direction);
			}
			else
				uiPrintfEx(trINFO, "[CONFIG] Warning , key 'direction' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][1]);


			// Get SRDY, MRDY or RESET Level Shifter Active High/Low
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd,
					sectionNamesArray[gpioIdx][1], "active_high_low", strBuf)))
			{
				// Copy from buffer to variable
				gpioCfg[gpioIdx]->levelshifter.active_high_low = atoi(strBuf);
				uiPrintfEx(trINFO, "gpioCfg[%i]->levelshifter.active_high_low = %d\n",
						gpioIdx, gpioCfg[gpioIdx]->levelshifter.active_high_low);
			}
			else
				uiPrintfEx(trINFO, "[CONFIG] Warning , key 'active_high_low' is missing for optional GPIO %s\n", sectionNamesArray[gpioIdx][1]);
		}
	}


	/**********************************************************************
	 * Now open the serial interface
	 */
	switch(devIdx)
	{
		case NPI_UART_FN_ARR_IDX:
		#if (defined NPI_UART) && (NPI_UART == TRUE)
		{
			npiUartCfg_t uartCfg;
			strBuf = pStrBufRoot;
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "speed", strBuf)))
			{
				uartCfg.speed = atoi(strBuf);
			}
			else
			{
				uartCfg.speed=115200;
			}
			if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "flowcontrol", strBuf)))
			{
				uartCfg.flowcontrol = atoi(strBuf);
			}
			else
			{
				uartCfg.flowcontrol=0;
			}
			ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiUartCfg_t *)&uartCfg);
		}
		#endif
		break;
		case NPI_SPI_FN_ARR_IDX:
			#if (defined NPI_SPI) && (NPI_SPI == TRUE)
			{
				halSpiCfg_t halSpiCfg;
				npiSpiCfg_t npiSpiCfg;
				strBuf = pStrBufRoot;
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "speed", strBuf)))
				{
					halSpiCfg.speed = strtol(strBuf, NULL, 10);
				}
				else
				{
					halSpiCfg.speed = 500000;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "mode", strBuf)))
				{
					halSpiCfg.mode = strtol(strBuf, NULL, 16);
				}
				else
				{
					halSpiCfg.mode = 0;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "bitsPerWord", strBuf)))
				{
					halSpiCfg.bitsPerWord = strtol(strBuf, NULL, 10);
				}
				else
				{
					halSpiCfg.bitsPerWord = 0;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "useFullDuplexAPI", strBuf)))
				{
					halSpiCfg.useFullDuplexAPI = strtol(strBuf, NULL, 10);
				}
				else
				{
					halSpiCfg.useFullDuplexAPI = TRUE;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "earlyMrdyDeAssert", strBuf)))
				{
					npiSpiCfg.earlyMrdyDeAssert = strtol(strBuf, NULL, 10);
				}
				else
				{
					// If it is not defined then set value for RNP
					npiSpiCfg.earlyMrdyDeAssert = TRUE;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "detectResetFromSlowSrdyAssert", strBuf)))
				{
					npiSpiCfg.detectResetFromSlowSrdyAssert = strtol(strBuf, NULL, 10);
				}
				else
				{
					// If it is not defined then set value for RNP
					npiSpiCfg.detectResetFromSlowSrdyAssert = TRUE;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "forceRunOnReset", strBuf)))
				{
					npiSpiCfg.forceRunOnReset = strtol(strBuf, NULL, 16);
				}
				else
				{
					// If it is not defined then set value for RNP
					npiSpiCfg.forceRunOnReset = NPI_LNX_UINT8_ERROR;
				}
				if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "srdyMrdyHandshakeSupport", strBuf)))
				{
					npiSpiCfg.srdyMrdyHandshakeSupport = strtol(strBuf, NULL, 10);
				}
				else
				{
					// If it is not defined then set value for RNP
					npiSpiCfg.srdyMrdyHandshakeSupport = TRUE;
				}

				npiSpiCfg.spiCfg = &halSpiCfg;
				npiSpiCfg.gpioCfg = gpioCfg;

				// Now open device for processing
				ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiSpiCfg_t *) &npiSpiCfg);

				// Perform Reset of the RNP
				(NPI_ResetSlaveFnArr[devIdx])();

				// Do the Hw Handshake
				(NPI_SynchSlaveFnArr[devIdx])();
			}
			#endif
			break;

		case NPI_I2C_FN_ARR_IDX:
			#if (defined NPI_I2C) && (NPI_I2C == TRUE)
				{
					npiI2cCfg_t i2cCfg;
					i2cCfg.gpioCfg = gpioCfg;

					// Open the Device and perform a reset
					ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiI2cCfg_t *) &i2cCfg);
				}
			#endif
			break;
		default:
			ret = NPI_LNX_FAILURE;
			break;
	}

	// Get port from configuration file
	if (NPI_LNX_FAILURE == (SerialConfigParser(serialCfgFd, "PORT", "port", strBuf)))
	{
		// Fall back to default if port was not found in the configuration file
		//strncpy(port, NPI_PORT, 128);
		//printf("Warning! Port not found in configuration file. Will use default port: %s\n",port);
	} 
	else 
	{
		//strncpy(port, strBuf, 128);
	}


	// The following will exit if ret != SUCCESS
	NPI_LNX_IPC_Exit(ret);

	/**********************************************************************
	 * Now that everything has been initialized and configured, let's open
	 * a socket and begin listening.
	 **********************************************************************/

	//mkfifo and open pipes
	if ((mkfifo (NPI_IPC_LISTEN_PIPE_CLIENT2SERVER, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		uiPrintfEx(trINFO, "cannot create fifo %s\n", NPI_IPC_LISTEN_PIPE_CLIENT2SERVER);
	}
	if ((mkfifo (NPI_IPC_LISTEN_PIPE_SERVER2CLIENT, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
	{
		uiPrintfEx(trINFO, "cannot create fifo %s\n", NPI_IPC_LISTEN_PIPE_SERVER2CLIENT);
	}

	memset(npi_ipc_read_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
	sprintf(npi_ipc_read_fifo_path, "%s%s", FIFO_PATH_PREFIX, NPI_IPC_LISTEN_PIPE_CLIENT2SERVER);

	listenPipeReadHndl = open(npi_ipc_read_fifo_path, ZB_LISTEN_PIPE_OPEN_FLAG, 0);
	if (listenPipeReadHndl == -1)
	{
		uiPrintfEx(trINFO, "open %s for read error\n", npi_ipc_read_fifo_path);
		exit (-1);
	}
	else
	{
		uiPrintfEx(trINFO, "open npi_ipc_listen_read_fifo ok with fd = %d.\n", listenPipeReadHndl);
	}

	// Connection main loop. Cannot get here with ret != SUCCESS
	// Clear file descriptor sets
	FD_ZERO(&activePipesFDs);
	FD_ZERO(&activePipesFDsSafeCopy);

	// Add the listener to the set
	FD_SET (listenPipeReadHndl, &activePipesFDs);


	fdmax = listenPipeReadHndl;

	uiPrintfEx(trINFO, "waiting for first connection on #%d...\n", listenPipeReadHndl);

	while (ret == NPI_LNX_SUCCESS)
	{
		activePipesFDsSafeCopy = activePipesFDs;

		// First use select to find activity on the sockets
		if (select (fdmax + 1, &activePipesFDsSafeCopy, NULL, NULL, NULL) == -1)
		{
			if (errno != EINTR)
			{
				perror("select");
				npi_ipc_errno = NPI_LNX_ERROR_IPC_PIPE_SELECT_CHECK_ERRNO;
				ret = NPI_LNX_FAILURE;
				break;
			}
			continue;
		}

		// Then process this activity
		for (c = 0; c <= fdmax; c++)
		{
			if (FD_ISSET(c, &activePipesFDsSafeCopy))
			{
				if (c == listenPipeReadHndl)
				{
                    n = read (listenPipeReadHndl, listen_buf, SERVER_LISTEN_BUF_SIZE);
                    if (n <= 0)
                    {
                    	uiPrintfEx(trINFO, "read failed for n <= 0\n");
                        if (n < 0)
                        {
                            perror ("read");
                            uiPrintfEx(trINFO, "NPI_LNX_ERROR_IPC_RECV_DATA_CHECK_ERRNO\n");
                            npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_CHECK_ERRNO;
                            ret = NPI_LNX_FAILURE;
                        }
                        else
                        {
                        	uiPrintfEx(trINFO, "NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT\n");
                            npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT;
                            //ret = NPI_LNX_FAILURE;
							//move out this fd from fd_set and reopen the pipe for listening and add to fd_set
							//pause();
							close(listenPipeReadHndl);
                            FD_CLR(listenPipeReadHndl, &activePipesFDs);		
    						listenPipeReadHndl = open (npi_ipc_read_fifo_path, ZB_LISTEN_PIPE_OPEN_FLAG, 0);
    						if (listenPipeReadHndl == -1)
    						{
    							uiPrintfEx(trINFO, "open %s for read error\n", npi_ipc_read_fifo_path);
       		 					exit (-1);
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
						uiPrintfEx(trINFO, "read %d bytes from listenPipeReadHndl of string is %s.\n",n,listen_buf);
                    	if (!strncmp(listen_buf, NPI_IPC_LISTEN_PIPE_CHECK_STRING, strlen(NPI_IPC_LISTEN_PIPE_CHECK_STRING)))
                    	{
                    		//npi_ipc_write_fifo_path
                    		memset(npi_ipc_write_fifo_path, '\0', FIFO_PATH_BUFFER_LEN);
                    		sprintf(npi_ipc_write_fifo_path, "%s%s", FIFO_PATH_PREFIX, NPI_IPC_LISTEN_PIPE_SERVER2CLIENT);
                        	listenPipeWriteHndl = open (npi_ipc_write_fifo_path, O_WRONLY, 0);
                        	if (listenPipeWriteHndl == -1)
                        	{
                            	if (errno == ENXIO)
                            	{
                            		uiPrintfEx(trINFO, "open error; no reading process\n");
                                	ret = NPI_LNX_FAILURE;
                                	break;
                            	}
                        	}

                        	sprintf(assignedId,"%d",clientsNum);
                        	memset (tmpReadPipeName, '\0', TMP_PIPE_NAME_SIZE);
                        	memset (tmpWritePipeName, '\0', TMP_PIPE_NAME_SIZE);
                        	sprintf(tmpReadPipeName, "%s%d", NPI_IPC_LISTEN_PIPE_CLIENT2SERVER, clientsNum);
                        	sprintf(tmpWritePipeName, "%s%d", NPI_IPC_LISTEN_PIPE_SERVER2CLIENT, clientsNum);

                        	if ((mkfifo (tmpReadPipeName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
                        	{
                        		uiPrintfEx(trINFO, "cannot create fifo %s\n", tmpReadPipeName);
                        	}
                        	if ((mkfifo (tmpWritePipeName, O_CREAT | O_EXCL) < 0) && (errno != EEXIST))
                        	{
                        		uiPrintfEx(trINFO, "cannot create fifo %s\n", tmpWritePipeName);
                        	}

                        	memset (tmpReadPipeName, '\0', TMP_PIPE_NAME_SIZE);
                        	memset (tmpWritePipeName, '\0', TMP_PIPE_NAME_SIZE);
                        	sprintf(tmpReadPipeName, "%s%s%d", FIFO_PATH_PREFIX, NPI_IPC_LISTEN_PIPE_CLIENT2SERVER, clientsNum);
                        	sprintf(tmpWritePipeName, "%s%s%d", FIFO_PATH_PREFIX, NPI_IPC_LISTEN_PIPE_SERVER2CLIENT, clientsNum);

                        	tmpReadPipe = open (tmpReadPipeName, ZB_READ_PIPE_OPEN_FLAG, 0);
                        	if (tmpReadPipe == -1)
                        	{
                        		uiPrintfEx(trINFO, "open %s for read error\n", tmpReadPipeName);
                            	ret = NPI_LNX_FAILURE;
                            	break;
                        	}

                        	tmpWritePipe = open (tmpWritePipeName, O_WRONLY, 0);
                        	if (tmpWritePipe == -1)
                        	{
                        		uiPrintfEx(trINFO, "open %s for write error\n", tmpWritePipeName);
                            	ret = NPI_LNX_FAILURE;
                            	break;
                       	 	}

                        	ret = addToActiveList (tmpReadPipe, tmpWritePipe);
                        	if (ret == NPI_LNX_FAILURE)
                       	 	{
                            	// Adding to the active connection list failed.
                            	// Close the accepted connection.
                            	close (tmpReadPipe);
                            	close (tmpWritePipe);
                            	//error handle
                        	}
                        	else
                       	 	{
                            	FD_SET (tmpReadPipe, &activePipesFDs);
                            	if (tmpReadPipe > fdmax)
                            	{
                                	fdmax = tmpReadPipe;
                            	}
                        	}
                        	clientsNum++;
							write(listenPipeWriteHndl, assignedId, strlen(assignedId));
                        	close (listenPipeWriteHndl);
                    	}
                    	else
                    	{
                    		uiPrintfEx(trINFO, "Other msg written to npi_lnx_ipc read pipe.\n");
                    	}
					}
				}
				else
				{
					ret = NPI_LNX_IPC_ConnectionHandle(c);
					if (ret == NPI_LNX_SUCCESS)
					{
						// Everything is ok
					}
					else
					{
						uint8 childThread;
						switch (npi_ipc_errno)
						{
                            case NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT:
                            	uiPrintfEx(trINFO, "Removing connection #%d\n", c);
                                // Connection closed. Remove from set
                                FD_CLR(c, &activePipesFDs);
                                // We should now set ret to NPI_SUCCESS, but there is still one fatal error
                                // possibility so simply set ret = to return value from removeFromActiveList().
                                ret = removeFromActiveList(c);
                                break;
                            case NPI_LNX_ERROR_UART_SEND_SYNCH_TIMEDOUT:
                                //This case can happen in some particular condition:
                                // if the network is in BOOT mode, it will not answer any synchronous request other than SYS_BOOT request.
                                // if we exit immediately, we will never be able to recover the NP device.
                                // This may be replace in the future by an update of the RNP behavior
                            	uiPrintfEx(trINFO, "Synchronous Request Timeout...");
                                ret = NPI_LNX_SUCCESS;
                                npi_ipc_errno = NPI_LNX_SUCCESS;
                                break;
                            default:
                                if (npi_ipc_errno == NPI_LNX_SUCCESS)
                                {
                                    // Do not report and abort if there is no real error.
                                    ret = NPI_LNX_SUCCESS;
                                }
                                else if (NPI_LNX_ERROR_JUST_WARNING(npi_ipc_errno))
                                {
                                    // This may be caused by an unexpected reset. Write it to the log,
                                    // but keep going.
                                    // Everything about the error can be found in the message, and in npi_ipc_errno:
                                    childThread = ((npiMsgData_t *) npi_ipc_buf[0])->cmdId;
                                    // Force continuation
                                    ret = NPI_LNX_SUCCESS;
                                }
                                else
                                {
                                	uiPrintfEx(trINFO, "[ERR] npi_ipc_errno 0x%.8X\n", npi_ipc_errno);
                                    // Everything about the error can be found in the message, and in npi_ipc_errno:
                                    childThread = ((npiMsgData_t *) npi_ipc_buf[0])->cmdId;
                                }
                                break;
						}

						// Check if error requested a reset
						if (NPI_LNX_ERROR_RESET_REQUESTED(npi_ipc_errno))
						{
							// Yes, utilize server control API to reset current device
							// Do it by reconnecting so that threads are kept synchronized
							npiMsgData_t npi_ipc_buf_tmp;
							int localRet = NPI_LNX_SUCCESS;
							uiPrintfEx(trINFO, "Reset was requested, so try to disconnect device %d\n", devIdx);
							npi_ipc_buf_tmp.cmdId = NPI_LNX_CMD_ID_DISCONNECT_DEVICE;
							localRet = npi_ServerCmdHandle((npiMsgData_t *)&npi_ipc_buf_tmp);
							uiPrintfEx(trINFO, "Disconnection from device %d was %s\n", devIdx, (localRet == NPI_LNX_SUCCESS) ? "successful" : "unsuccessful");
							if (localRet == NPI_LNX_SUCCESS)
							{
								uiPrintfEx(trINFO, "Then try to connect device %d again\n", devIdx);
								int bigDebugWas = __BIG_DEBUG_ACTIVE;
								if (bigDebugWas == FALSE)
								{
									__BIG_DEBUG_ACTIVE = TRUE;
									uiPrintfEx(trINFO, "__BIG_DEBUG_ACTIVE set to TRUE\n");
								}
								npi_ipc_buf_tmp.cmdId = NPI_LNX_CMD_ID_CONNECT_DEVICE;
								localRet = npi_ServerCmdHandle((npiMsgData_t *)&npi_ipc_buf_tmp);
								uiPrintfEx(trINFO, "Reconnection to device %d was %s\n", devIdx, (localRet == NPI_LNX_SUCCESS) ? "successful" : "unsuccessful");
								if (bigDebugWas == FALSE)
								{
									__BIG_DEBUG_ACTIVE = FALSE;
									uiPrintfEx(trINFO, "__BIG_DEBUG_ACTIVE set to FALSE\n");
								}
							}
						}

						// If this error was sent through socket; close this connection
						if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_CMD_TYPE_MASK) == RPC_CMD_NOTIFY_ERR)
						{
							uiPrintfEx(trINFO, "Removing connection #%d\n", c);
							// Connection closed. Remove from set
							FD_CLR(c, &activePipesFDs);
                            ret = removeFromActiveList(c);
						}
					}
				}
			}
		}
	}
	uiPrintfEx(trINFO, "Exit socket while loop\n");
	/**********************************************************************
	 * Remember to close down all connections
	 *********************************************************************/

	(NPI_CloseDeviceFnArr[devIdx])();

	// Free all remaining memory
	NPI_LNX_IPC_Exit(NPI_LNX_SUCCESS + 1);

	//return ret;
}

int searchWritePipeFromActiveList(int readPipe)
{
	int i;
	// Find entry
	for (i = 0; i < activePipes.size; i++)
	{
		if (activePipes.list[i][0] == readPipe)
        {
            return activePipes.list[i][1];
        }
	}
    return -1;    
}

/**************************************************************************************************
 *
 * @fn          addToActiveList
 *
 * @brief       Manage active connections, add to list
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      -1 if something went wrong, 0 if success
 *
 **************************************************************************************************/

int addToActiveList(int readPipe, int writePipe)
{
	if (activePipes.size <= NPI_SERVER_PIPE_QUEUE_SIZE)
	{
		// Entry at position activePipes.size is always the last available entry
		activePipes.list[activePipes.size][0] = readPipe;
        activePipes.list[activePipes.size][1] = writePipe;

		// Increment size
		activePipes.size++;

		return NPI_LNX_SUCCESS;
	}
	else
	{
		// There's no more room in the list
		npi_ipc_errno = NPI_LNX_ERROR_IPC_ADD_TO_ACTIVE_LIST_NO_ROOM;
		return NPI_LNX_FAILURE;
	}
}

/**************************************************************************************************
 *
 * @fn          removeFromActiveList
 *
 * @brief       Manage active connections, remove from list. Re organize so list is full
 * 				up to its declared size
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      -1 if something went wrong, 0 if success
 *
 **************************************************************************************************/

int removeFromActiveList(int pipe)
{
	int i;
	// Find entry
	for (i = 0; i < activePipes.size; i++)
	{
		if (activePipes.list[i][0] == pipe || activePipes.list[i][1] == pipe)
			break;
	}

    close(activePipes.list[i][0]);
    close(activePipes.list[i][1]);

	if (i < activePipes.size)
	{
		//Check if the last active conection has been removed
		if (activePipes.size == 1)
		{
			//continue to wait for new connection
			activePipes.size = 0;
			activePipes.list[0][0] = 0;
            activePipes.list[0][1] = 0;
            uiPrintfEx(trINFO, "No  Active Connections");
		}
		else
		{

			// Found our entry, replace this entry by the last entry
			activePipes.list[i][0] = activePipes.list[activePipes.size - 1][0];
			activePipes.list[i][1] = activePipes.list[activePipes.size - 1][1];

			// Decrement size
			activePipes.size--;
#ifdef __BIG_DEBUG__
			uiPrintfEx(trINFO, "Remaining Active Connections: #%d", activePipes.list[0][0]);
			// Send data to all connections, except listener
			for (i = 1; i < activePipes.size; i++)
			{
				rt_kprintf(", #%d", activePipes.list[i][0]);
			}
			rt_kprintf("\n");
#endif //__BIG_DEBUG__
		}
		return NPI_LNX_SUCCESS;
	}
	else
	{
		// Could not find entry
		npi_ipc_errno = NPI_LNX_ERROR_IPC_REMOVE_FROM_ACTIVE_LIST_NOT_FOUND;
		return NPI_LNX_FAILURE;
	}
}

/**************************************************************************************************
 *
 * @fn          NPI_LNX_IPC_ConnectionHandle
 *
 * @brief       Handle connections
 *
 * input parameters
 *
 * None.
 *
 * output parameters
 *
 * None.
 *
 * @return      STATUS
 *
 **************************************************************************************************/
int NPI_LNX_IPC_ConnectionHandle(int readPipe)
{
	int n, i, ret = NPI_LNX_SUCCESS;
    int writePipe;

	// Handle the readPipe
    uiPrintfEx(trINFO, "Receive message...\n");

	// Receive only NPI header first. Then then number of bytes indicated by length.
	n = read(readPipe, npi_ipc_buf[0], RPC_FRAME_HDR_SZ);
	if (n <= 0)
	{
		if (n < 0)
		{
			perror("recv");
			if ( (errno == ENOTSOCK) || (errno == EPIPE))
			{
				uiPrintfEx(trINFO, "[ERROR] Tried to read #%d as socket\n", readPipe);
				uiPrintfEx(trINFO, "Will disconnect #%d\n", readPipe);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT;
				ret = NPI_LNX_FAILURE;
			}
			else if (errno == ECONNRESET)
			{
				uiPrintfEx(trINFO, "[WARNING] Client disconnect while attempting to send to it\n");
				uiPrintfEx(trINFO, "Will disconnect #%d\n", readPipe);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT;
				ret = NPI_LNX_FAILURE;
			}
			else
			{
				npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_CHECK_ERRNO;
				ret = NPI_LNX_FAILURE;
			}
		}
		else
		{
			uiPrintfEx(trINFO, "Will disconnect #%d\n", readPipe);
			npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT;
			ret = NPI_LNX_FAILURE;
		}
	}
	else if (n == RPC_FRAME_HDR_SZ)
	{
		// Now read out the payload of the NPI message, if it exists
		if (((npiMsgData_t *) npi_ipc_buf[0])->len > 0)
		{
			n = read(readPipe, (uint8*) &npi_ipc_buf[0][RPC_FRAME_HDR_SZ], ((npiMsgData_t *) npi_ipc_buf[0])->len);
			if (n != ((npiMsgData_t *) npi_ipc_buf[0])->len)
			{
				uiPrintfEx(trINFO, "[ERR] Could not read out the NPI payload. Requested %d, but read %d!\n",
						((npiMsgData_t *) npi_ipc_buf[0])->len, n);
				npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_TOO_FEW_BYTES;
				ret = NPI_LNX_FAILURE;
				if (n < 0)
				{
					uiPrintfEx(trINFO, "recv");
					// Disconnect this
					npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT;
					ret = NPI_LNX_FAILURE;
				}
			}
			else
			{
				ret = NPI_LNX_SUCCESS;
			}
			// n is only used by debug traces from here on, but add header length
			// so the whole message is written out
			n += RPC_FRAME_HDR_SZ;
		}
		/*
		 * Take the message from the client and pass it to the NPI
		 */

		if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_CMD_TYPE_MASK) == RPC_CMD_SREQ)
		{
			uiPrintfEx(trINFO, "NPI SREQ:  (len %d)", n);
			for (i = 0; i < n; i++)
			{
				rt_kprintf(" 0x%.2X", (uint8)npi_ipc_buf[0][i]);
			}
			rt_kprintf("\n");

			if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_SUBSYSTEM_MASK) == RPC_SYS_SRV_CTRL)
			{
				//SREQ Command send to this server.
				ret = npi_ServerCmdHandle((npiMsgData_t *)npi_ipc_buf[0]);
			}
			else
			{
				uint8 sreqHdr[RPC_FRAME_HDR_SZ] = {0};
				// Retain the header for later integrety check
				memcpy(sreqHdr, npi_ipc_buf[0], RPC_FRAME_HDR_SZ);
				// Synchronous request requires an answer...
				ret = (NPI_SendSynchDataFnArr[devIdx])(
						(npiMsgData_t *) npi_ipc_buf[0]);
				if ( (ret != NPI_LNX_SUCCESS) &&
						( (npi_ipc_errno == NPI_LNX_ERROR_HAL_GPIO_WAIT_SRDY_CLEAR_POLL_TIMEDOUT) ||
							(npi_ipc_errno == NPI_LNX_ERROR_HAL_GPIO_WAIT_SRDY_SET_POLL_TIMEDOUT) ))
				{
					// Report this error to client through a pseudo response
					((npiMsgData_t *) npi_ipc_buf[0])->len = 1;
					((npiMsgData_t *) npi_ipc_buf[0])->pData[0] = 0xFF;
				}
				else
				{
					// Capture incoherent SRSP, check type and subsystem
					if ( ( (((npiMsgData_t *) npi_ipc_buf[0])->subSys & ~(RPC_SUBSYSTEM_MASK)) != RPC_CMD_SRSP )
						||
						 ( (((npiMsgData_t *) npi_ipc_buf[0])->subSys & (RPC_SUBSYSTEM_MASK)) != (sreqHdr[RPC_POS_CMD0] & RPC_SUBSYSTEM_MASK))
						)
					{
						// Report this error to client through a pseudo response
						((npiMsgData_t *) npi_ipc_buf[0])->len = 1;
						((npiMsgData_t *) npi_ipc_buf[0])->subSys = (sreqHdr[RPC_POS_CMD0] & RPC_SUBSYSTEM_MASK) | RPC_CMD_SRSP;
						((npiMsgData_t *) npi_ipc_buf[0])->cmdId = sreqHdr[RPC_POS_CMD1];
						((npiMsgData_t *) npi_ipc_buf[0])->pData[0] = 0xFF;
					}
				}
			}

			if ( (ret == NPI_LNX_SUCCESS) ||
						(npi_ipc_errno == NPI_LNX_ERROR_HAL_GPIO_WAIT_SRDY_CLEAR_POLL_TIMEDOUT) ||
						(npi_ipc_errno == NPI_LNX_ERROR_HAL_GPIO_WAIT_SRDY_SET_POLL_TIMEDOUT) )
			{
				n = ((npiMsgData_t *) npi_ipc_buf[0])->len + RPC_FRAME_HDR_SZ;

				// Copy response into transmission buffer
				memcpy(npi_ipc_buf[1], npi_ipc_buf[0], n);

				// Command type is not set, so set it here
				((npiMsgData_t *) npi_ipc_buf[1])->subSys |= RPC_CMD_SRSP;

				if (n > 0)
				{
					uiPrintfEx(trINFO, "NPI SRSP: (len %d)", n);
					for (i = 0; i < n; i++)
					{
						rt_kprintf(" 0x%.2X", (uint8)npi_ipc_buf[1][i]);
					}
					rt_kprintf("\n");
				}
				else
				{
					uiPrintfEx(trINFO, "[ERR] SRSP is 0!\n");
				}

				//			pthread_mutex_lock(&npiSyncRespLock);
				// Send bytes
                writePipe = searchWritePipeFromActiveList(readPipe);
				ret = NPI_LNX_IPC_SendData(n, writePipe);
			}
			else
			{
				// Keep status from NPI_SendSynchDataFnArr
				uiPrintfEx(trINFO, "[ERR] SRSP: npi_ipc_errno 0x%.8X\n", npi_ipc_errno);
			}
		}
		else if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_CMD_TYPE_MASK) == RPC_CMD_AREQ)
		{
			uiPrintfEx(trINFO, "NPI AREQ: (len %d)", n);
			for (i = 0; i < n; i++)
			{
				rt_kprintf(" 0x%.2X", (uint8)npi_ipc_buf[0][i]);
			}
			rt_kprintf("\n");

			if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_SUBSYSTEM_MASK) == RPC_SYS_SRV_CTRL)
			{
				//AREQ Command send to this server.
				ret = npi_ServerCmdHandle((npiMsgData_t *)npi_ipc_buf[0]);
			}
			else
			{
				// Asynchronous request may just be sent
				ret = (NPI_SendAsynchDataFnArr[devIdx])(
						(npiMsgData_t *) npi_ipc_buf[0]);
			}
		}
		else if (((uint8) (((npiMsgData_t *) npi_ipc_buf[0])->subSys) & (uint8) RPC_CMD_TYPE_MASK)  == RPC_CMD_NOTIFY_ERR)
		{
			// An error occurred in a child thread.
			ret = NPI_LNX_FAILURE;
		}
		else
		{
			uiPrintfEx(trINFO, "Can only accept AREQ or SREQ for now...\n");
			uiPrintfEx(trINFO, "Unknown: (len %d)\n", n);
			for (i = 0; i < n; i++)
			{
				rt_kprintf(" 0x%.2X", (uint8)npi_ipc_buf[0][i]);
			}
			rt_kprintf("\n");

			npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_INCOMPATIBLE_CMD_TYPE;
			ret = NPI_LNX_FAILURE;
		}
	}

	if ((ret == NPI_LNX_FAILURE) && (npi_ipc_errno == NPI_LNX_ERROR_IPC_RECV_DATA_DISCONNECT))
	{
		uiPrintfEx(trINFO, "Done with %d\n", readPipe);
	}
	else
	{
		uiPrintfEx(trINFO, "!Done\n");
	}

	return ret;
}

/**************************************************************************************************
 *
 * @fn          NPI_LNX_IPC_SendData
 *
 * @brief       Send data from NPI to client
 *
 * input parameters
 *
 * @param          len                                                          - length of message to send
 * @param          connection                         - connection to send message (for synchronous response) otherwise -1 for all connections
 *
 * output parameters
 *
 * None.
 *
 * @return      STATUS
 *
 **************************************************************************************************/
int NPI_LNX_IPC_SendData(uint8 len, int writePipe)
{
	int bytesSent = 0, i, ret = NPI_LNX_SUCCESS;

	if (writePipe < 0)
	{
#ifdef __BIG_DEBUG__
		uiPrintfEx(trINFO, "Dispatch AREQ to all active connections: #%d", activePipes.list[0][0]);
		// Send data to all connections, except listener
		for (i = 1; i < activePipes.size; i++)
		{
			rt_kprintf(", %d", activePipes.list[i][0]);
		}
		rt_kprintf("\n");
#endif //__BIG_DEBUG__
		// Send data to all connections, except listener
		for (i = 0; i < activePipes.size; i++)
		{
            bytesSent = write(activePipes.list[i][1], npi_ipc_buf[1], len);
            
            uiPrintfEx(trINFO, "...sent %d bytes to Client #%d\n", bytesSent, activePipes.list[i][1]);
            
            if (bytesSent < 0)
            {
                if (errno != ENOTSOCK)
                {
                    char *errorStr = (char *)malloc(30);
                    sprintf(errorStr, "send %d, %d", activePipes.list[i][1], errno);
                    perror(errorStr);
                    // Remove from list if detected bad file descriptor
                    if (errno == EBADF)
                    {
                    	uiPrintfEx(trINFO, "Removing connection #%d\n", activePipes.list[i][1]);
                        // Connection closed. Remove from set
                        FD_CLR(activePipes.list[i][0], &activePipesFDs);
                        ret = removeFromActiveList(activePipes.list[i][0]);
                    }
                    else
                    {
                        npi_ipc_errno = NPI_LNX_ERROR_IPC_SEND_DATA_ALL;
                        ret = NPI_LNX_FAILURE;
                    }
                }
            }
            else if (bytesSent != len)
            {
            	uiPrintfEx(trINFO, "[ERROR] Failed to send all %d bytes on socket\n", len);
            }
		}
	}
	else
	{
		// Send to specific connection only
		bytesSent = write(writePipe, npi_ipc_buf[1], len);

		uiPrintfEx(trINFO, "...sent %d bytes to Client #%d\n", bytesSent, writePipe);

		if (bytesSent < 0)
		{
			perror("send");
			// Remove from list if detected bad file descriptor
			if (errno == EBADF)
			{
				uiPrintfEx(trINFO, "Removing connection #%d\n", writePipe);
				// Connection closed. Remove from set
				FD_CLR(writePipe, &activePipesFDs);
				ret = removeFromActiveList(writePipe);
				if (ret == NPI_LNX_SUCCESS)
				{
					npi_ipc_errno = NPI_LNX_ERROR_IPC_SEND_DATA_SPECIFIC_PIPE_REMOVED;
					ret = NPI_LNX_FAILURE;
				}
			}
			else
			{
				npi_ipc_errno = NPI_LNX_ERROR_IPC_SEND_DATA_SPECIFIC;
				ret = NPI_LNX_FAILURE;
			}
		}
	}

	return ret;
}

/**************************************************************************************************
 *
 * @fn          SerialConfigParser
 *
 * @brief       This function searches for a string a returns its value
 *
 * input parameters
 *
 * @param          configFilePath   - path to configuration file
 * @param          section          - section to search for
 * @param          key                                                         - key to return value of within section
 *
 * output parameters
 *
 * None.
 *
 * @return      None.
 *
 **************************************************************************************************/
int SerialConfigParser(RTT_FILE* serialCfgFd, const char* section,
		const char* key, char* resultString)
{
	uint8 sectionFound = FALSE, invalidLineLen = FALSE;
	char* resString = NULL;
	char* resStringToFree = NULL;
	char* psStr; // Processing string pointer
	int res = NPI_LNX_FAILURE;


	resString = malloc (128);
	if (resString == NULL)
	{
		npi_ipc_errno = NPI_LNX_ERROR_IPC_GENERIC;
		return NPI_LNX_FAILURE;
	}
	resStringToFree = resString;
	uiPrintfEx(trINFO, "------------------------------------------------------\n");
	uiPrintfEx(trINFO, "Serial Config Parsing:\n");
	uiPrintfEx(trINFO, "- \tSection: \t%s\n", section);
	uiPrintfEx(trINFO, "- \tKey: \t\t%s\n", key);

	// Do nothing if the file doesn't exist
	if (serialCfgFd != NULL)
	{
		// Make sure we start search from the beginning of the file
		rtt_fseek(serialCfgFd, 0, SEEK_SET);

		// Search through the configuration file for the wanted
		while ((resString = rtt_fgets(resString, 128, serialCfgFd)) != NULL)
		{
			// Check if we have a valid line, i.e. begins with [.
			// Note! No valid line can span more than 128 bytes. Hence we
			// must hold off parsing until we hit a newline.
			if (strlen(resString) == 128)
			{
				invalidLineLen = TRUE;
				//uiPrintfEx(trINFO, "Found line > 128 bytes\n");
				rtt_fflush(rtt_stdout);
			}
			else
			{
				// First time we find a valid line length after having
				// found invalid line length may be the end of the
				// invalid line. Hence, do not process this string.
				// We set the invalidLineLen parameter to FALSE after
				// the processing logic.
				if (invalidLineLen == FALSE)
				{
					// Remove the newline character (ok even if line had length 128)
					resString[strlen(resString) - 1] = '\0';

					//uiPrintfEx(trINFO, "Found line < 128 bytes\n");
					rtt_fflush(rtt_stdout);
					if (resString[0] == '[')
					{
						uiPrintfEx(trINFO, "Found section %s\n", resString);
						// Search for wanted section
						psStr = strstr(resString, section);
						if (psStr != NULL)
						{
							resString = psStr;
							// We found our wanted section. Now search for wanted key.
							sectionFound = TRUE;
							uiPrintfEx(trINFO, "Found wanted section!\n");
						}
						else
						{
							// We found another section.
							sectionFound = FALSE;
						}
					}
					else if (sectionFound == TRUE)
					{
						uiPrintfEx(trINFO, "Line to process %s (strlen=%d)\n",
								resString,
								strlen(resString));
						// We have found our section, now we search for wanted key
						// Check for commented lines, tagged with '#', and
						// lines > 0 in length
						if ((resString[0] != '#') && (strlen(resString) > 0))
						{
							// Search for wanted section
							psStr = strstr(resString, key);
							if (psStr != NULL)
							{
								uiPrintfEx(trINFO, "Found key \t'%s' in \t'%s'\n", key, resString);
								// We found our key. The value is located after the '='
								// after the key.
								//                                                                                                                            printf("%s\n", psStr);
								psStr = strtok(psStr, "=");
								//                                                                                                                            printf("%s\n", psStr);
								psStr = strtok(NULL, "=;\"");
								//                                                                                                                            printf("%s\n", psStr);

								resString = psStr;
								res = NPI_LNX_SUCCESS;
								uiPrintfEx(trINFO, "Found value '%s'\n", resString);
								strcpy(resultString, resString);
								uiPrintfEx(trINFO, "Found value2 '%s'\n", resultString);
								// We can return this string to the calling function
								break;
							}
						}
					}
				}
				else
				{
					uiPrintfEx(trINFO, "Found end of line > 128 bytes\n");
					invalidLineLen = FALSE;
				}
			}
		}
	}
	else
	{
		npi_ipc_errno = NPI_LNX_ERROR_IPC_SERIAL_CFG_FILE_DOES_NOT_EXIST;
		free(resStringToFree);
		return NPI_LNX_FAILURE;
	}

	free(resStringToFree);
	return res;
}

/**************************************************************************************************
 * @fn          NPI_AsynchMsgCback
 *
 * @brief       This function is a NPI callback to the client that indicates an
 *              asynchronous message has been received. The client software is
 *              expected to complete this call.
 *
 *              Note: The client must copy this message if it requires it
 *                    beyond the context of this call.
 *
 * input parameters
 *
 * @param       *pMsg - A pointer to an asynchronously received message.
 *
 * output parameters
 *
 * None.
 *
 * @return      STATUS
 **************************************************************************************************
 */
int NPI_AsynchMsgCback(npiMsgData_t *pMsg)
{
	int i;
	//	int ret = NPI_LNX_SUCCESS;

	uiPrintfEx(trINFO, "[-->] %d bytes, subSys 0x%.2X, cmdId 0x%.2X, pData:",
			pMsg->len,
			pMsg->subSys,
			pMsg->cmdId);
	for (i = 0; i < pMsg->len; i++)
	{
		rt_kprintf(" 0x%.2X", pMsg->pData[i]);
	}
	rt_kprintf("\n");

	memcpy(npi_ipc_buf[1], (uint8*) pMsg, pMsg->len + RPC_FRAME_HDR_SZ);

	return NPI_LNX_IPC_SendData(pMsg->len + RPC_FRAME_HDR_SZ, -1);
}


/**************************************************************************************************
 * @fn          NPI_LNX_IPC_Exit
 *
 * @brief       This function will exit gracefully
 *
 *
 * input parameters
 *
 * @param       ret	-	exit condition. Return on Success, exit on Failure
 *
 * output parameters
 *
 * None.
 *
 * @return      None
 **************************************************************************************************
 */
void NPI_LNX_IPC_Exit(int ret)
{
	uiPrintfEx(trINFO, "... freeing memory (ret %d)\n", ret);

	// Close file for parsing
	if (serialCfgFd != NULL)
	{
		rtt_fclose(serialCfgFd);
		serialCfgFd = NULL;
	}

	// Free memory for configuration buffers
	if (pStrBufRoot != NULL)
	{
		free(pStrBufRoot);
		pStrBufRoot = NULL;
	}

	if (ret != NPI_LNX_SUCCESS)
	{
		// Keep path for later use
		if (devPath != NULL)
		{
			free(devPath);
			devPath = NULL;
		}
	}

	uint8 gpioIdx;
	if (ret != NPI_LNX_SUCCESS)
	{
		// Keep GPIO paths for later use
		for (gpioIdx = 0; gpioIdx < 3; gpioIdx++)
		{
			if (gpioCfg[gpioIdx] != NULL)
			{
				free(gpioCfg[gpioIdx]);
				gpioCfg[gpioIdx] = NULL;
			}
		}
		if (gpioCfg != NULL)
		{
			free(gpioCfg);
			gpioCfg = NULL;
		}
	}

	if (ret == NPI_LNX_FAILURE)
	{
		// Don't even bother open a socket; device opening failed..
		uiPrintfEx(trINFO, "Could not open device... exiting\n");

		exit(npi_ipc_errno);
	}
}

/**************************************************************************************************
 * @fn          NPI_LNX_IPC_NotifyError
 *
 * @brief       This function allows other threads to notify of error conditions.
 *
 *
 * input parameters
 *
 * @param       source		- source identifier
 * @param       *errorMsg 	- A string containing the error message.
 *
 * output parameters
 *
 * None.
 *
 * @return      None
 **************************************************************************************************
 */
int NPI_LNX_IPC_NotifyError(uint16 source, const char* errorMsg)
{
	int ret = NPI_LNX_SUCCESS;
    int tmpWritePipe;
	npiMsgData_t msg;
	/**********************************************************************
	 * Connect to the NPI server
	 **********************************************************************/

    tmpWritePipe = open(NPI_IPC_LISTEN_PIPE_CLIENT2SERVER, O_WRONLY, 0);
    if(tmpWritePipe == -1)
    {
        //
    }

	uiPrintfEx(trINFO, "[NOTIFY_ERROR] Trying to connect...\n");

	if (ret == NPI_LNX_SUCCESS)
		uiPrintfEx(trINFO, "[NOTIFY_ERROR] Connected.\n");

	if (strlen(errorMsg) <= AP_MAX_BUF_LEN)
	{
		memcpy(msg.pData, errorMsg, strlen(errorMsg));
	}
	else
	{
		errorMsg = "Default msg. Requested msg too long.\n";
		memcpy(msg.pData, errorMsg, strlen(errorMsg));
		uiPrintfEx(trINFO, "[NOTIFY_ERROR] Size of error message too long (%d, max %d).\n",
				strlen(errorMsg),
				AP_MAX_BUF_LEN);
	}
	// If last character is \n then remove it.
	if ((msg.pData[strlen(errorMsg) - 1]) == '\n')
	{
		msg.pData[strlen(errorMsg) - 1] = 0;
		msg.len = strlen(errorMsg);
	}
	else
	{
		msg.pData[strlen(errorMsg)] = 0;
		msg.len = strlen(errorMsg) + 1;
	}

	// For now the only required info here is command type.
	msg.subSys = RPC_CMD_NOTIFY_ERR;
	// CmdId is filled with the source identifier.
	msg.cmdId = source;

	write(tmpWritePipe, &msg, msg.len + RPC_FRAME_HDR_SZ);

    close(tmpWritePipe);

	return ret;
}

static int npi_ServerCmdHandle(npiMsgData_t *pNpi_ipc_buf)
{
	int ret = NPI_LNX_SUCCESS;

	switch(pNpi_ipc_buf->cmdId)
	{
		case NPI_LNX_CMD_ID_CTRL_TIME_PRINT_REQ:
        {
        	uiPrintfEx(trINFO, "NPI_Server not compiled to support time stamps\n");
            // Set return status
            pNpi_ipc_buf->len = 1;
            pNpi_ipc_buf->pData[0] = (uint8) NPI_LNX_FAILURE;
            pNpi_ipc_buf->subSys = RPC_SYS_SRV_CTRL;
            ret = NPI_LNX_SUCCESS;
		}
		break;
		case NPI_LNX_CMD_ID_CTRL_BIG_DEBUG_PRINT_REQ:
		{
			__BIG_DEBUG_ACTIVE = pNpi_ipc_buf->pData[0];
			if (__BIG_DEBUG_ACTIVE == FALSE)
			{
				uiPrintfEx(trINFO, "__BIG_DEBUG_ACTIVE set to FALSE\n");
			}
			else
			{
				uiPrintfEx(trINFO, "__BIG_DEBUG_ACTIVE set to TRUE\n");
			}
			// Set return status
			pNpi_ipc_buf->len = 1;
			pNpi_ipc_buf->pData[0] = NPI_LNX_SUCCESS;
			ret = NPI_LNX_SUCCESS;
			break;
		}
		case NPI_LNX_CMD_ID_VERSION_REQ:
		{
			// Set return status
			pNpi_ipc_buf->len = 4;
			pNpi_ipc_buf->pData[0] = NPI_LNX_SUCCESS;
			pNpi_ipc_buf->pData[1] = NPI_LNX_MAJOR_VERSION;
			pNpi_ipc_buf->pData[2] = NPI_LNX_MINOR_VERSION;
			pNpi_ipc_buf->pData[3] = NPI_LNX_REVISION;
			ret = NPI_LNX_SUCCESS;
		}
		break;
		case NPI_LNX_CMD_ID_GET_PARAM_REQ:
		{
			// Set return status
			switch(pNpi_ipc_buf->pData[0])
			{
				case NPI_LNX_PARAM_NB_CONNECTIONS:
					pNpi_ipc_buf->len = 3;
					pNpi_ipc_buf->pData[0] = NPI_LNX_SUCCESS;
					//Number of Active Connections
					pNpi_ipc_buf->pData[1] = activePipes.size;
					//Max number of possible connections.
					pNpi_ipc_buf->pData[2] = NPI_SERVER_PIPE_QUEUE_SIZE;

					ret = NPI_LNX_SUCCESS;
					break;
				case NPI_LNX_PARAM_DEVICE_USED:
					pNpi_ipc_buf->len = 2;
					pNpi_ipc_buf->pData[0] = NPI_LNX_SUCCESS;
					//device open and used by the sever
					pNpi_ipc_buf->pData[1] = devIdx;

					ret = NPI_LNX_SUCCESS;
					break;

				default:
					npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_INVALID_GET_PARAM_CMD;
					ret = NPI_LNX_FAILURE;
					break;
			}
		}
		break;
		case NPI_LNX_CMD_ID_RESET_DEVICE:
		{
			if (devIdx == NPI_SPI_FN_ARR_IDX)
			{
				// Perform Reset of the RNP
				(NPI_ResetSlaveFnArr[devIdx])();

				// Do the Hw Handshake
				(NPI_SynchSlaveFnArr[devIdx])();
			}
			else if (devIdx == NPI_I2C_FN_ARR_IDX)
			{
				// Perform Reset of the RNP
				(NPI_ResetSlaveFnArr[devIdx])();
			}
			else
			{
				ret = HalGpioResetSet(FALSE);
				uiPrintfEx(trINFO, "Resetting device\n");
				usleep(1);
				ret = HalGpioResetSet(TRUE);
			}
		}
		break;
		case NPI_LNX_CMD_ID_DISCONNECT_DEVICE:
		{
			uiPrintfEx(trINFO, "Trying to disconnect device %d\n", devIdx);
			(NPI_CloseDeviceFnArr[devIdx])();
			uiPrintfEx(trINFO, "Preparing return message after disconnecting device %d\n", devIdx);
			pNpi_ipc_buf->len = 1;
			pNpi_ipc_buf->pData[0] = NPI_LNX_SUCCESS;
		}
		break;
		case NPI_LNX_CMD_ID_CONNECT_DEVICE:
		{
			uiPrintfEx(trINFO, "Trying to connect to device %d, %s\n", devIdx, devPath);
			switch(devIdx)
			{
                case NPI_UART_FN_ARR_IDX:
    #if (defined NPI_UART) && (NPI_UART == TRUE)
                {
                    npiUartCfg_t uartCfg;
                    char* strBuf;
                    strBuf = (char*) malloc(128);
                    strBuf = pStrBufRoot;
                    if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "speed", strBuf)))
                    {
                        uartCfg.speed = atoi(strBuf);
                    }
                    else
                    {
                        uartCfg.speed=115200;
                    }
                    if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "UART", "flowcontrol", strBuf)))
                    {
                        uartCfg.flowcontrol = atoi(strBuf);
                    }
                    else
                    {
                        uartCfg.flowcontrol=0;
                    }
                    free(strBuf);
                    ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiUartCfg_t *)&uartCfg);
                }
    #endif
                break;
                case NPI_SPI_FN_ARR_IDX:
    #if (defined NPI_SPI) && (NPI_SPI == TRUE)
                {
                    halSpiCfg_t halSpiCfg;
                    npiSpiCfg_t npiSpiCfg;
                    char* strBuf;
                    strBuf = (char*) malloc(128);
                    if (serialCfgFd != NULL)
                    {
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "speed", strBuf)))
                        {
                            halSpiCfg.speed = atoi(strBuf);
                        }
                        else
                        {
                            halSpiCfg.speed=500000;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "mode", strBuf)))
                        {
                            halSpiCfg.mode = strtol(strBuf, NULL, 16);
                        }
                        else
                        {
                            halSpiCfg.mode = 0;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "bitsPerWord", strBuf)))
                        {
                            halSpiCfg.bitsPerWord = strtol(strBuf, NULL, 10);
                        }
                        else
                        {
                            halSpiCfg.bitsPerWord = 0;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "useFullDuplexAPI", strBuf)))
                        {
                            halSpiCfg.useFullDuplexAPI = strtol(strBuf, NULL, 10);
                        }
                        else
                        {
                            halSpiCfg.useFullDuplexAPI = TRUE;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "earlyMrdyDeAssert", strBuf)))
                        {
                            npiSpiCfg.earlyMrdyDeAssert = strtol(strBuf, NULL, 10);
                        }
                        else
                        {
                            // If it is not defined then set value for RNP
                            npiSpiCfg.earlyMrdyDeAssert = TRUE;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "detectResetFromSlowSrdyAssert", strBuf)))
                        {
                            npiSpiCfg.detectResetFromSlowSrdyAssert = strtol(strBuf, NULL, 10);
                        }
                        else
                        {
                            // If it is not defined then set value for RNP
                            npiSpiCfg.detectResetFromSlowSrdyAssert = TRUE;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "forceRunOnReset", strBuf)))
                        {
                            npiSpiCfg.forceRunOnReset = strtol(strBuf, NULL, 16);
                        }
                        else
                        {
                            // If it is not defined then set value for RNP
                            npiSpiCfg.forceRunOnReset = NPI_LNX_UINT8_ERROR;
                        }
                        if (NPI_LNX_SUCCESS == (SerialConfigParser(serialCfgFd, "SPI", "srdyMrdyHandshakeSupport", strBuf)))
                        {
                            npiSpiCfg.srdyMrdyHandshakeSupport = strtol(strBuf, NULL, 10);
                        }
                        else
                        {
                            // If it is not defined then set value for RNP
                            npiSpiCfg.srdyMrdyHandshakeSupport = TRUE;
                        }
                    }
                    else
                    {
                        halSpiCfg.speed=500000;
                        halSpiCfg.mode = 0;
                        halSpiCfg.bitsPerWord = 8;
                        // If it is not defined then set value for RNP
                        npiSpiCfg.earlyMrdyDeAssert = TRUE;
                        // If it is not defined then set value for RNP
                        npiSpiCfg.detectResetFromSlowSrdyAssert = TRUE;
                        // If it is not defined then set value for RNP
                        npiSpiCfg.forceRunOnReset = NPI_LNX_UINT8_ERROR;
                        // If it is not defined then set value for RNP
                        npiSpiCfg.srdyMrdyHandshakeSupport = TRUE;
                    }
                    // GPIO config is stored
                    npiSpiCfg.gpioCfg = gpioCfg;
                    npiSpiCfg.spiCfg = &halSpiCfg;
                    free(strBuf);

                    // Now open device for processing
                    ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiSpiCfg_t *) &npiSpiCfg);

                    // Must also reset and synch

                    // Perform Reset of the RNP
                    (NPI_ResetSlaveFnArr[devIdx])();

                    // Do the Hw Handshake
                    (NPI_SynchSlaveFnArr[devIdx])();

                    // Since SPI does not indicate reset to host we should notify here
                    // but there's no unified way of doing it for RNP and ZNP...
                    // For RemoTI we can send RTI_ResetInd(). This message should just
                    // be discarded by anything but RNP, so should be safe.
                    ((npiMsgData_t *) npi_ipc_buf[1])->len = 0;
                    ((npiMsgData_t *) npi_ipc_buf[1])->subSys = 0x4A;
                    ((npiMsgData_t *) npi_ipc_buf[1])->cmdId = 0x0D;
                    NPI_LNX_IPC_SendData(((npiMsgData_t *) npi_ipc_buf[1])->len + RPC_FRAME_HDR_SZ, -1);
                }
    #endif
                break;

                case NPI_I2C_FN_ARR_IDX:
    #if (defined NPI_I2C) && (NPI_I2C == TRUE)
                {
                    npiI2cCfg_t i2cCfg;
                    i2cCfg.gpioCfg = gpioCfg;

                    // Open the Device and perform a reset
                    ret = (NPI_OpenDeviceFnArr[devIdx])(devPath, (npiI2cCfg_t *) &i2cCfg);
                }
    #endif
                break;
                default:
                    ret = NPI_LNX_FAILURE;
                    break;
			}
			uiPrintfEx(trINFO, "Preparing return message after connecting to device %d (ret == 0x%.2X, npi_ipc_errno == 0x%.2X)\n",
					devIdx, ret, npi_ipc_errno);
			pNpi_ipc_buf->len = 1;
			pNpi_ipc_buf->pData[0] = ret;
		}
		break;
		default:
		{
			npi_ipc_errno = NPI_LNX_ERROR_IPC_RECV_DATA_INVALID_SREQ;
			ret = NPI_LNX_FAILURE;
			break;
		}
	}
	return ret;
}

#ifdef RT_USING_COMPONENTS_DRIVERS_PIPE_TEST_NPI_IPC
void npi_rtt_ipc_pipe_main(void* args)
{
	int  fd;
	int  ret_size;

	fd=open("/dev/npi_ipc_c2s", O_WRONLY, 0);
	if(fd==-1)
	{
		if(errno==ENXIO)
		{
			rt_kprintf("open /dev/npi_ipc_c2s for write error; no reading process\n");
		}
	}
	else
	{
		rt_kprintf("open pipe for write ok.\n");
	}
	while(1)
	{
		ret_size=write(fd,NPI_IPC_LISTEN_PIPE_CHECK_STRING,strlen(NPI_IPC_LISTEN_PIPE_CHECK_STRING));
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
#endif /* npi_ipc_pipe_thread */
/**************************************************************************************************
 **************************************************************************************************/

