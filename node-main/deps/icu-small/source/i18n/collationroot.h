// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationroot.h
*
* created on: 2012dec17
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONROOT_H__
#define __COLLATIONROOT_H__

#include "unicode/utypes.h"
#include "unicode/udata.h"

#if !UCONFIG_NO_COLLATION

U_NAMESPACE_BEGIN

struct CollationCacheEntry;
struct CollationData;
struct CollationSettings;
struct CollationTailoring;

/**
 * Collation root provider.
 */
class U_I18N_API CollationRoot {  // purely static
public:
    static const CollationCacheEntry *getRootCacheEntry(UErrorCode &errorCode);
    static const CollationTailoring *getRoot(UErrorCode &errorCode);
    static const CollationData *getData(UErrorCode &errorCode);
    static const CollationSettings *getSettings(UErrorCode &errorCode);
    static void U_EXPORT2 forceLoadFromFile(const char* ucadataPath, UErrorCode &errorCode);

private:
    static void U_CALLCONV load(const char* ucadataPath, UErrorCode &errorCode);
    static UDataMemory* loadFromFile(const char* ucadataPath, UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOT_H__
