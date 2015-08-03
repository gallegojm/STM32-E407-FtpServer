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

#ifndef _NTPC_H_
#define _NTPC_H_

#include "ch.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "stdlib.h"
#include "stdio.h"

#include "console.h"

#define NTP_VERSION                     "2015-07-30"

#define NTP_PORT                        123

/*
#define NTP_SERVER_LIST "200.89.75.198", \
                        "200.186.125.195", \
                        "198.55.111.51", \
                        "206.209.110.2", \
                        "218.93.250.18"
*/

/*
#define NTP_SERVER_LIST "192.168.1.1"
*/

#define NTP_SERVER_LIST "0.south-america.pool.ntp.org", \
                        "1.south-america.pool.ntp.org", \
                        "2.south-america.pool.ntp.org", \
                        "0.north-america.pool.ntp.org", \
                        "venezuela.pool.ntp.org"

// Number of seconds to add to get unix local time
//  ( minus 4 hours and half for Venezuela )
#define NTP_LOCAL_DIFFERERENCE          ( - 60UL * ( 4 * 60 + 30 ))
// #define NTP_LOCAL_DIFFERERENCE          0  // UTC

// Set to 1 to enable Day Saving Time
#define NTP_LOCAL_DST                   0

// Time of the day for synchronization (hour and minutes)
#define NTP_TIME_SYNCHRO                ( 4UL * 60 + 15 ) // 4h 15mn in the night
// #define NTP_TIME_SYNCHRO                ( 13UL * 60 + 50 )

// Delay in minutes between retries in case of failure
#define NTP_DELAY_FAILURE               5

// Number of seconds between January 1900 and January 1970 (Unix time)
#define NTP_SEVENTY_YEARS               2208988800UL

#define NTP_PACKET_SIZE                 48

// #define NTP_SCHEDULER_THREAD_STACK_SIZE 768
#define NTP_SCHEDULER_THREAD_STACK_SIZE 512

#define NTP_SCHEDULER_THREAD_PRIORITY   (LOWPRIO + 2)

// define a structure of parameters for debugging and statistics
struct ntp_stru
{
  ip_addr_t addr;
  time_t    unixLocalTime;
  time_t    unixTime;
  uint32_t  elapsed;
  int32_t   lag;
  uint8_t   fase;
  int8_t    err;
};

extern THD_WORKING_AREA( wa_ntp_scheduler, NTP_SCHEDULER_THREAD_STACK_SIZE );

#ifdef __cplusplus
extern "C" {
#endif
  THD_FUNCTION( ntp_scheduler, p );
  char * strSec2hms( char * str, uint32_t sec, uint32_t msec );
  char * strUTime( char * str, time_t tt );
  char * strRTCDateTime( char * str, RTCDateTime * prtcdt );
  char * strLocalTime( char * str );
#ifdef __cplusplus
}
#endif

#endif // _NTPC_H_
