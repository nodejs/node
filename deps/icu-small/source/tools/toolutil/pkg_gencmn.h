// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
 *   Copyright (C) 2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 */

#ifndef __PKG_GENCMN_H__
#define __PKG_GENCMN_H__

#include "unicode/utypes.h"

U_CAPI void U_EXPORT2
createCommonDataFile(const char *destDir, const char *name, const char *entrypointName, const char *type, const char *source, const char *copyRight,
                     const char *dataFile, uint32_t max_size, UBool sourceTOC, UBool verbose, char *gencmnFileName);

#endif
