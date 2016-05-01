/*
 * util.h
 *
 *  Created on: 4 août 2015
 *      Author: Jean-Michel
 */

#ifndef UTIL_H_
#define UTIL_H_

// =========================================================
//
//              Some utility functions
//
// =========================================================

// Convert a positive integer to string
//
// Parameters:
//   s: string where the conversion is made (must be large enough)
//   i: integer t convert
//   z: if >= 0, size of string s
//      if < 0, size of returned string
//              (must be <= than size of string s; leading space filled with '0')
//
// Return pointer to string

char * int2strZ( char * s, uint32_t i, int8_t z )
{
  char * psi = s + abs( z );

  * -- psi = 0;
  if( i == 0 )
    * -- psi = '0';
  for( ; i; i /= 10 )
    * -- psi = '0' + i % 10;
  if( z < 0 )
    while( psi > s )
      * -- psi = '0';
  return psi;
}

// Convert an integer to string
//
// Parameters:
//   s: string where the conversion is made (must be large enough)
//   i: integer t convert
//   ls: size of string s
//
// Return pointer to string

char * int2str( char * s, int32_t i, int8_t ls )
{
  if( i >= 0 )
    return int2strZ( s, i, ls );
  char * pstr = int2strZ( s + 1, - i, ls - 1 );
  * -- pstr = '-';
  return pstr;
}

#endif /* UTIL_H_ */
