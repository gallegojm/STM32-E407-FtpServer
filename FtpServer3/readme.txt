*****************************************************************************
**                 FTP Server for STM32-E407 and ChibiOS                   **
**                        by Jean-Michel Gallego                           **
*****************************************************************************

 Runs on an Olimex STM32-E407 board.
 Build with ChibiStudio and chibios 3.0.0p5

 User and password are defined in ftps.h
 
 Number of simultaneous clients is defined by FTP_NBR_CLIENTS
 
 Some definitions in lwipopts.h depend on the number of clients:
    MEMP_NUM_TCP_PCB   must be   >= 2 * FTP_NBR_CLIENTS
    MEMP_NUM_NETBUF    must be   >= FTP_NBR_CLIENTS
    MEMP_NUM_NETCONN   must be   >= 1 + 3 * FTP_NBR_CLIENTS
 
 For example,
in ftps.h :
#define FTP_NBR_CLIENTS          4

in lwipopts.h :
#define MEMP_NUM_TCP_PCB         8
#define MEMP_NUM_NETBUF          4
#define MEMP_NUM_NETCONN         13

 I also modified those definitions:
#define MEM_SIZE                 6400
#define LWIP_DHCP                1       // to enable DHCP
#define TCP_MSS                  1460
#define LWIP_SO_RCVTIMEO         1
 
 In ffconf.h , I modified 2 definitions:
#define _FS_REENTRANT   1       /* 0:Disable or 1:Enable */
#define _FS_TIMEOUT     MS2ST( 2000 )   /* Timeout period in milliseconds */

 Commands implemented: 
   USER, PASS
   CDUP, CWD, QUIT
   MODE, STRU, TYPE
   PASV, PORT
   DELE
   LIST, MLSD, NLST
   NOOP, PWD
   RETR, STOR
   MKD,  RMD
   RNTO, RNFR
   FEAT, SIZE
   SITE FREE

 Tested with those clients:
   on Windows: FTP Rush, Filezilla
   on Ubuntu: gFTP, Filezilla
   on Android: AndFTP
   IP cameras: Panasonic BLC-131, Trendnet TV-IP121W, D-Link DCS-930L

 It is no more necessary (as it was in the previous version) to force
   the client to use the primary connection for data transfers.
 
 For debugging, modify the definition of DEBUG_PRINT and/or COMMAND_PRINT
   in file console.h, connect USB-OTG#2 to the PC and open a terminal
   