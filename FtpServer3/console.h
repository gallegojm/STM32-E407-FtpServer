/*
 * console.h
 *
 *  Created on: 9 mai 2015
 *      Author: Jean-Michel
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include "chprintf.h"

extern SerialUSBDriver SDU2;

// for printing to console

#define CONSOLE_PRINT(...) chprintf( (BaseSequentialStream*) &SDU2, __VA_ARGS__ )

// to print messages for debugging

#define DEBUG_PRINT(...)
// #define DEBUG_PRINT(...) CONSOLE_PRINT( __VA_ARGS__ )

// to print commands received from clients and response of server

#define COMMAND_PRINT(...)
// #define COMMAND_PRINT(...) CONSOLE_PRINT( __VA_ARGS__ )

#endif // CONSOLE_H_
