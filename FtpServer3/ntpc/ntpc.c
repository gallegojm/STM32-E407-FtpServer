/*
 *
 *  NTP Client on STM32-E407 with ChibiOs
 *
 *  Copyright (c) 2015 by Jean-Michel Gallego
 *
 *  Please read file ReadMe.txt for instructions
 *
 *  This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ntpc.h"

#include "string.h"
#include <sdlog/sdlog.h>
#include <util.h>

//  Stack area for the NTP Scheduler thread.
THD_WORKING_AREA( wa_ntp_scheduler, NTP_SCHEDULER_THREAD_STACK_SIZE );

//  Array of server addresses
static char * ntpSrvAddr[] = { NTP_SERVER_LIST, NULL };

//  Variables for debugging and statistics
struct ntp_stru ntps;

// Write to register RTC_CALR to calibrate RTC frequency
//
// cal is the number of cycles added or rested every 32 seconds.
// cal > 0 and < +512 to accelerate the frequency
// cal < 0 and > -511 to slow down the frequency
// Each cycle correspond to a frequency variation of 0.954 ppm
//
// Return false if cal out of limits -511 +512

bool rtcSmoothCalibration( int32_t cal )
{
  syssts_t sts;

  // Check cal not out of bounds
  if( cal > 512 || cal < -511 )
    return false;

  // See DM00031020.pdf :
  //   26.3.11 RTC smooth digital calibration
  if( cal <= 0 )
    cal = abs( cal );
  else
    cal = RTC_CALR_CALP + ( 512 - cal );

  // Entering a reentrant critical zone
  sts = chSysGetStatusAndLockX();

  // Wait for recalibration pending flag reset
  while( ( RTCD1.rtc->ISR & RTC_ISR_RECALPF ) != 0 )
    ;

  // Write to RTC calibration register
  RTCD1.rtc->CALR = cal;

  // Leaving a reentrant critical zone
  chSysRestoreStatusX( sts );

  return true;
}

uint32_t ntpClient( const char * serverAddr )
{
  struct netconn * conn = NULL;
  struct netbuf * buf = NULL;
  char     buffer[ NTP_PACKET_SIZE ];
  uint32_t secsSince1900 = 0;
  char   * pbuf;
  uint16_t buflen;
  uint8_t  mode, stratum, ni;

  ntps.fase = 1;
  ntps.err = 0;

  buf = netbuf_new();
  conn = netconn_new( NETCONN_UDP );
  if( buf == NULL || conn == NULL )
    goto ntpend;

  ntps.fase = 2;
  for( ni = 0; ni < 6; ni ++ )
  {
    ntps.err = netconn_gethostbyname( serverAddr , & ntps.addr );
    if( ntps.err != ERR_VAL )
      break;
    chThdSleepMilliseconds( 500 );
  }
  if( ntps.err != ERR_OK )
    goto ntpend;

  // Initialize message for NTP request
  memset( buffer, 0, NTP_PACKET_SIZE );
  buffer[ 0 ]  = 0b11100011; // LI(clock unsynchronized), Version 4, Mode client
  buffer[ 1 ]  = 0;          // Stratum, or type of clock

  ntps.fase = 3;
  ntps.err = netbuf_ref( buf, buffer, NTP_PACKET_SIZE );
  if( ntps.err != ERR_OK )
    goto ntpend;

  ntps.fase = 4;
  ntps.err = netconn_connect( conn, & ntps.addr, NTP_PORT );
  if( ntps.err != ERR_OK )
    goto ntpend;

  ntps.fase = 5;
  ntps.err = netconn_send( conn, buf );
  if( ntps.err != ERR_OK )
    goto ntpend;

  ntps.fase = 6;
  // netconn_recv create a new buffer so it is necessary to delete the previous buffer
  netbuf_delete( buf );
  // Set receive time out to 3 seconds
  netconn_set_recvtimeout( conn, MS2ST( 3000 ));
  ntps.err = netconn_recv( conn, & buf );
  if( ntps.err != ERR_OK )
    goto ntpend;

  ntps.fase = 7;
  ntps.err = netbuf_data( buf, (void **) & pbuf, & buflen );
  if( ntps.err != ERR_OK )
    goto ntpend;

  // Check length of response
  ntps.fase = 8;
  if( buflen < NTP_PACKET_SIZE )
  {
    ntps.err = buflen;
    goto ntpend;
  }

  // Check Mode (must be Server or Broadcast)
  ntps.fase = 9;
  mode = pbuf[ 0 ] & 0b00000111;
  if( mode != 4 && mode != 5 )
  {
    ntps.err = mode;
    goto ntpend;
  }

  // Check stratum != 0 (kiss-of-death response)
  ntps.fase = 10;
  stratum = pbuf[ 1 ];
  if( stratum == 0 )
    goto ntpend;

  // Combine four high bytes of Transmit Timestamp to get NTP time
  //  (seconds since Jan 1 1900):
  ntps.fase = 0;
  secsSince1900 = pbuf[ 40 ] << 24 | pbuf[ 41 ] << 16 |
                  pbuf[ 42 ] <<  8 | pbuf[ 43 ];

  ntpend:
  if( conn != NULL )
    netconn_delete( conn );
  if( buf != NULL )
    netbuf_delete( buf );
  if( secsSince1900 == 0L )
  {
    DEBUG_PRINT( "NTP error: %i %i\r\n", ntps.fase, ntps.err );
    return 0L;
  }
  return secsSince1900;
}

// Loop on the list of Ntp servers until it got a response
// Update the RTC
// Save some data for statistics
// Return false if no valid response from any Ntp server
//        true in case of success

bool ntpRequest( void )
{
  uint8_t     i = 0;
  uint32_t    secsSince1900 = 0;
  uint32_t    lastUnixTime;
  int32_t     dcal;
  RTCDateTime timespec;
  struct tm   timp;

  // Loop on NTP servers
  while( ntpSrvAddr[ i ] != NULL )
    if(( secsSince1900 = ntpClient( ntpSrvAddr[ i ++ ] )) > 0 )
      break;

  // Exit if no valid response
  if( secsSince1900 == 0 )
    return false;

  // Save current local time
  rtcGetTime( & RTCD1, & timespec );
  rtcConvertDateTimeToStructTm( & timespec, & timp, NULL );
  ntps.unixLocalTime = mktime( & timp );

  // Update local time
  lastUnixTime = ntps.unixTime;
  ntps.unixTime = secsSince1900 - NTP_SEVENTY_YEARS + NTP_LOCAL_DIFFERERENCE;
  rtcConvertStructTmToDateTime( gmtime((time_t *) & ntps.unixTime ), 0, & timespec );
  rtcSetTime( & RTCD1, & timespec );

  // Update RTC calibration
  if( lastUnixTime > 0 )
  {
    ntps.elapsed = ntps.unixTime - lastUnixTime;
    ntps.lag = (int32_t) ntps.unixTime - (int32_t) ntps.unixLocalTime;
    dcal = ( ntps.lag * 1048218 ) / ntps.elapsed;
    ntps.calibration += dcal;
    DEBUG_PRINT( "elapsed %D\r\n", ntps.elapsed );
    DEBUG_PRINT( "lag     %D\r\n", ntps.lag );
    DEBUG_PRINT( "dcal    %D\r\n", dcal );
    DEBUG_PRINT( "calibr. %D\r\n", ntps.calibration );
    rtcSmoothCalibration( ntps.calibration );
  }
  else
  {
    ntps.elapsed = 0;
    ntps.calibration = 0;
  }

  return true;
}

// Create a string from a time in seconds
// String str must be at least 14 characters long
//    or 20 characters long if msec > 0
// Format is xxxh xxmn xxs or xxxh xxmn xxs xxxms

char * strSec2hms( char * str, uint32_t sec, uint32_t msec )
{
  uint32_t min, hour;
  char * pe = str;

  hour = sec / 3600UL;
  sec %= 3600UL;
  min = sec / 60UL;
  sec %= 60UL;

  if( hour > 0 )
  {
    chsnprintf( str, 14, " %Uh", hour );
    pe = strchr( str, 0 );
  }
  if( min > 0 )
  {
    chsnprintf( pe, 14, " %Umn", min );
    pe = strchr( pe, 0 );
  }
  if( sec > 0 || ( min == 0 && hour == 0 ))
  {
    chsnprintf( pe, 14, " %Us", sec );
    pe = strchr( pe, 0 );
  }
  if( msec > 0 || ( sec == 0 && min == 0 && hour == 0 ))
    chsnprintf( pe, 20, " %Ums", msec );

  return str;
}

// Create a string from tm structure
// String str must be at least 20 characters long
//    or 24 characters long if pmsec # NULL
// Format is dd/mm/yyyy hh:mn:ss
//    or dd/mm/yyyy hh:mn:ss.mmm

char * strStructTm( char * str, struct tm * stm, uint32_t * pmsec )
{
  if( pmsec )
    chsnprintf( str, 24, "%02u/%02u/%04u %02u:%02u:%02u.%03u",
                stm->tm_mday, stm->tm_mon + 1, stm->tm_year + 1900,
                stm->tm_hour, stm->tm_min, stm->tm_sec, * pmsec );
  else
    chsnprintf( str, 20, "%02u/%02u/%04u %02u:%02u:%02u",
                stm->tm_mday, stm->tm_mon + 1, stm->tm_year + 1900,
                stm->tm_hour, stm->tm_min, stm->tm_sec );
  return str;
}

// Create a string from time_t
// string str must be at least 20 characters long

char * strUTime( char * str, uint32_t tt )
{
  return strStructTm( str, gmtime((time_t *) & tt ), NULL );
}

// Create a string from local time
// string str must be at least 20 characters long

char * strRTCDateTime( char * str, RTCDateTime * prtcdt )
{
  struct tm stm;
  uint32_t msec;

  rtcConvertDateTimeToStructTm( prtcdt, & stm, & msec );
  return strStructTm( str, & stm, & msec );
}

// Create a string from local time
// string str must be at least 20 characters long

char * strLocalTime( char * str )
{
  RTCDateTime timenow;

  rtcGetTime( & RTCD1, & timenow );
  return strRTCDateTime( str, & timenow );
}

// Print statistic

/*
void printRtcStat( void )
{
  int32_t drift = 0;
  char    str[20];

  DEBUG_PRINT( "\n\rRTC updated from NTP server %s\n\r", ipaddr_ntoa( & ntps.addr ));
//  CONSOLE_PRINT( "Local time:   %s\r", asctime( gmtime( & ntps.unixLocalTime )));
//  CONSOLE_PRINT( "Updated time: %s\r", asctime( gmtime( & ntps.unixTime )));
  DEBUG_PRINT( "Local time:   %s\n\r", strUTime( str, ntps.unixLocalTime ));
  DEBUG_PRINT( "Updated time: %s\n\r", strUTime( str, ntps.unixTime ));
  if( ntps.elapsed > 0 )
  {
    if( ntps.lag != 0 )
      drift = ( ntps.lag * 10000 ) / ntps.elapsed;
    DEBUG_PRINT( "Time since last update: %i s\r\n", ntps.elapsed );
    DEBUG_PRINT( "Time lag:     %D s\r\n", ntps.lag );
    DEBUG_PRINT( "Drift:        %s%D,%02u %%\r\n", ( drift < 0 ? "-" : "" ),
                   abs( drift / 100 ), abs( drift ) % 100 );
  }
}
*/

// =========================================================
//
//                   NTP Scheduler thread
//
// =========================================================

THD_FUNCTION( ntp_scheduler, p )
{
  (void ) p;
  RTCDateTime currentTime;
  uint32_t secondsNextSynchro;
  const uint32_t milisecsRefresh = 60UL * 1000 * NTP_TIME_SYNCHRO;
  struct sdlog_stru sdl;
  char line[80];
  char str[20];

  chRegSetThreadName( "ntp_scheduler" );

  ntps.unixTime = 0;
  ntps.elapsed = 0;
  ntps.calibration = 0;

  // Set RTC to a valid value, in case NTP fails at start up
  currentTime.year  = 2015 - 1980;
  currentTime.month = 1;  // January
  currentTime.day = 1;
  currentTime.millisecond = 0;
  currentTime.dstflag = NTP_LOCAL_DST;
  rtcSetTime( & RTCD1, & currentTime );
  rtcSmoothCalibration( 0 );

  // Initialize structure for passing messages to SD logger
  sdl.file = "/Log/rtc.log";
  sdl.line = line;
  sdl.append = true;

  while( true )
  {
    if( ntpRequest())
    {
      // Write data to log
      strUTime( str, ntps.unixLocalTime );
      strcpy( line, "LocT: " );
      strUTime( str, ntps.unixLocalTime );
      strcat( line, str );
      strcat( line, " SrvT: " );
      strUTime( str, ntps.unixTime );
      strcat( line, str );
      strcat( line, " cal: " );
      strcat( line, int2str( str, (int32_t) ntps.calibration, 12 ));
      strcat( line, " SrvIP: " );
      strcat( line, ipaddr_ntoa( & ntps.addr ));
      strcat( line, "\r\n" );
      chMsgSend( tsdlog, (msg_t) & sdl );

      //printRtcStat();

      // Calculate time to next RTC synchro
      rtcGetTime( & RTCD1, & currentTime );
      if( currentTime.millisecond < milisecsRefresh )
        secondsNextSynchro = ( milisecsRefresh - currentTime.millisecond ) / 1000U;
      else
        secondsNextSynchro = 86400UL - ( currentTime.millisecond - milisecsRefresh ) / 1000U;
      if( secondsNextSynchro < 60U * NTP_DELAY_FAILURE )
        secondsNextSynchro += 24U * 60U * 60U; // next day

      DEBUG_PRINT( "Next update in%s\r\n", strSec2hms( str, secondsNextSynchro, 0 ));

      // Sleep until next synchro
      chThdSleepSeconds( secondsNextSynchro );
    }
    else
      // Wait NTP_DELAY_FAILURE minutes before next attempt
      chThdSleepSeconds( 60UL * NTP_DELAY_FAILURE );
  }
}
