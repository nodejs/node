// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File parse.h
*
* Modification History:
*
*   Date        Name        Description
*   05/26/99    stephen     Creation.
*******************************************************************************
*/

#ifndef PARSE_H
#define PARSE_H 1

#include "unicode/utypes.h"
#include "filestrm.h"
#include "ucbuf.h"

U_CDECL_BEGIN
/* One time parser initialization */
void initParser();

/* Parse a ResourceBundle text file */
struct SRBRoot* parse(UCHARBUF *buf, const char* inputDir, const char* outputDir,
                      const char *filename,
                      UBool makeBinaryCollation, UBool omitCollationRules, UErrorCode *status);

U_CDECL_END

#endif
