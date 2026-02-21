// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationdatabuilder.h
*
* created on: 2012apr01
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONDATABUILDER_H__
#define __COLLATIONDATABUILDER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/uversion.h"
#include "collation.h"
#include "collationdata.h"
#include "collationsettings.h"
#include "normalizer2impl.h"
#include "utrie2.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

struct ConditionalCE32;

class CollationFastLatinBuilder;
class CopyHelper;
class DataBuilderCollationIterator;
class UCharsTrieBuilder;

/**
 * Low-level CollationData builder.
 * Takes (character, CE) pairs and builds them into runtime data structures.
 * Supports characters with context prefixes and contraction suffixes.
 */
class U_I18N_API CollationDataBuilder : public UObject {
public:
    /**
     * Collation element modifier. Interface class for a modifier
     * that changes a tailoring builder's temporary CEs to final CEs.
     * Called for every non-special CE32 and every expansion CE.
     */
    class CEModifier : public UObject {
    public:
        virtual ~CEModifier();
        /** Returns a new CE to replace the non-special input CE32, or else Collation::NO_CE. */
        virtual int64_t modifyCE32(uint32_t ce32) const = 0;
        /** Returns a new CE to replace the input CE, or else Collation::NO_CE. */
        virtual int64_t modifyCE(int64_t ce) const = 0;
    };

    CollationDataBuilder(UBool icu4xMode, UErrorCode &errorCode);

    virtual ~CollationDataBuilder();

    void initForTailoring(const CollationData *b, UErrorCode &errorCode);

    virtual UBool isCompressibleLeadByte(uint32_t b) const;

    inline UBool isCompressiblePrimary(uint32_t p) const {
        return isCompressibleLeadByte(p >> 24);
    }

    /**
     * @return true if this builder has mappings (e.g., add() has been called)
     */
    UBool hasMappings() const { return modified; }

    /**
     * @return true if c has CEs in this builder
     */
    UBool isAssigned(UChar32 c) const;

    /**
     * @return the three-byte primary if c maps to a single such CE and has no context data,
     * otherwise returns 0.
     */
    uint32_t getLongPrimaryIfSingleCE(UChar32 c) const;

    /**
     * @return the single CE for c.
     * Sets an error code if c does not have a single CE.
     */
    int64_t getSingleCE(UChar32 c, UErrorCode &errorCode) const;

    void add(const UnicodeString &prefix, const UnicodeString &s,
             const int64_t ces[], int32_t cesLength,
             UErrorCode &errorCode);

    /**
     * Encodes the ces as either the returned ce32 by itself,
     * or by storing an expansion, with the returned ce32 referring to that.
     *
     * add(p, s, ces, cesLength) = addCE32(p, s, encodeCEs(ces, cesLength))
     */
    virtual uint32_t encodeCEs(const int64_t ces[], int32_t cesLength, UErrorCode &errorCode);
    void addCE32(const UnicodeString &prefix, const UnicodeString &s,
                 uint32_t ce32, UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order,
     * if it is worth doing; otherwise no change is made.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return true if an OFFSET_TAG range was used for start..end
     */
    UBool maybeSetPrimaryRange(UChar32 start, UChar32 end,
                               uint32_t primary, int32_t step,
                               UErrorCode &errorCode);

    /**
     * Sets three-byte-primary CEs for a range of code points in code point order.
     * Sets range values if that is worth doing, or else individual values.
     * None of the code points in the range should have complex mappings so far
     * (expansions/contractions/prefixes).
     * @param start first code point
     * @param end last code point (inclusive)
     * @param primary primary weight for 'start'
     * @param step per-code point primary-weight increment
     * @param errorCode ICU in/out error code
     * @return the next primary after 'end': start primary incremented by ((end-start)+1)*step
     */
    uint32_t setPrimaryRangeAndReturnNext(UChar32 start, UChar32 end,
                                          uint32_t primary, int32_t step,
                                          UErrorCode &errorCode);

    /**
     * Copies all mappings from the src builder, with modifications.
     * This builder here must not be built yet, and should be empty.
     */
    void copyFrom(const CollationDataBuilder &src, const CEModifier &modifier,
                  UErrorCode &errorCode);

    void optimize(const UnicodeSet &set, UErrorCode &errorCode);
    void suppressContractions(const UnicodeSet &set, UErrorCode &errorCode);

    void enableFastLatin() { fastLatinEnabled = true; }
    virtual void build(CollationData &data, UErrorCode &errorCode);

    /**
     * Looks up CEs for s and appends them to the ces array.
     * Does not handle normalization: s should be in FCD form.
     *
     * Does not write completely ignorable CEs.
     * Does not write beyond Collation::MAX_EXPANSION_LENGTH.
     *
     * @return incremented cesLength
     */
    int32_t getCEs(const UnicodeString &s, int64_t ces[], int32_t cesLength);
    int32_t getCEs(const UnicodeString &prefix, const UnicodeString &s,
                   int64_t ces[], int32_t cesLength);

protected:
    friend class CopyHelper;
    friend class DataBuilderCollationIterator;

    uint32_t getCE32FromOffsetCE32(UBool fromBase, UChar32 c, uint32_t ce32) const;

    int32_t addCE(int64_t ce, UErrorCode &errorCode);
    int32_t addCE32(uint32_t ce32, UErrorCode &errorCode);
    int32_t addConditionalCE32(const UnicodeString &context, uint32_t ce32, UErrorCode &errorCode);

    inline ConditionalCE32 *getConditionalCE32(int32_t index) const {
        return static_cast<ConditionalCE32 *>(conditionalCE32s[index]);
    }
    inline ConditionalCE32 *getConditionalCE32ForCE32(uint32_t ce32) const {
        return getConditionalCE32(Collation::indexFromCE32(ce32));
    }

    static uint32_t makeBuilderContextCE32(int32_t index) {
        return Collation::makeCE32FromTagAndIndex(Collation::BUILDER_DATA_TAG, index);
    }
    static inline UBool isBuilderContextCE32(uint32_t ce32) {
        return Collation::hasCE32Tag(ce32, Collation::BUILDER_DATA_TAG);
    }

    static uint32_t encodeOneCEAsCE32(int64_t ce);
    uint32_t encodeOneCE(int64_t ce, UErrorCode &errorCode);
    uint32_t encodeExpansion(const int64_t ces[], int32_t length, UErrorCode &errorCode);
    uint32_t encodeExpansion32(const int32_t newCE32s[], int32_t length, UErrorCode &errorCode);

    uint32_t copyFromBaseCE32(UChar32 c, uint32_t ce32, UBool withContext, UErrorCode &errorCode);
    /**
     * Copies base contractions to a list of ConditionalCE32.
     * Sets cond->next to the index of the first new item
     * and returns the index of the last new item.
     */
    int32_t copyContractionsFromBaseCE32(UnicodeString &context, UChar32 c, uint32_t ce32,
                                         ConditionalCE32 *cond, UErrorCode &errorCode);

    UBool getJamoCE32s(uint32_t jamoCE32s[], UErrorCode &errorCode);
    void setDigitTags(UErrorCode &errorCode);
    void setLeadSurrogates(UErrorCode &errorCode);

    void buildMappings(CollationData &data, UErrorCode &errorCode);

    void clearContexts();
    void buildContexts(UErrorCode &errorCode);
    uint32_t buildContext(ConditionalCE32 *head, UErrorCode &errorCode);
    int32_t addContextTrie(uint32_t defaultCE32, UCharsTrieBuilder &trieBuilder,
                           UErrorCode &errorCode);

    void buildFastLatinTable(CollationData &data, UErrorCode &errorCode);

    int32_t getCEs(const UnicodeString &s, int32_t start, int64_t ces[], int32_t cesLength);

    static UChar32 jamoCpFromIndex(int32_t i) {
        // 0 <= i < CollationData::JAMO_CE32S_LENGTH = 19 + 21 + 27
        if(i < Hangul::JAMO_L_COUNT) { return Hangul::JAMO_L_BASE + i; }
        i -= Hangul::JAMO_L_COUNT;
        if(i < Hangul::JAMO_V_COUNT) { return Hangul::JAMO_V_BASE + i; }
        i -= Hangul::JAMO_V_COUNT;
        // i < 27
        return Hangul::JAMO_T_BASE + 1 + i;
    }

    /** @see Collation::BUILDER_DATA_TAG */
    static const uint32_t IS_BUILDER_JAMO_CE32 = 0x100;

    const Normalizer2Impl &nfcImpl;
    const CollationData *base;
    const CollationSettings *baseSettings;
    UTrie2 *trie;
    UVector32 ce32s;
    UVector64 ce64s;
    UVector conditionalCE32s;  // vector of ConditionalCE32
    // Characters that have context (prefixes or contraction suffixes).
    UnicodeSet contextChars;
    // Serialized UCharsTrie structures for finalized contexts.
    UnicodeString contexts;
private:
    /**
     * The "era" of building intermediate contexts.
     * When the array of cached, temporary contexts overflows, then clearContexts()
     * removes them all and invalidates the builtCE32 that used to point to built tries.
     * See ConditionalCE32::era.
     */
    int32_t contextsEra = 0;
protected:
    UnicodeSet unsafeBackwardSet;
    UBool modified;
    UBool icu4xMode;

    UBool fastLatinEnabled;
    CollationFastLatinBuilder *fastLatinBuilder;

    DataBuilderCollationIterator *collIter;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONDATABUILDER_H__
