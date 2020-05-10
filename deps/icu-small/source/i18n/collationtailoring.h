// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtailoring.h
*
* created on: 2013mar12
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONTAILORING_H__
#define __COLLATIONTAILORING_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collationsettings.h"
#include "uhash.h"
#include "umutex.h"

struct UDataMemory;
struct UResourceBundle;
struct UTrie2;

U_NAMESPACE_BEGIN

struct CollationData;

class UnicodeSet;

/**
 * Collation tailoring data & settings.
 * This is a container of values for a collation tailoring
 * built from rules or deserialized from binary data.
 *
 * It is logically immutable: Do not modify its values.
 * The fields are public for convenience.
 *
 * It is shared, reference-counted, and auto-deleted; see SharedObject.
 */
struct U_I18N_API CollationTailoring : public SharedObject {
    CollationTailoring(const CollationSettings *baseSettings);
    virtual ~CollationTailoring();

    /**
     * Returns TRUE if the constructor could not initialize properly.
     */
    UBool isBogus() { return settings == NULL; }

    UBool ensureOwnedData(UErrorCode &errorCode);

    static void makeBaseVersion(const UVersionInfo ucaVersion, UVersionInfo version);
    void setVersion(const UVersionInfo baseVersion, const UVersionInfo rulesVersion);
    int32_t getUCAVersion() const;

    // data for sorting etc.
    const CollationData *data;  // == base data or ownedData
    const CollationSettings *settings;  // reference-counted
    UnicodeString rules;
    // The locale is bogus when built from rules or constructed from a binary blob.
    // It can then be set by the service registration code which is thread-safe.
    mutable Locale actualLocale;
    // UCA version u.v.w & rules version r.s.t.q:
    // version[0]: builder version (runtime version is mixed in at runtime)
    // version[1]: bits 7..3=u, bits 2..0=v
    // version[2]: bits 7..6=w, bits 5..0=r
    // version[3]= (s<<5)+(s>>3)+t+(q<<4)+(q>>4)
    UVersionInfo version;

    // owned objects
    CollationData *ownedData;
    UObject *builder;
    UDataMemory *memory;
    UResourceBundle *bundle;
    UTrie2 *trie;
    UnicodeSet *unsafeBackwardSet;
    mutable UHashtable *maxExpansions;
    mutable UInitOnce maxExpansionsInitOnce;

private:
    /**
     * No copy constructor: A CollationTailoring cannot be copied.
     * It is immutable, and the data trie cannot be copied either.
     */
    CollationTailoring(const CollationTailoring &other);
};

struct U_I18N_API CollationCacheEntry : public SharedObject {
    CollationCacheEntry(const Locale &loc, const CollationTailoring *t)
            : validLocale(loc), tailoring(t) {
        if(t != NULL) {
            t->addRef();
        }
    }
    ~CollationCacheEntry();

    Locale validLocale;
    const CollationTailoring *tailoring;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONTAILORING_H__
