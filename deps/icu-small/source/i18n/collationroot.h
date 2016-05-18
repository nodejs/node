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

private:
    static void load(UErrorCode &errorCode);
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOT_H__
