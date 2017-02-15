// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationsets.h
*
* created on: 2013feb09
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONSETS_H__
#define __COLLATIONSETS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "collation.h"

U_NAMESPACE_BEGIN

struct CollationData;

/**
 * Finds the set of characters and strings that sort differently in the tailoring
 * from the base data.
 *
 * Every mapping in the tailoring needs to be compared to the base,
 * because some mappings are copied for optimization, and
 * all contractions for a character are copied if any contractions for that character
 * are added, modified or removed.
 *
 * It might be simpler to re-parse the rule string, but:
 * - That would require duplicating some of the from-rules builder code.
 * - That would make the runtime code depend on the builder.
 * - That would only work if we have the rule string, and we allow users to
 *   omit the rule string from data files.
 */
class TailoredSet : public UMemory {
public:
    TailoredSet(UnicodeSet *t)
            : data(NULL), baseData(NULL),
              tailored(t),
              suffix(NULL),
              errorCode(U_ZERO_ERROR) {}

    void forData(const CollationData *d, UErrorCode &errorCode);

    /**
     * @return U_SUCCESS(errorCode) in C++, void in Java
     * @internal only public for access by callback
     */
    UBool handleCE32(UChar32 start, UChar32 end, uint32_t ce32);

private:
    void compare(UChar32 c, uint32_t ce32, uint32_t baseCE32);
    void comparePrefixes(UChar32 c, const UChar *p, const UChar *q);
    void compareContractions(UChar32 c, const UChar *p, const UChar *q);

    void addPrefixes(const CollationData *d, UChar32 c, const UChar *p);
    void addPrefix(const CollationData *d, const UnicodeString &pfx, UChar32 c, uint32_t ce32);
    void addContractions(UChar32 c, const UChar *p);
    void addSuffix(UChar32 c, const UnicodeString &sfx);
    void add(UChar32 c);

    /** Prefixes are reversed in the data structure. */
    void setPrefix(const UnicodeString &pfx) {
        unreversedPrefix = pfx;
        unreversedPrefix.reverse();
    }
    void resetPrefix() {
        unreversedPrefix.remove();
    }

    const CollationData *data;
    const CollationData *baseData;
    UnicodeSet *tailored;
    UnicodeString unreversedPrefix;
    const UnicodeString *suffix;
    UErrorCode errorCode;
};

class ContractionsAndExpansions : public UMemory {
public:
    class CESink : public UMemory {
    public:
        virtual ~CESink();
        virtual void handleCE(int64_t ce) = 0;
        virtual void handleExpansion(const int64_t ces[], int32_t length) = 0;
    };

    ContractionsAndExpansions(UnicodeSet *con, UnicodeSet *exp, CESink *s, UBool prefixes)
            : data(NULL),
              contractions(con), expansions(exp),
              sink(s),
              addPrefixes(prefixes),
              checkTailored(0),
              suffix(NULL),
              errorCode(U_ZERO_ERROR) {}

    void forData(const CollationData *d, UErrorCode &errorCode);
    void forCodePoint(const CollationData *d, UChar32 c, UErrorCode &ec);

    // all following: @internal, only public for access by callback

    void handleCE32(UChar32 start, UChar32 end, uint32_t ce32);

    void handlePrefixes(UChar32 start, UChar32 end, uint32_t ce32);
    void handleContractions(UChar32 start, UChar32 end, uint32_t ce32);

    void addExpansions(UChar32 start, UChar32 end);
    void addStrings(UChar32 start, UChar32 end, UnicodeSet *set);

    /** Prefixes are reversed in the data structure. */
    void setPrefix(const UnicodeString &pfx) {
        unreversedPrefix = pfx;
        unreversedPrefix.reverse();
    }
    void resetPrefix() {
        unreversedPrefix.remove();
    }

    const CollationData *data;
    UnicodeSet *contractions;
    UnicodeSet *expansions;
    CESink *sink;
    UBool addPrefixes;
    int8_t checkTailored;  // -1: collected tailored  +1: exclude tailored
    UnicodeSet tailored;
    UnicodeSet ranges;
    UnicodeString unreversedPrefix;
    const UnicodeString *suffix;
    int64_t ces[Collation::MAX_EXPANSION_LENGTH];
    UErrorCode errorCode;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONSETS_H__
