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

//  HTTP server wrapper thread macros and structures.

#ifndef _FTPS_H_
#define _FTPS_H_

#define FTP_VERSION              "FTP-2015-04-12"

#define FTP_USER                 "Stm32-E407"
#define FTP_PASS                 "Chibi"

#define FTP_MAX_LFN              255             // max size of long file name
#define FTP_SERV_PORT            21
#define FTP_DATA_PORT            55600           // Data port in passive mode
#define FTP_TIME_OUT             5      // Disconnect client after 5 minutes of inactivity
#define FTP_PARAM_SIZE           FTP_MAX_LFN + 8
#define FTP_CWD_SIZE             FTP_MAX_LFN + 8 // max size of a directory name
#define SD_BUF_SIZE              1024 // 512     // size of file buffer for read/write

#define FTPS_THREAD_STACK_SIZE   1024

#ifndef FTPS_THREAD_PRIORITY
#define FTPS_THREAD_PRIORITY     (LOWPRIO + 2)
#endif

extern WORKING_AREA( wa_ftp_server, FTPS_THREAD_STACK_SIZE );

#ifdef __cplusplus
extern "C" {
#endif
  msg_t ftp_server( void *p );
#ifdef __cplusplus
}
#endif

#endif /* _FTPS_H_ */
