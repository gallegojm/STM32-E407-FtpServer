/*
 * console.h
 *
 *  Created on: 17/03/2015
 *      Author: Jean-Michel
 */

#ifndef FTPSERVER_CONSOLE_H_
#define FTPSERVER_CONSOLE_H_

#include "chprintf.h"

extern SerialUSBDriver SDU1;

// for debugging

#define CONSOLE_PRINT(...)
// #define CONSOLE_PRINT(...) chprintf( (BaseSequentialStream*) &SDU1, __VA_ARGS__ )

#endif /* FTPSERVER_CONSOLE_H_ */
