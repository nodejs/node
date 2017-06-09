// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2000, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File writejava.c
*
* Modification History:
*
*   Date        Name        Description
*   01/11/02    Ram        Creation.
*******************************************************************************
*/

#ifndef RLE_H
#define RLE_H 1

#include "unicode/utypes.h"
#include "unicode/ustring.h"

U_CDECL_BEGIN
/**
 * Construct a string representing a byte array.  Use run-length encoding.
 * Two bytes are packed into a single char, with a single extra zero byte at
 * the end if needed.  A byte represents itself, unless it is the
 * ESCAPE_BYTE.  Then the following notations are possible:
 *   ESCAPE_BYTE ESCAPE_BYTE   ESCAPE_BYTE literal
 *   ESCAPE_BYTE n b           n instances of byte b
 * Since an encoded run occupies 3 bytes, we only encode runs of 4 or
 * more bytes.  Thus we have n > 0 and n != ESCAPE_BYTE and n <= 0xFF.
 * If we encounter a run where n == ESCAPE_BYTE, we represent this as:
 *   b ESCAPE_BYTE n-1 b
 * The ESCAPE_BYTE value is chosen so as not to collide with commonly
 * seen values.
 */
int32_t
byteArrayToRLEString(const uint8_t* src,int32_t srcLen, uint16_t* buffer,int32_t bufLen, UErrorCode* status);


/**
 * Construct a string representing a char array.  Use run-length encoding.
 * A character represents itself, unless it is the ESCAPE character.  Then
 * the following notations are possible:
 *   ESCAPE ESCAPE   ESCAPE literal
 *   ESCAPE n c      n instances of character c
 * Since an encoded run occupies 3 characters, we only encode runs of 4 or
 * more characters.  Thus we have n > 0 and n != ESCAPE and n <= 0xFFFF.
 * If we encounter a run where n == ESCAPE, we represent this as:
 *   c ESCAPE n-1 c
 * The ESCAPE value is chosen so as not to collide with commonly
 * seen values.
 */
int32_t
usArrayToRLEString(const uint16_t* src,int32_t srcLen,uint16_t* buffer, int32_t bufLen,UErrorCode* status);

/**
 * Construct an array of bytes from a run-length encoded string.
 */
int32_t
rleStringToByteArray(uint16_t* src, int32_t srcLen, uint8_t* target, int32_t tgtLen, UErrorCode* status);
/**
 * Construct an array of shorts from a run-length encoded string.
 */
int32_t
rleStringToUCharArray(uint16_t* src, int32_t srcLen, uint16_t* target, int32_t tgtLen, UErrorCode* status);

U_CDECL_END

#endif
