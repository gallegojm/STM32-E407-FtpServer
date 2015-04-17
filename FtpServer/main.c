/*
 *
 *  FTP Server on STM32-E407 with ChibiOs
 *
 *  Copyright (c) 2014-2015 by Jean-Michel Gallego
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

#include "ch.h"
#include "hal.h"

#include "usbser.h"
#include "console.h"

#include "lwipthread.h"
#include "ff.h"
#include "ftps/ftps.h"

//===========================================================================
// Function to calculate free stack memory of a thread
//===========================================================================

size_t get_thd_free_stack( Thread * wsp, size_t size )
{
  size_t n = 0;
  #if CH_DBG_FILL_THREADS
    uint8_t * startp = (uint8_t *) wsp + sizeof( Thread );
    uint8_t * endp = (uint8_t *) wsp + size;
    while( startp < endp )
      if( * startp ++ == CH_STACK_FILL_VALUE )
        ++ n;
  #endif
  return n;
}

void print_thd_free_stack( Thread * wsp, size_t size )
{
  chprintf( (BaseSequentialStream*) &SDU1,
            "%s free stack memory: %u of %u bytes\r\n",
            wsp->p_name, get_thd_free_stack( wsp, size), size );
}

size_t ftps_fm;

//===========================================================================
// Led related
//===========================================================================

//
//  Green LED blinker thread, times are in milliseconds.
//
//  Blinks each 2 seconds when ftpserver is idle
//  Blinks each 200 ms when ftpserver is active
//

bool_t blinkFast;

static WORKING_AREA( waThread1, 64 );

static msg_t Thread1( void *arg )
{
  (void) arg;
  uint8_t i, n;
  chRegSetThreadName( "blinker" );
  blinkFast = FALSE;
  n = 0;
  while( TRUE )
  {
    // led on for 100 ms
    palClearPad( GPIOC, GPIOC_LED );
    chThdSleepMilliseconds( 100 );
    // led off for 1900 ms or less
    palSetPad( GPIOC, GPIOC_LED );
    for( i = 0; i < 19; i ++ )
    {
      chThdSleepMilliseconds( 100 );
      if( blinkFast > 0 )
      {
        n = 2;
        blinkFast = FALSE;
      }
      if( n > 0 )
      {
        n --;
        break;
      }
    }
  }
  return 0;
}

//===========================================================================
// FatFs related
//===========================================================================

//  FS object.
static FATFS SDC_FS;

/*===========================================================================*/
/* Main code.                                                                */
/*===========================================================================*/

//
//  Application entry point.
//

int main( void )
{
  halInit();
  chSysInit();

  //  Initializes a serial-over-USB CDC driver.
  sduObjectInit( & SDU1 );
  sduStart( & SDU1, &serusbcfg );

  //  Activates the USB driver and then the USB bus pull-up on D+.
  //  Note, a delay is inserted in order to not have to disconnect the cable
  //    after a reset.
  usbDisconnectBus( serusbcfg.usbp );
  chThdSleepMilliseconds( 1500 );
  usbStart( serusbcfg.usbp, &usbcfg );
  usbConnectBus( serusbcfg.usbp );

  //  Creates the blinker thread.
  chThdCreateStatic( waThread1, sizeof( waThread1 ), NORMALPRIO, Thread1, NULL);

  //  Mount the SD card
  sdcStart( &SDCD1, NULL );
  sdcConnect( &SDCD1 );
  f_mount( 0, & SDC_FS );

  //  Creates the LWIP threads (it changes priority internally).
  chThdCreateStatic( wa_lwip_thread, LWIP_THREAD_STACK_SIZE, NORMALPRIO + 2,
                     lwip_thread, NULL );

  //  Creates the FTP Server thread (it changes priority internally).
  chThdCreateStatic( wa_ftp_server, sizeof( wa_ftp_server ), NORMALPRIO + 1,
                     ftp_server, NULL );

  while( TRUE )
  {
    chThdSleepMilliseconds( 1000 );
    //  ftps_fm is global so I can supervise the free stack memory of the
    //  ftp server thread with my ST-Link debugger
    ftps_fm = get_thd_free_stack( (Thread *) wa_ftp_server, sizeof( wa_ftp_server ));
    // print_thd_free_stack( (Thread *) wa_ftp_server, sizeof( wa_ftp_server ));
  }
  return 0;
}
