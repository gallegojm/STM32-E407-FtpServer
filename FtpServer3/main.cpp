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

#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "hal.h"

#include "usbser.h"
#include "console.h"

#include "lwipthread.h"
#include "ff.h"

#include <ftps/ftps.h>
#include <ntpc/ntpc.h>
#include <sdlog/sdlog.h>

//==========================================================================*/
// Green LED blinker thread
//
// Blink faster for 5 seconds when external threads set fast_blink to TRUE
//===========================================================================*/

static THD_WORKING_AREA( waThread1, 128 );
bool fast_blink = TRUE;

static THD_FUNCTION( Thread1, arg )
{
  (void) arg;
  chRegSetThreadName( "blinker" );
  uint8_t n = 10;
  uint8_t m = 0;
  while( true )
  {
    palClearPad( GPIOC, GPIOC_LED );  // led on
    chThdSleepMilliseconds( 100 );
    palSetPad( GPIOC, GPIOC_LED );    // led off
    for( int i = 0; i < n; i ++ )
    {
      chThdSleepMilliseconds( 100 );
      if( fast_blink )
      {
        fast_blink = FALSE;
        n = 1;
        m = 5;
      }
    }
    if( m > 0 )
    {
      m --;
      if( m == 0)
        n = 10;
    }
  }
}

//===========================================================================*/
// FatFs related.                                                            */
//===========================================================================*/

static FATFS SDC_FS;

//===========================================================================*/
// Lwip related.                                                             */
//===========================================================================*/

const uint8_t localMACAddress[6] = { 0xC2, 0xAF, 0x51, 0x03, 0xCF, 0x31 };
static struct lwipthread_opts netOptions = { (uint8_t *) localMACAddress,
                                             0, 0, 0, NET_ADDRESS_DHCP };

/*
const uint8_t localMACAddress[6] = { 0xC2, 0xAF, 0x51, 0x03, 0xCF, 0x30 };
static struct lwipthread_opts netOptions = { (uint8_t *) localMACAddress,
                                             IP4_ADDR_VALUE( 192, 168, 3, 100 ),
                                             IP4_ADDR_VALUE( 255, 255, 255, 0 ),
                                             IP4_ADDR_VALUE( 192, 168, 3, 1 ),
                                             NET_ADDRESS_STATIC };
*/

//===========================================================================*/
// Main code.                                                                */
//===========================================================================*/

int main( void )
{
  // ChibiOS initializations
  halInit();
  chSysInit();
  lwipInit( & netOptions );

  // Switch off wakeup
  rtcSTM32SetPeriodicWakeup( & RTCD1, NULL );

  // Serial-over-USB CDC driver initialization
  sduObjectInit( & SDU2 );
  sduStart( & SDU2, & serusbcfg );

  // Activates the USB driver and then the USB bus pull-up on D+.
  // A delay is inserted in order to not have to disconnect the cable after a reset.
  usbDisconnectBus( serusbcfg.usbp );
  chThdSleepMilliseconds( 1500 );
  usbStart( serusbcfg.usbp, &usbcfg );
  usbConnectBus( serusbcfg.usbp );

  // Mount the SD card
  sdStart( & SD6, NULL );
  sdcStart( & SDCD1, NULL );
  sdcConnect( & SDCD1 );
  f_mount( & SDC_FS, "/", 1 );

  // Creates the blinker thread.
  chThdCreateStatic( waThread1, sizeof( waThread1 ),
                     NORMALPRIO, Thread1, NULL );

  // Creates the Logger thread
  tsdlog = chThdCreateStatic( wa_sd_logger, sizeof( wa_sd_logger ),
                              SDLOG_SERVER_THREAD_PRIORITY, sd_logger, NULL );

  // To avoid lwip initialisation race condition
  chThdSleepSeconds( 5 );

  // Creates the NTP scheduler thread
  chThdCreateStatic( wa_ntp_scheduler, sizeof( wa_ntp_scheduler ),
                     NTP_SCHEDULER_THREAD_PRIORITY, ntp_scheduler, NULL );

  chThdSleepSeconds( 5 );

  // Creates the FTP thread (it changes priority internally).
  chThdCreateStatic( wa_ftp_server, sizeof( wa_ftp_server ),
                     NORMALPRIO + 1, ftp_server, NULL );

  // Normal main() thread activity

  while( true )
  {
    // Detect button pushed
    if( palReadPad( GPIOA, GPIOA_BUTTON_WKUP ) != 0 )
      ; // do nothing for now

    chThdSleepMilliseconds( 1000 );
  }
}
