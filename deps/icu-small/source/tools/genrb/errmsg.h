/*
*******************************************************************************
*
*   Copyright (C) 1998-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File error.h
*
* Modification History:
*
*   Date        Name        Description
*   05/28/99    stephen     Creation.
*******************************************************************************
*/

#ifndef ERROR_H
#define ERROR_H 1

#include "unicode/utypes.h"

U_CDECL_BEGIN

extern const char *gCurrentFileName;

U_CFUNC void error(uint32_t linenumber, const char *msg, ...);
U_CFUNC void warning(uint32_t linenumber, const char *msg, ...);

/* Show warnings? */
U_CFUNC void setShowWarning(UBool val);
U_CFUNC UBool getShowWarning(void);

/* strict */
U_CFUNC void setStrict(UBool val);
U_CFUNC UBool isStrict(void);

/* verbosity */
U_CFUNC void setVerbose(UBool val);
U_CFUNC UBool isVerbose(void);

U_CDECL_END

#endif
