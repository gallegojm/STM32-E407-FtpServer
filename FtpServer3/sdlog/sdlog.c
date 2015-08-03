/*
 *
 *  SD Logger on STM32-E407 with ChibiOs
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

#include "sdlog.h"

#include "string.h"

//  Stack area for the SdLog Server thread.
THD_WORKING_AREA( wa_sd_logger, SDLOG_SERVER_THREAD_STACK_SIZE );

thread_t * tsdlog;

// =========================================================
//
//                   SD Logger thread
//
// =========================================================

THD_FUNCTION( sd_logger, p )
{
  (void) p;
  struct sdlog_stru * plog;
  FIL    file;
  UINT   nb;
  BYTE   mode;

  chRegSetThreadName( "sd_logger" );

  while( true )
  {
    thread_t * tp = chMsgWait();
    plog = (struct sdlog_stru *) chMsgGet( tp );
    DEBUG_PRINT( "Write to file %s\r\n%s\r\n", plog->file, plog->line );

    mode = FA_WRITE | ( plog->append ? FA_OPEN_ALWAYS : FA_CREATE_ALWAYS );
    if( f_open( & file, plog->file, mode ) == FR_OK )
    {
      if( plog->append )
        f_lseek( & file, f_size( & file ));
      f_write( & file, plog->line, strlen( plog->line ), (UINT *) & nb );
      f_close( & file );
    }

    chMsgRelease( tp, MSG_OK );
  }
}
