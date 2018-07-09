// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
 *   Copyright (C) 2008-2016, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 */

#ifndef __PKG_ICU_H__
#define __PKG_ICU_H__

#include "unicode/utypes.h"
#include "package.h"

#define U_PKG_RESERVED_CHARS "\"%&'()*+,-./:;<=>?_"

U_CAPI int U_EXPORT2
writePackageDatFile(const char *outFilename, const char *outComment,
                    const char *sourcePath, const char *addList, icu::Package *pkg,
                    char outType);

U_CAPI icu::Package * U_EXPORT2
readList(const char *filesPath, const char *listname, UBool readContents, icu::Package *listPkgIn);

#endif
