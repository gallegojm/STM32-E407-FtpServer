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

#include <string.h>

#include <ftps/ftps.h>

// Stack area for the ftp server thread.
THD_WORKING_AREA( wa_ftp_server, SERVER_THREAD_STACK_SIZE );

//  Stack areas for the ftp threads.
THD_WORKING_AREA( wa_ftp_conn[ FTP_NBR_CLIENTS ], FTP_THREAD_STACK_SIZE );

//  array of parameters for each ftp thread
struct server_stru ss[ FTP_NBR_CLIENTS ];

//  FTP connection thread.

THD_FUNCTION( ftp_conn, p )
{
  struct server_stru * pss = (server_stru *) p;
  char tn[] = "ftp_conn_n";
  FtpServer ftpSrv;

  // names thread as ftp_conn_1, ftp_conn_2, ...
  tn[ strlen( tn ) - 1 ] = pss->num + '1';
  chRegSetThreadName( tn );

  while( true )
  {
    // wait for a connection
    if( chBSemWait( & pss->semrequest ) == MSG_OK )
    {
      ftpSrv.service( pss->num, pss->ftpconn );   // call the http function
      netconn_delete( pss->ftpconn );             // delete the connection.
      pss->ftpconn = NULL;
    }
  }
}

//  FTP server thread.

THD_FUNCTION( ftp_server, p )
{
  struct netconn * ftpsrvconn;
  uint8_t i;

  (void)p;
  chRegSetThreadName( "ftp_server" );

  //  Initialize ftp thread' parameters for each thread
  for( i = 0; i < FTP_NBR_CLIENTS; i ++ )
  {
    ss[ i ].num = i;
    ss[ i ].fase = 0;
    ss[ i ].ftpconn = NULL;
    chBSemObjectInit( & ss[ i ].semrequest, true );
  }

  //  Creates the FTP threads
  for( i = 0; i < FTP_NBR_CLIENTS; i ++ )
    chThdCreateStatic( wa_ftp_conn[ i ], sizeof( wa_ftp_conn[ i ] ),
                       FTP_THREAD_PRIORITY, ftp_conn, & ss[ i ] );

  // Create the TCP connection handle
  ftpsrvconn = netconn_new( NETCONN_TCP );
  LWIP_ERROR( "http_server: invalid ftpsrvconn", (ftpsrvconn != NULL), return; );

  // Bind to port 21 (FTP) with default IP address
  //    and put the connection into LISTEN state
  netconn_bind( ftpsrvconn, NULL, FTP_SERVER_PORT );
  netconn_listen( ftpsrvconn );

  //  Goes to the final priority after initialization
  chThdSetPriority( FTP_THREAD_PRIORITY );

  while( true )
  {
    // Look for the first connection not used
    for( i = 0; i < FTP_NBR_CLIENTS; i ++ )
      if( ss[ i ].ftpconn == NULL )
        break;
    if( i == FTP_NBR_CLIENTS )
      // All connections occupied
      chThdSleepMilliseconds( 300 );
    else if( netconn_accept( ftpsrvconn, & ss[ i ].ftpconn ) == ERR_OK )
      // New client request, wake up the corresponding thread
      chBSemSignal( & ss[ i ].semrequest );
  }
}
