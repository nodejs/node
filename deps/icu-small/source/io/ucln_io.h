/*
******************************************************************************
*                                                                            *
* Copyright (C) 2001-2011, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  ucln_io.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2006August11
*   created by: George Rhoten
*/

#ifndef __UCLN_IO_H__
#define __UCLN_IO_H__

#include "unicode/utypes.h"
#include "ucln.h"

/*
Please keep the order of enums declared in same order
as the functions are suppose to be called. */
typedef enum ECleanupIOType {
    UCLN_IO_START = -1,
    UCLN_IO_LOCBUND,
    UCLN_IO_PRINTF,
    UCLN_IO_COUNT /* This must be last */
} ECleanupIOType;

/* Main library cleanup registration function. */
/* See common/ucln.h for details on adding a cleanup function. */
U_CFUNC void U_EXPORT2 ucln_io_registerCleanup(ECleanupIOType type,
                                                 cleanupFunc *func);

#endif
