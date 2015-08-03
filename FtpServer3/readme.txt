*****************************************************************************
**                 FTP Server for STM32-E407 and ChibiOS                   **
**                        by Jean-Michel Gallego                           **
*****************************************************************************

 Runs on an Olimex STM32-E407 board.
 Build with ChibiStudio and chibios 3.0

 Configuration of the local IP and Mac address is done in main.cpp thanks to 
the patch lwip_bindings.zip uploaded by steved at:
    http://forum.chibios.org/phpbb/download/file.php?id=863

 User and password are defined in ftps.h
 
 Number of simultaneous clients is defined by FTP_NBR_CLIENTS
 
 Some definitions in lwipopts.h depend on the number of clients:
    MEMP_NUM_TCP_PCB   must be   >= 2 * FTP_NBR_CLIENTS
    MEMP_NUM_NETBUF    must be   >= FTP_NBR_CLIENTS (+1 for NTP client)
    MEMP_NUM_NETCONN   must be   >= 1 + 3 * FTP_NBR_CLIENTS (+1 for NTP client)

 For example,
in ftps.h :
#define FTP_NBR_CLIENTS          4

in lwipopts.h :
#define MEMP_NUM_TCP_PCB         8
#define MEMP_NUM_NETBUF          5
#define MEMP_NUM_NETCONN         14

 I also modified those definitions:
#define MEM_SIZE                 6400
#define LWIP_DHCP                1       // to enable DHCP
#define TCP_MSS                  1460
#define LWIP_SO_RCVTIMEO         1

For NTP client to use DNS it also necessary to modify:
#define LWIP_DNS                 1
 
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
   STAT

 Tested with those clients:
   on Windows: FTP Rush, Filezilla
   on Ubuntu: gFTP, Filezilla
   on Android: AndFTP
   IP cameras: Panasonic BLC-131, Trendnet TV-IP121W, D-Link DCS-930L

 It is no more necessary (as it was in the previous version) to force
   the client to use the primary connection for data transfers.

 The server now use the RTC clock to timestamp the uploaded files.
 The clock is synchronized daily with a list of NTP servers.
 This list (NTP_SERVER_LIST) is defined in ntpc.h
 It is also possible to modify:
  - the time difference between UTC and local time (NTP_LOCAL_DIFFERERENCE),
  - the Day Saving Time flag (NTP_LOCAL_DST),
  - the time of the day at which synchronization is done (NTP_TIME_SYNCHRO).
  
 Some information about server status is logged on the SD card.
 This is useful to monitor the behavior of the RTC.
 You must create manually the subdirectory /Log where are stored those files.
 
 For debugging, modify the definition of DEBUG_PRINT and/or COMMAND_PRINT
   in file console.h, connect USB-OTG#2 to the PC and open a terminal.
   