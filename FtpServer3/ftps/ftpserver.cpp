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
#include <ctype.h>
#include <stdlib.h>

#include "ftps.h"
#include <ntpc/ntpc.h>
#include <sdlog/sdlog.h>
#include <util.h>

extern bool   fast_blink;
extern struct ntp_stru ntps;
extern struct server_stru ss[ FTP_NBR_CLIENTS ];

// =========================================================
//
//              Send a response to the client
//
// =========================================================

void FtpServer::sendBegin( const char * s )
{
  strncpy( buf, s, FTP_BUF_SIZE );
}

void FtpServer::sendCat( const char * s )
{
  size_t len = FTP_BUF_SIZE - strlen( buf );
  strncat( buf, s, len );
}

void FtpServer::sendWrite( const char * s )
{
  sendBegin( s );
  sendWrite();
}

void FtpServer::sendCatWrite( const char * s )
{
  sendCat( s );
  sendWrite();
}

void FtpServer::sendWrite()
{
  if( strlen( buf ) + 2 < FTP_BUF_SIZE )
    strcat( buf, "\r\n" );
  netconn_write( ctrlconn, buf, strlen( buf ), NETCONN_COPY );
  COMMAND_PRINT( ">%u> %s", num, buf );
}

//  Convert an integer to string
//
//  Return pointer to string

char * FtpServer::i2str( int32_t i )
{
  return int2str( str, i, 12 );
}

// Create string YYYYMMDDHHMMSS from date and time
//
// parameters:
//    date, time
//
// return:
//    pointer to string

char * FtpServer::makeDateTimeStr( uint16_t date, uint16_t time )
{
  int2strZ( str, (( date & 0xFE00 ) >> 9 ) + 1980, -5 );
  int2strZ( str + 4, ( date & 0x01E0 ) >> 5, -3 );
  int2strZ( str + 6, date & 0x001F, -3 );
  int2strZ( str + 8, ( time & 0xF800 ) >> 11, -3 );
  int2strZ( str + 10, ( time & 0x07E0 ) >> 5, -3 );
  int2strZ( str + 12, ( time & 0x001F ) << 1, -3 );
  return str;
}

// Calculate date and time from first parameter sent by MDTM command (YYYYMMDDHHMMSS)
//
// parameters:
//   pdate, ptime: pointer of variables where to store data
//
// return:
//    length of (time parameter + space) if date/time are ok
//    0 if parameter is not YYYYMMDDHHMMSS

int8_t FtpServer::getDateTime( uint16_t * pdate, uint16_t * ptime )
{
  // Date/time are expressed as a 14 digits long string
  //   terminated by a space and followed by name of file
  if( strlen( parameters ) < 15 || parameters[ 14 ] != ' ' )
    return 0;
  for( uint8_t i = 0; i < 14; i++ )
    if( ! isdigit( parameters[ i ]))
      return 0;

  parameters[ 14 ] = 0;
  * ptime = atoi( parameters + 12 ) >> 1;   // seconds
  parameters[ 12 ] = 0;
  * ptime |= atoi( parameters + 10 ) << 5;  // minutes
  parameters[ 10 ] = 0;
  * ptime |= atoi( parameters + 8 ) << 11;  // hours
  parameters[ 8 ] = 0;
  * pdate = atoi( parameters + 6 );         // days
  parameters[ 6 ] = 0;
  * pdate |= atoi( parameters + 4 ) << 5;   // months
  parameters[ 4 ] = 0;
  * pdate |= ( atoi( parameters ) - 1980 ) << 9;       // years

  return 15;
}

// =========================================================
//
//             Get a command from the client
//
// =========================================================

// update variables command and parameters
//
// return: -4 time out
//         -3 error receiving data
//         -2 command line too long
//         -1 syntax error
//          0 command without parameters
//          >0 length of parameters

int8_t FtpServer::readCommand()
{
  char   * pbuf;
  uint16_t buflen;
  int8_t   rc = 0;
  int8_t   i;
  char     car;

  command[ 0 ] = 0;
  parameters[ 0 ] = 0;
  nerr = netconn_recv( ctrlconn, & inbuf );
  if( nerr == ERR_TIMEOUT )
    return -4;
  if( nerr != ERR_OK )
    return -3;
  netbuf_data( inbuf, (void **) & pbuf, & buflen );
  if( buflen == 0 )
    goto deletebuf;
  i = 0;
  car = pbuf[ 0 ];
  do
  {
    if( ! isalpha( car ))
      break;
    command[ i ++ ] = car;
    car = pbuf[ i ];
  }
  while( i < buflen && i < 4 );
  command[ i ] = 0;
  if( car != ' ' )
    goto deletebuf;
  do
    if( i > buflen + 2 )
      goto deletebuf;
  while( pbuf[ i ++ ] == ' ' );
  rc = i;
  do
    car = pbuf[ rc ++ ];
  while( car != '\n' && car != '\r' && rc < buflen );
  if( rc == buflen )
  {
    rc = -1;
    goto deletebuf;
  }
  if( rc - i - 1 >= FTP_PARAM_SIZE )
  {
    rc = -2;
    goto deletebuf;
  }
  strncpy( parameters, pbuf + i - 1, rc - i );
  parameters[ rc - i ] = 0;
  rc = rc - i;

  deletebuf:
  COMMAND_PRINT( "<%u< %s %s\r\n", num, command, parameters );
  netbuf_delete( inbuf );
  return rc;
}

// =========================================================
//
//               Functions for data connection
//
// =========================================================

bool FtpServer::listenDataConn()
{
  bool ok = true;

  // If this is not already done, create the TCP connection handle
  //   to listen to client to open data connection
  if( listdataconn == NULL )
  {
    listdataconn = netconn_new( NETCONN_TCP );
    ok = listdataconn != NULL;
    if( ok )
    {
      // Bind listdataconn to port (FTP_DATA_PORT+num) with default IP address
      nerr = netconn_bind( listdataconn, IP_ADDR_ANY, dataPort );
      ok = nerr == ERR_OK;
    }
    if( ok )
    {
      // Put the connection into LISTEN state
      nerr = netconn_listen( listdataconn );
      ok = nerr == ERR_OK;
    }
  }
  if( ! ok )
    DEBUG_PRINT( "Error in listenDataConn()\r\n" );
  return ok;
}

bool FtpServer::dataConnect()
{
  nerr = ERR_CONN;

  if( dataConnMode == NOTSET )
  {
    DEBUG_PRINT( "No connecting mode defined\r\n" );
    goto error;
  }
  DEBUG_PRINT( "Connecting in %s mode\r\n",
               ( dataConnMode == PASSIVE ? "passive" : "active" ));

  if( dataConnMode == PASSIVE )
  {
    if( listdataconn == NULL )
      goto error;
    // Wait for connection from client during 500 ms
    netconn_set_recvtimeout( listdataconn, MS2ST( 500 ));
    nerr = netconn_accept( listdataconn, & dataconn );
    if( nerr != ERR_OK )
    {
      DEBUG_PRINT( "Error in dataConnect(): netconn_accept\r\n" );
      goto error;
    }
  }
  else
  {
    //  Create a new TCP connection handle
    dataconn = netconn_new( NETCONN_TCP );
    if( dataconn == NULL )
    {
      DEBUG_PRINT( "Error in dataConnect(): netconn_new\r\n" );
      // goto delconn;
      goto error;
    }
    nerr = netconn_bind( dataconn, IP_ADDR_ANY, 0 ); //dataPort );
    //  Connect to data port with client IP address
    if( nerr != ERR_OK )
    {
      DEBUG_PRINT( "Error %u in dataConnect(): netconn_bind\r\n", abs( nerr ));
      // goto error;   // pas sûr !!!
      goto delconn;
    }
    nerr = netconn_connect( dataconn, & ipclient, dataPort );
    if( nerr != ERR_OK )
    {
      DEBUG_PRINT( "Error %u in dataConnect(): netconn_connect\r\n", abs( nerr ));
      // goto error;   // sûr !!!
      goto delconn;
    }
  }
  return true;

  delconn:
  if( dataconn != NULL )
  {
    netconn_delete( dataconn );
    dataconn = NULL;
  }

  error:
  sendWrite( "425 No data connection" );
  return false;
}

void FtpServer::dataClose()
{
  dataConnMode = NOTSET;
  if( dataconn == NULL )
    return;
  netconn_close( dataconn );
  netconn_delete( dataconn );
  dataconn = NULL;
}

void FtpServer::dataWrite( const char * data )
{
  netconn_write( dataconn, data, strlen( data ), NETCONN_COPY );
  // COMMAND_PRINT( data );
}

// =========================================================
//
//                  Functions on files
//
// =========================================================

// Make complete path/name from cwdName and parameters
//
// 3 possible cases:
//   parameters can be absolute path, relative path or only the name
//
// parameters:
//   fullName : where to store the path/name
//
// return:
//   true, if done

bool FtpServer::makePathFrom( char * fullName, char * param )
{
  // Root or empty?
  if( ! strcmp( param, "/" ) || strlen( param ) == 0 )
  {
    strcpy( fullName, "/" );
    return true;
  }
  // If relative path, concatenate with current dir
  if( param[0] != '/' )
  {
    strcpy( fullName, cwdName );
    if( fullName[ strlen( fullName ) - 1 ] != '/' )
      strncat( fullName, "/", FTP_CWD_SIZE );
    strncat( fullName, param, FTP_CWD_SIZE );
  }
  else
    strcpy( fullName, param );
  // If ends with '/', remove it
  uint16_t strl = strlen( fullName ) - 1;
  if( fullName[ strl ] == '/' && strl > 1 )
    fullName[ strl ] = 0;
  if( strlen( fullName ) < FTP_CWD_SIZE )
    return true;
  sendWrite( "500 Command line too long" );
  return false;
}

bool FtpServer::makePath( char * fullName )
{
  return makePathFrom( fullName, parameters );
}

void FtpServer::closeTransfer()
{
  uint32_t deltaT = (uint32_t) ( chVTGetSystemTimeX() - timeBeginTrans );
  if( deltaT > 0 && bytesTransfered > 0 )
  {
    sendBegin( "226-File successfully transferred\r\n" );
    sendCat( "226 " );
    sendCat( i2str( deltaT ));
    sendCat( " ms, " );
    uint32_t bps;
    if( bytesTransfered < 0x7fffffff / CH_CFG_ST_FREQUENCY )
      bps = ( bytesTransfered * CH_CFG_ST_FREQUENCY ) / deltaT;
    else
      bps = ( bytesTransfered / deltaT ) * CH_CFG_ST_FREQUENCY;
    if( bps > 10000 )
    {
      sendCat( i2str( bps / 1000 ));
      sendCatWrite( " kbytes/s" );
    }
    else
    {
      sendCat( i2str( bps ));
      sendCatWrite( " bytes/s" );
    }
  }
  else
    sendWrite( "226 File successfully transferred" );
}

// Return true if a file or directory exists
//
// parameters:
//   path : absolute name of file or directory

bool FtpServer::fs_exists( char * path )
{
  if( ! strcmp( path, "/" ) )
    return true;

  char *  path0 = path;

  return f_stat( path0, & finfo ) == FR_OK;
}

// Open a directory
//
// parameters:
//   path : absolute name of directory
//
// return true if opened

bool FtpServer::fs_opendir( DIR * pdir, char * dirName )
{
  char * dirName0 = dirName;
  uint8_t ffs_result;

  ffs_result = f_opendir( pdir, dirName0 );
  return ffs_result == FR_OK;
}

// =========================================================
//
//                   Process a command
//
// =========================================================

bool FtpServer::processCommand( char * command, char * parameters )
{
  fast_blink = TRUE;

  ///////////////////////////////////////
  //                                   //
  //      ACCESS CONTROL COMMANDS      //
  //                                   //
  ///////////////////////////////////////

  //
  //  QUIT
  //
  if( ! strcmp( command, "QUIT" ))
    return FALSE;
  //
  //  PWD - Print Directory
  //
  else if( ! strcmp( command, "PWD" ) ||
           ( ! strcmp( command, "CWD" ) && ! strcmp( parameters, "." )))  // 'CWD .' is the same as PWD command
  {
    sendBegin( "257 \"" );
    sendCat( cwdName );
    sendCatWrite( "\" is your current directory" );
  }
  //
  //  CWD - Change Working Directory
  //
  else if( ! strcmp( command, "CWD" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No directory name" );
    else if( makePath( path ))
      if( fs_exists( path ))
      {
        strcpy( cwdName, path );
        sendWrite( "250 Directory successfully changed." );
      }
      else
        sendWrite( "550 Failed to change directory." );
  }
  //
  //  CDUP - Change to Parent Directory
  //
  else if( ! strcmp( command, "CDUP" ))
  {
    bool ok = false;

    if( strlen( cwdName ) > 1 )  // do nothing if cwdName is root
    {
      // if cwdName ends with '/', remove it (must not append)
      if( cwdName[ strlen( cwdName ) - 1 ] == '/' )
        cwdName[ strlen( cwdName ) - 1 ] = 0;
      // search last '/'
      char * pSep = strrchr( cwdName, '/' );
      ok = pSep > cwdName;
      // if found, ends the string on its position
      if( ok )
      {
        * pSep = 0;
        ok = fs_exists( cwdName );
      }
    }
    // if an error appends, move to root
    if( ! ok )
      strcpy( cwdName, "/" );
    sendBegin( "200 Ok. Current directory is " );
    sendCatWrite( cwdName );
  }

  ///////////////////////////////////////
  //                                   //
  //    TRANSFER PARAMETER COMMANDS    //
  //                                   //
  ///////////////////////////////////////

  //
  //  MODE - Transfer Mode
  //
  else if( ! strcmp( command, "MODE" ))
  {
    if( ! strcmp( parameters, "S" ))
      sendWrite( "200 S Ok" );
    // else if( ! strcmp( parameters, "B" ))
    //  sendWrite( "200 B Ok" );
    else
      sendWrite( "504 Only S(tream) is suported" );
  }
  //
  //  STRU - File Structure
  //
  else if( ! strcmp( command, "STRU" ))
  {
    if( ! strcmp( parameters, "F" ))
      sendWrite( "200 F Ok" );
    else
      sendWrite( "504 Only F(ile) is suported" );
  }
  //
  //  TYPE - Data Type
  //
  else if( ! strcmp( command, "TYPE" ))
  {
    if( ! strcmp( parameters, "A" ))
      sendWrite( "200 TYPE is now ASCII" );
    else if( ! strcmp( parameters, "I" ))
      sendWrite( "200 TYPE is now 8-bit binary" );
    else
      sendWrite( "504 Unknow TYPE" );
  }
  //
  //  PASV - Passive Connection management
  //
  else if( ! strcmp( command, "PASV" ))
  {
    if( listenDataConn())
    {
      dataClose();
      sendBegin( "227 Entering Passive Mode (" );
      sendCat( i2str( ip4_addr1( & ipserver ))); sendCat( "," );
      sendCat( i2str( ip4_addr2( & ipserver ))); sendCat( "," );
      sendCat( i2str( ip4_addr3( & ipserver ))); sendCat( "," );
      sendCat( i2str( ip4_addr4( & ipserver ))); sendCat( "," );
      sendCat( i2str( dataPort >> 8 )); sendCat( "," );
      sendCat( i2str( dataPort & 255 )); sendCatWrite( ")." );
      DEBUG_PRINT( "Data port set to %U\r\n", dataPort );
      dataConnMode = PASSIVE;
    }
    else
    {
      sendWrite( "425 Can't set connection management to passive" );
      dataConnMode = NOTSET;
    }
  }
  //
  //  PORT - Data Port
  //
  else if( ! strcmp( command, "PORT" ))
  {
    uint8_t ip[4];
    uint8_t i;
    dataClose();
    // get IP of data client
    char * p = NULL;
    if( strlen( parameters ) > 0 )
    {
      p = parameters - 1;
      for( i = 0; i < 4 && p != NULL; i ++ )
      {
        ip[ i ] = atoi( ++ p );
        p = strchr( p, ',' );
      }
      // get port of data client
      if( i == 4 && p != NULL )
      {
        dataPort = 256 * atoi( ++ p );
        p = strchr( p, ',' );
        if( p != NULL )
          dataPort += atoi( ++ p );
      }
    }
    if( p == NULL )
    {
      sendWrite( "501 Can't interpret parameters" );
      dataConnMode = NOTSET;
    }
    else
    {
      IP4_ADDR( & ipclient, ip[0], ip[1], ip[2], ip[3] );
      sendWrite( "200 PORT command successful" );
      DEBUG_PRINT( "Data IP set to %u:%u:%u:%u\r\n", ip[0], ip[1], ip[2], ip[3] );
      DEBUG_PRINT( "Data port set to %U\r\n", dataPort );
      dataConnMode = ACTIVE;
    }
  }

  ///////////////////////////////////////
  //                                   //
  //        FTP SERVICE COMMANDS       //
  //                                   //
  ///////////////////////////////////////

  //
  //  LIST and NLST - List
  //
  else if( ! strcmp( command, "LIST" ) || ! strcmp( command, "NLST" ))
  {
    uint16_t nm = 0;
    DIR dir;

    if( ! fs_opendir( & dir, cwdName ))
    {
      sendBegin( "550 Can't open directory " );
      sendCatWrite( cwdName );
    }
    else if( dataConnect())
    {
      sendWrite( "150 Accepted data connection" );
      for( ; ; )
      {
        if( f_readdir( & dir, & finfo ) != FR_OK ||
            finfo.fname[0] == 0 )
          break;
        if( finfo.fname[0] == '.' )
          continue;
        if( ! strcmp( command, "LIST" ))
        {
          if( finfo.fattrib & AM_DIR )
            strcpy( buf, "+/" );
          else
          {
            strcpy( buf, "+r,s" );
            strcat( buf, i2str( finfo.fsize ));
          }
          strcat( buf, ",\t" );
          strcat( buf, lfn[0] == 0 ? finfo.fname : lfn );
        }
        else
          strcpy( buf, lfn[0] == 0 ? finfo.fname : lfn );
        strcat( buf, "\r\n" );
        dataWrite( buf );
        nm ++;
      }
      sendWrite( "226 Directory send OK." );
      dataClose();
    }
  }
  //
  //  MLSD - Listing for Machine Processing (see RFC 3659)
  //
  else if( ! strcmp( command, "MLSD" ))
  {
    DIR dir;
    uint16_t nm = 0;

    if( ! fs_opendir( & dir, cwdName ))
    {
      sendBegin( "550 Can't open directory " );
      sendCatWrite( parameters );
    }
    else if( dataConnect())
    {
      sendWrite( "150 Accepted data connection" );
      for( ; ; )
      {
        if( f_readdir( & dir, & finfo ) != FR_OK ||
            finfo.fname[0] == 0 )
          break;
        if( finfo.fname[0] == '.' )
          continue;
        strcpy( buf, "Type=" );
        strcat( buf, finfo.fattrib & AM_DIR ? "dir" : "file" );
        strcat( buf, ";Size=" );
        strcat( buf, i2str( finfo.fsize ));
        if( finfo.fdate != 0 )
        {
          strcat( buf, ";Modify=" );
          strcat( buf, makeDateTimeStr( finfo.fdate, finfo.ftime ));
        }
        strcat( buf, "; " );
        strcat( buf, lfn[0] == 0 ? finfo.fname : lfn );
        strcat( buf, "\r\n" );
        dataWrite( buf );
        nm ++;
      }
      sendBegin( "226-options: -a -l\r\n" );
      sendCat( "226 " );
      sendCat( i2str( nm ));
      sendCatWrite( " matches total" );
      dataClose();
    }
  }
  //
  //  DELE - Delete a File
  //
  else if( ! strcmp( command, "DELE" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( path ))
    {
      if( ! fs_exists( path ))
      {
        sendBegin( "550 File " );
        sendCat( parameters );
        sendCatWrite( " not found" );
      }
      else
      {
        uint8_t ffs_result = f_unlink( path );
        if( ffs_result == FR_OK )
        {
          sendBegin( "250 Deleted " );
          sendCatWrite( parameters );
        }
        else
        {
          sendBegin( "450 Can't delete " );
          sendCatWrite( parameters );
        }
      }
    }
  }
  //
  //  NOOP
  //
  else if( ! strcmp( command, "NOOP" ))
  {
    sendWrite( "200 Zzz..." );
  }
  //
  //  RETR - Retrieve
  //
  else if( ! strcmp( command, "RETR" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( path ))
    {
      if( ! fs_exists( path ))
      {
        sendBegin( "550 File " );
        sendCat( parameters );
        sendCatWrite( " not found" );
      }
      else if( f_open( & file, path, FA_READ ) != FR_OK )
      {
        sendBegin( "450 Can't open " );
        sendCatWrite( parameters );
      }
      else if( dataConnect())
      {
        uint16_t nb;

        DEBUG_PRINT( "Sending %s\r\n", parameters );
        sendBegin( "150-Connected to port " );
        sendCat( i2str( dataPort ));
        sendCat( "\r\n150 " );
        sendCat( i2str( f_size( & file )));
        sendCatWrite( " bytes to download" );
        timeBeginTrans = chVTGetSystemTimeX();
        bytesTransfered = 0;

        DEBUG_PRINT( "Start transfert\r\n" );
        while( f_read( & file, buf, FTP_BUF_SIZE, (UINT *) & nb ) == FR_OK && nb > 0 )
        {
          netconn_write( dataconn, buf, nb, NETCONN_COPY );
          bytesTransfered += nb;
          DEBUG_PRINT( "Sent %u bytes\r", bytesTransfered );
          fast_blink = TRUE;
        }
        DEBUG_PRINT( "\n" );
        f_close( & file );
        closeTransfer();
        dataClose();
      }
    }
  }
  //
  //  STOR - Store
  //
  else if( ! strcmp( command, "STOR" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( path ))
    {
      if( f_open( & file, path, FA_CREATE_ALWAYS | FA_WRITE ) != FR_OK )
      {
        sendBegin( "451 Can't open/create " );
        sendCatWrite( parameters );
      }
      else if( ! dataConnect())
        f_close( & file );
      else
      {
        struct   pbuf * rcvbuf = NULL;
        void   * prcvbuf;
        uint16_t buflen = 0;
        uint16_t off = 0;
        uint16_t copylen;
        int8_t   ferr = 0;
        UINT     nb;

        DEBUG_PRINT( "Receiving %s\r\n", parameters );
        sendBegin( "150 Connected to port " );
        sendCatWrite( i2str( dataPort ));
        timeBeginTrans = chVTGetSystemTimeX();
        bytesTransfered = 0;
        do
        {
          nerr = netconn_recv_tcp_pbuf( dataconn, & rcvbuf );
          if( nerr != ERR_OK )
            break;
          prcvbuf = rcvbuf->payload;
          buflen = rcvbuf->tot_len;
          while( buflen > 0 )
          {
            if( buflen <= FTP_BUF_SIZE - off )
              copylen = buflen;
            else
              copylen = FTP_BUF_SIZE - off;
            buflen -= copylen;
            memcpy( buf + off, prcvbuf, copylen );
            prcvbuf += copylen;
            off += copylen;
            if( off == FTP_BUF_SIZE )
            {
              if( ferr == 0 )
                ferr = f_write( & file, buf, FTP_BUF_SIZE, (UINT *) & nb );
              off = 0;
            }
            bytesTransfered += copylen;
          }
          pbuf_free( rcvbuf );
          DEBUG_PRINT( "Received %u bytes\r", bytesTransfered );
          fast_blink = TRUE;
        }
        while( true ); // ferr == 0 );
        if( off > 0 && ferr == 0 )
          ferr = f_write( & file, buf, off, (UINT *) & nb );
        f_close( & file );
        DEBUG_PRINT( "\n" );
        if( nerr != ERR_CLSD  )
        {
          sendBegin( "451 Requested action aborted: communication error " );
          sendCatWrite( i2str( abs( nerr )));
        }
        if( ferr != 0  )
        {
          sendBegin( "451 Requested action aborted: file error " );
          sendCatWrite( i2str( abs( ferr )));
        }
        dataClose();
        closeTransfer();
      }
    }
  }
  //
  //  MKD - Make Directory
  //
  else if( ! strcmp( command, "MKD" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No directory name" );
    else if( makePath( path ))
    {
      if( fs_exists( path ))
      {
        sendBegin( "521 \"" );
        sendCat( parameters );
        sendCatWrite( "\" directory already exists" );
      }
      else
      {
        DEBUG_PRINT(  "Creating directory %s\r\n", parameters );
        uint8_t ffs_result = f_mkdir( path );

        RTCDateTime timespec;
        struct tm stm;
        rtcGetTime( & RTCD1, & timespec );
        rtcConvertDateTimeToStructTm( & timespec, & stm, NULL );
        DEBUG_PRINT( "Date/Time: %04u/%02u/%02u %02u:%02u:%02u\r\n",
                     stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
                     stm.tm_hour, stm.tm_min, stm.tm_sec );


        if( ffs_result == FR_OK )
        {
          sendBegin( "257 \"" );
          sendCat( parameters );
          sendCatWrite( "\" created" );
        }
        else
        {
          sendBegin( "550 Can't create \"" );
          sendCat( parameters );
          sendCatWrite( "\"" );
        }
      }
    }
  }
  //
  //  RMD - Remove a Directory
  //
  else if( ! strcmp( command, "RMD" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No directory name" );
    else if( makePath( path ))
    {
      DEBUG_PRINT(  "Deleting %s\r\n", path );
      if( ! fs_exists( path ))
      {
        sendBegin( "550 Directory \"" );
        sendCat( parameters );
        sendCatWrite( "\" not found" );
      }
      else if( f_unlink( path ) == FR_OK)
      {
        sendBegin( "250 \"" );
        sendCat( parameters );
        sendCatWrite( "\" removed" );
      }
      else
      {
        sendBegin( "501 Can't delete \"" );
        sendCat( parameters );
        sendCatWrite( "\"" );
      }
    }
  }
  //
  //  RNFR - Rename From
  //
  else if( ! strcmp( command, "RNFR" ))
  {
    cwdRNFR[ 0 ] = 0;
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( cwdRNFR ))
    {
      if( ! fs_exists( cwdRNFR ))
      {
        sendBegin( "550 File " );
        sendCat( parameters );
        sendCatWrite( " not found" );
      }
      else
      {
        DEBUG_PRINT( "Renaming %s\r\n", cwdRNFR );
        sendWrite( "350 RNFR accepted - file exists, ready for destination" );
      }
    }
  }
  //
  //  RNTO - Rename To
  //
  else if( ! strcmp( command, "RNTO" ))
  {
    char sdir[ FTP_CWD_SIZE ];
    if( strlen( cwdRNFR ) == 0 )
      sendWrite( "503 Need RNFR before RNTO" );
    else if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( path ))
    {
      if( fs_exists( path ))
      {
        sendBegin( "553 " );
        sendCat( parameters );
        sendCatWrite( " already exists" );
      }
      else
      {
        strcpy( sdir, path );
        char * psep = strrchr( sdir, '/' );
        bool fail = psep == NULL;
        if( ! fail )
        {
          if( psep == sdir )
            psep ++;
          * psep = 0;
          fail = ! ( fs_exists( sdir ) &&
                     ( finfo.fattrib & AM_DIR || ! strcmp( sdir, "/")));
          if( fail )
          {
            sendBegin( "550 \"" );
            sendCat( sdir );
            sendCatWrite( "\" is not directory" );
          }
          else
          {
            DEBUG_PRINT(  "Renaming %s to %s\r\n", cwdRNFR, path );
            if( f_rename( cwdRNFR, path ) == FR_OK )
              sendWrite( "250 File successfully renamed or moved" );
            else
              fail = true;
          }
        }
        if( fail )
          sendWrite( "451 Rename/move failure" );
      }
    }
  }
  //
  //  SYST
  //
  /*
  else if( ! strcmp( command, "SYST" ))
  {
    sendWrite( "215 UNIX Type: L8" );
  }
  */

  ///////////////////////////////////////
  //                                   //
  //   EXTENSIONS COMMANDS (RFC 3659)  //
  //                                   //
  ///////////////////////////////////////

  //
  //  FEAT - New Features
  //
  else if( ! strcmp( command, "FEAT" ))
  {
    sendBegin( "211-Extensions supported:\r\n") ;
    sendCat( " MDTM\r\n" );
    sendCat( " MLSD\r\n" );
    sendCat( " SIZE\r\n" );
    sendCat( " SITE FREE\r\n" );
    sendCatWrite( "211 End." );
  }
  //
  //  MDTM - File Modification Time (see RFC 3659)
  //
  else if( ! strcmp( command, "MDTM" ))
  {
    char * fname;
    uint16_t date, time;
    uint8_t gettime;

    gettime = getDateTime( & date, & time );
    fname = parameters + gettime;

    if( strlen( fname ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePathFrom( path, fname ))
    {
      if( ! fs_exists( path ))
      {
        sendBegin( "550 File " );
        sendCat( fname );
        sendCatWrite( " not found" );
      }
      else if( gettime )
      {
        finfo.fdate = date;
        finfo.ftime = time;
        if( f_utime( path, & finfo ) == FR_OK )
          sendWrite( "200 Ok" );
        else
          sendWrite( "550 Unable to modify time" );
      }
      else
      {
        sendBegin( "213 " );
        sendCatWrite( makeDateTimeStr( finfo.fdate, finfo.ftime ));
      }
    }
  }
  //
  //  SIZE - Size of the file
  //
  else if( ! strcmp( command, "SIZE" ))
  {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name" );
    else if( makePath( path ))
    {
      if( ! fs_exists( path ) || finfo.fattrib & AM_DIR )
        sendWrite( "550 No such file" );
      else
      {
        sendBegin( "213 " );
        sendCatWrite( i2str( finfo.fsize ));
        f_close( & file );
      }
    }
  }
  //
  //  SITE - System command
  //
  else if( ! strcmp( command, "SITE" ))
  {
    if( ! strcmp( parameters, "FREE" ))
    {
      FATFS * fs;
      uint32_t free_clust;
      f_getfree( "0:", & free_clust, & fs );
      sendBegin( "211 " );
      sendCat( i2str( free_clust * fs->csize >> 11 ));
      sendCat( " MB free of " );
      sendCat( i2str((fs->n_fatent - 2) * fs->csize >> 11 ));
      sendCatWrite( " MB capacity" );
    }
    else
    {
      sendBegin( "500 Unknow SITE command " );
      sendCatWrite( parameters );
    }
  }
  //
  //  STAT - Status command
  //
  else if( ! strcmp( command, "STAT" ))
  {
    uint8_t i, ncli;
    for( i = 0, ncli = 0; i < FTP_NBR_CLIENTS; i ++ )
      if( ss[ i ].ftpconn != NULL )
        ncli ++;
    sendBegin( "211-FTP server status\r\n" );
    sendCat( " Local time is " );
    sendCat( strLocalTime( str ));
    sendCat( "\r\n " );
    sendCat( i2str( ncli ));
    sendCat( " user(s) currently connected to up to " );
    sendCat( i2str( FTP_NBR_CLIENTS ));
    sendCat( "\r\n You will be disconnected after " );
    sendCat( i2str( FTP_TIME_OUT ));
    sendCat( " minutes of inactivity\r\n" );
    sendCatWrite( "211 End." );
  }
  //
  //  Unknow command
  //
  else
    sendWrite( "500 Unknow command" );
  return TRUE;
}

// =========================================================
//
//                       Ftp server
//
// =========================================================

void FtpServer::service( int8_t n, struct netconn *ctrlcn )
{
  uint16_t dummy;
  struct ip_addr ippeer;
  struct sdlog_stru sdl;
  systime_t systemTimeBeginConnect;
  RTCDateTime rtcBeginTime;

  //  Led blink fast to show activity
  fast_blink = TRUE;

  // variables initialization
  systemTimeBeginConnect = chVTGetSystemTimeX();
  rtcGetTime( & RTCD1, & rtcBeginTime );
  strcpy( cwdName, "/" );  // Set the root directory
  cwdRNFR[ 0 ] = 0;
  num = n;
  ctrlconn = ctrlcn;
  listdataconn = NULL;
  dataconn = NULL;
  dataPort = FTP_DATA_PORT + num;
  cmdStatus = 0;
  dataConnMode = NOTSET;
  finfo.lfname = lfn;
  finfo.lfsize = _MAX_LFN + 1;

  //  Get the local and peer IP
  netconn_addr( ctrlconn, & ipserver, & dummy );
  netconn_peer( ctrlconn, & ippeer, & dummy );

  sendBegin( "220---   Welcome to FTP Server!   ---\r\n" );
  sendCat( "   ---  for ChibiOs & STM32-E407  ---\r\n" );
  sendCat( "   ---   by Jean-Michel Gallego   ---\r\n" );
  sendCat( "220 --   Version " );
  sendCat( FTP_VERSION );
  sendCatWrite( "   --" );

  DEBUG_PRINT( "Client connected!\r\n" );

  //  Wait for user name during 10 seconds
  netconn_set_recvtimeout( ctrlconn, MS2ST( 10 * 1000 ));
  if( readCommand() < 0 )
    goto close;
  if( strcmp( command, "USER" ))
  {
    sendWrite( "500 Syntax error" );
    goto close;
  }
  if( strcmp( parameters, FTP_USER ))
  {
    sendWrite( "530 " );
    goto close;
  }
  sendWrite( "331 OK. Password required" );

  //  Wait for password during 10 seconds
  if( readCommand() < 0 )
    goto close;
  if( strcmp( command, "PASS" ))
  {
    sendWrite( "500 Syntax error" );
    goto close;
  }
  if( strcmp( parameters, FTP_PASS ))
  {
    sendWrite( "530 " );
    goto close;
  }
  sendWrite( "230 OK." );

  //  Wait for user commands
  //  Disconnect if FTP_TIME_OUT minutes of inactivity
  netconn_set_recvtimeout( ctrlconn, MS2ST( FTP_TIME_OUT * 60 * 1000 ));
  while( true )
  {
    int8_t err = readCommand();
    if( err == -4 ) // time out
      goto close;
    if( err < 0 )
      goto close;
    if( ! processCommand( command, parameters ))
      goto bye;
  }

  bye:
  sendWrite( "221 Goodbye" );

  close:
  //  Close the connections
  dataClose();
  if( listdataconn != NULL )
  {
    netconn_close( listdataconn );
    netconn_delete( listdataconn );
  }

  //  Write data to log
  uint32_t timeConnect = (uint32_t) ( chVTGetSystemTimeX() - systemTimeBeginConnect );
  strRTCDateTime( str, & rtcBeginTime );
  strcpy( buf, "Connected at " );
  strcat( buf, str );
  strcat( buf, " for" );
  strSec2hms( str, timeConnect / CH_CFG_ST_FREQUENCY,
              ( timeConnect % CH_CFG_ST_FREQUENCY ) / 10 );
  strcat( buf, str );
  strcat( buf, "\r\n" );
  sdl.line = buf;
  strcpy( str, "/Log/" );
  strcat( str, ipaddr_ntoa( & ippeer ));
  strcat( str, ".log" );
  sdl.file = str;
  sdl.append = false;
  chMsgSend( tsdlog, (msg_t) & sdl );

  DEBUG_PRINT( "Client disconnected\r\n" );
}
