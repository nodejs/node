// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File rbutil.h
*
* Modification History:
*
*   Date        Name        Description
*   06/10/99    stephen     Creation.
*******************************************************************************
*/

#ifndef UTIL_H
#define UTIL_H 1

#include "unicode/utypes.h"

U_CDECL_BEGIN

void get_dirname(char *dirname, const char *filename);
void get_basename(char *basename, const char *filename);
int32_t itostr(char * buffer, int32_t i, uint32_t radix, int32_t pad);

U_CDECL_END

#endif /* ! UTIL_H */
