#  FTP Server for STM32-E407 and ChibiOS
  by Jean-Michel Gallego

 New version FtpServer3 support multiple simultaneous clients
 Runs on an Olimex STM32-E407 board.
 Build with ChibiStudio and chibios 3.0.0p5

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

 Please read file readme.txt for more 
 