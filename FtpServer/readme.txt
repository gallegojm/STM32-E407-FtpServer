*****************************************************************************
**                  FTP Server on STM32-E407 with ChibiOs                  **
*****************************************************************************

 Runs on an Olimex STM32-E407 board.
 Build with ChibiStudio (chibios 2.6.7)

 To enable DHCP I define LWIP_DHCP to 1 and I apply to file lwipthread.c
   the patch proposed by ulikoehler at
   http://forum.chibios.org/phpbb/viewtopic.php?f=14&t=2539&sid=2abe153e08c7ab2526952c9ff5b43296#p20157 

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

 User and password are defined in ftps.h

 Tested with those clients:
   under Windows: FTP Rush, Filezilla
   under Ubuntu: gFTP, Filezilla

 It is necessary to force the client  to use the primary connection
   for data transfers.
 In FTP Rush:
   Go to 'Tools/Site Manager', right-select your site and 'Site Properties'
   In 'General', check "Single connection mode"
 In FileZilla:
   Go to 'File/Site Manager' then select your site.
   In 'Transfer Settings', check "Limit number of simultaneous connections"
     and set the maximum to 1
 In gFTP (valide for all sites):
   Go to 'FTP', 'Préférences...' and 'Général' (french version)
   Select 'Un seul transfer à la fois'
 
 For debugging, uncomment in file console.h the line 18 and connect USB-OTG#2
   to the PC
         