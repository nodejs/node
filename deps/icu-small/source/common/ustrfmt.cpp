// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "cstring.h"
#include "ustrfmt.h"


/***
 * Fills in a char16_t* string with the radix-based representation of a
 * uint32_t number padded with zeroes to minwidth.  The result
 * will be null terminated if there is room.
 *
 * @param buffer char16_t buffer to receive result
 * @param capacity capacity of buffer
 * @param i the unsigned number to be formatted
 * @param radix the radix from 2..36
 * @param minwidth the minimum width.  If the result is narrower than
 *        this, '0's will be added on the left.  Must be <=
 *        capacity.
 * @return the length of the result, not including any terminating
 *        null
 */
U_CAPI int32_t U_EXPORT2
uprv_itou (char16_t * buffer, int32_t capacity,
           uint32_t i, uint32_t radix, int32_t minwidth)
{
    int32_t length = 0;
    int digit;
    int32_t j;
    char16_t temp;

    do{
        digit = (int)(i % radix);
        buffer[length++]=(char16_t)(digit<=9?(0x0030+digit):(0x0030+digit+7));
        i=i/radix;
    } while(i && length<capacity);

    while (length < minwidth){
        buffer[length++] = (char16_t) 0x0030;/*zero padding */
    }
    /* null terminate the buffer */
    if(length<capacity){
        buffer[length] = (char16_t) 0x0000;
    }

    /* Reverses the string */
    for (j = 0; j < (length / 2); j++){
        temp = buffer[(length-1) - j];
        buffer[(length-1) - j] = buffer[j];
        buffer[j] = temp;
    }
    return length;
}
