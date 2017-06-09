// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  filetools.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009jan09
*   created by: Michael Ow
*
* Contains various functions to handle files.
* Not suitable for production use. Not supported.
* Not conformant. Not efficient.
*/

#ifndef __FILETOOLS_H__
#define __FILETOOLS_H__

#include "unicode/utypes.h"

U_CAPI UBool U_EXPORT2
isFileModTimeLater(const char *filePath, const char *checkAgainst, UBool isDir=FALSE);

U_CAPI void U_EXPORT2
swapFileSepChar(char *filePath, const char oldFileSepChar, const char newFileSepChar);

#endif
