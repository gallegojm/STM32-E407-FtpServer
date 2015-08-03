/*
    FTP Server for STM32-E407 and ChibiOS
    Copyright (C) 2015 Jean-Michel Gallego

    See readme.txt for information

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef _FTPS_H_
#define _FTPS_H_

#include "ch.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "ff.h"

#include "console.h"

#define FTP_VERSION              "FTP-2015-07-31"

#define FTP_USER                 "Stm32"
#define FTP_PASS                 "Chibi"

#define FTP_SERVER_PORT          21
#define FTP_DATA_PORT            55600         // Data port in passive mode
#define FTP_TIME_OUT             10            // Disconnect client after 5 minutes of inactivity
#define FTP_PARAM_SIZE           _MAX_LFN + 8
#define FTP_CWD_SIZE             _MAX_LFN + 8  // max size of a directory name

// number of clients we want to serve simultaneously
#define FTP_NBR_CLIENTS          5

// size of file buffer for reading a file
#define FTP_BUF_SIZE             512

#define SERVER_THREAD_STACK_SIZE 256
//#define FTP_THREAD_STACK_SIZE    ( 1536 + FTP_BUF_SIZE + ( 5 * _MAX_LFN ))
#define FTP_THREAD_STACK_SIZE    ( 1600 + FTP_BUF_SIZE + ( 5 * _MAX_LFN ))

#define FTP_THREAD_PRIORITY      (LOWPRIO + 2)

extern THD_WORKING_AREA( wa_ftp_server, SERVER_THREAD_STACK_SIZE );

// define a structure of parameters for a ftp thread
struct server_stru
{
  uint8_t num;
  // uint8_t fase;   // for debugging only
  struct netconn *ftpconn;
  binary_semaphore_t semrequest;
};

#ifdef __cplusplus
extern "C" {
#endif
  THD_FUNCTION( ftp_server, p );
#ifdef __cplusplus
}
#endif

enum dcm_type     // Data Connection mode:
{
  NOTSET  = 0,    //   not set
  PASSIVE = 1,    //   passive
  ACTIVE  = 2,    //   active
};

class FtpServer
{
public:
  void service( int8_t n, struct netconn *dscn );

private:
  bool processCommand( char * command, char * parameters );
  int8_t readCommand();

  void sendBegin( const char * s );
  void sendCat( const char * s );
  void sendWrite( const char * s );
  void sendCatWrite( const char * s );
  void sendWrite();

  bool listenDataConn();
  bool dataConnect();
  void dataClose();
  void dataWrite( const char * data );
  void closeTransfer();

  bool makePathFrom( char * fullName, char * param );
  bool makePath( char * fullName );
  bool fs_exists( char * path );
  bool fs_opendir( DIR * pdir, char * dirName );

  char * i2str( int32_t i );
  char * makeDateTimeStr( uint16_t date, uint16_t time );
  int8_t getDateTime( uint16_t * pdate, uint16_t * ptime );

  struct    netconn * listdataconn, * dataconn, * ctrlconn;
  struct    netbuf  * inbuf;
  struct    ip_addr ipclient;
  struct    ip_addr ipserver;

  FIL       file;
  FILINFO   finfo;
  char      lfn[ _MAX_LFN + 1 ];          // Buffer to store the LFN

  uint16_t  dataPort;
  int8_t    cmdStatus;                    // status of ftp command connection
  char      command[ 5 ];                 // command sent by client
  char      parameters[ FTP_PARAM_SIZE ]; // parameters sent by client
  char      cwdName[ FTP_CWD_SIZE ];      // name of current directory
  char      cwdRNFR[ FTP_CWD_SIZE ];      // name of origin directory for Rename command
  char      path[ FTP_CWD_SIZE ];
  char      str[ 25 ];
  systime_t timeBeginTrans;
  uint32_t  bytesTransfered;
  int8_t    nerr;
  uint8_t   num;
  char      buf[ FTP_BUF_SIZE ];           // data buffer for communication
  uint16_t  pbuf;
  dcm_type  dataConnMode;
};

#endif // _FTPS_H_
