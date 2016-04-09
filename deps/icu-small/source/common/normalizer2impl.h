/*
*******************************************************************************
*
*   Copyright (C) 2009-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  normalizer2impl.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009nov22
*   created by: Markus W. Scherer
*/

#ifndef __NORMALIZER2IMPL_H__
#define __NORMALIZER2IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"
#include "unicode/utf16.h"
#include "mutex.h"
#include "uset_imp.h"
#include "utrie2.h"

U_NAMESPACE_BEGIN

struct CanonIterData;

class U_COMMON_API Hangul {
public:
    /* Korean Hangul and Jamo constants */
    enum {
        JAMO_L_BASE=0x1100,     /* "lead" jamo */
        JAMO_L_END=0x1112,
        JAMO_V_BASE=0x1161,     /* "vowel" jamo */
        JAMO_V_END=0x1175,
        JAMO_T_BASE=0x11a7,     /* "trail" jamo */
        JAMO_T_END=0x11c2,

        HANGUL_BASE=0xac00,
        HANGUL_END=0xd7a3,

        JAMO_L_COUNT=19,
        JAMO_V_COUNT=21,
        JAMO_T_COUNT=28,

        JAMO_VT_COUNT=JAMO_V_COUNT*JAMO_T_COUNT,

        HANGUL_COUNT=JAMO_L_COUNT*JAMO_V_COUNT*JAMO_T_COUNT,
        HANGUL_LIMIT=HANGUL_BASE+HANGUL_COUNT
    };

    static inline UBool isHangul(UChar32 c) {
        return HANGUL_BASE<=c && c<HANGUL_LIMIT;
    }
    static inline UBool
    isHangulWithoutJamoT(UChar c) {
        c-=HANGUL_BASE;
        return c<HANGUL_COUNT && c%JAMO_T_COUNT==0;
    }
    static inline UBool isJamoL(UChar32 c) {
        return (uint32_t)(c-JAMO_L_BASE)<JAMO_L_COUNT;
    }
    static inline UBool isJamoV(UChar32 c) {
        return (uint32_t)(c-JAMO_V_BASE)<JAMO_V_COUNT;
    }

    /**
     * Decomposes c, which must be a Hangul syllable, into buffer
     * and returns the length of the decomposition (2 or 3).
     */
    static inline int32_t decompose(UChar32 c, UChar buffer[3]) {
        c-=HANGUL_BASE;
        UChar32 c2=c%JAMO_T_COUNT;
        c/=JAMO_T_COUNT;
        buffer[0]=(UChar)(JAMO_L_BASE+c/JAMO_V_COUNT);
        buffer[1]=(UChar)(JAMO_V_BASE+c%JAMO_V_COUNT);
        if(c2==0) {
            return 2;
        } else {
            buffer[2]=(UChar)(JAMO_T_BASE+c2);
            return 3;
        }
    }

    /**
     * Decomposes c, which must be a Hangul syllable, into buffer.
     * This is the raw, not recursive, decomposition. Its length is always 2.
     */
    static inline void getRawDecomposition(UChar32 c, UChar buffer[2]) {
        UChar32 orig=c;
        c-=HANGUL_BASE;
        UChar32 c2=c%JAMO_T_COUNT;
        if(c2==0) {
            c/=JAMO_T_COUNT;
            buffer[0]=(UChar)(JAMO_L_BASE+c/JAMO_V_COUNT);
            buffer[1]=(UChar)(JAMO_V_BASE+c%JAMO_V_COUNT);
        } else {
            buffer[0]=orig-c2;  // LV syllable
            buffer[1]=(UChar)(JAMO_T_BASE+c2);
        }
    }
private:
    Hangul();  // no instantiation
};

class Normalizer2Impl;

class U_COMMON_API ReorderingBuffer : public UMemory {
public:
    ReorderingBuffer(const Normalizer2Impl &ni, UnicodeString &dest) :
        impl(ni), str(dest),
        start(NULL), reorderStart(NULL), limit(NULL),
        remainingCapacity(0), lastCC(0) {}
    ~ReorderingBuffer() {
        if(start!=NULL) {
            str.releaseBuffer((int32_t)(limit-start));
        }
    }
    UBool init(int32_t destCapacity, UErrorCode &errorCode);

    UBool isEmpty() const { return start==limit; }
    int32_t length() const { return (int32_t)(limit-start); }
    UChar *getStart() { return start; }
    UChar *getLimit() { return limit; }
    uint8_t getLastCC() const { return lastCC; }

    UBool equals(const UChar *start, const UChar *limit) const;

    // For Hangul composition, replacing the Leading consonant Jamo with the syllable.
    void setLastChar(UChar c) {
        *(limit-1)=c;
    }

    UBool append(UChar32 c, uint8_t cc, UErrorCode &errorCode) {
        return (c<=0xffff) ?
            appendBMP((UChar)c, cc, errorCode) :
            appendSupplementary(c, cc, errorCode);
    }
    // s must be in NFD, otherwise change the implementation.
    UBool append(const UChar *s, int32_t length,
                 uint8_t leadCC, uint8_t trailCC,
                 UErrorCode &errorCode);
    UBool appendBMP(UChar c, uint8_t cc, UErrorCode &errorCode) {
        if(remainingCapacity==0 && !resize(1, errorCode)) {
            return FALSE;
        }
        if(lastCC<=cc || cc==0) {
            *limit++=c;
            lastCC=cc;
            if(cc<=1) {
                reorderStart=limit;
            }
        } else {
            insert(c, cc);
        }
        --remainingCapacity;
        return TRUE;
    }
    UBool appendZeroCC(UChar32 c, UErrorCode &errorCode);
    UBool appendZeroCC(const UChar *s, const UChar *sLimit, UErrorCode &errorCode);
    void remove();
    void removeSuffix(int32_t suffixLength);
    void setReorderingLimit(UChar *newLimit) {
        remainingCapacity+=(int32_t)(limit-newLimit);
        reorderStart=limit=newLimit;
        lastCC=0;
    }
    void copyReorderableSuffixTo(UnicodeString &s) const {
        s.setTo(reorderStart, (int32_t)(limit-reorderStart));
    }
private:
    /*
     * TODO: Revisit whether it makes sense to track reorderStart.
     * It is set to after the last known character with cc<=1,
     * which stops previousCC() before it reads that character and looks up its cc.
     * previousCC() is normally only called from insert().
     * In other words, reorderStart speeds up the insertion of a combining mark
     * into a multi-combining mark sequence where it does not belong at the end.
     * This might not be worth the trouble.
     * On the other hand, it's not a huge amount of trouble.
     *
     * We probably need it for UNORM_SIMPLE_APPEND.
     */

    UBool appendSupplementary(UChar32 c, uint8_t cc, UErrorCode &errorCode);
    void insert(UChar32 c, uint8_t cc);
    static void writeCodePoint(UChar *p, UChar32 c) {
        if(c<=0xffff) {
            *p=(UChar)c;
        } else {
            p[0]=U16_LEAD(c);
            p[1]=U16_TRAIL(c);
        }
    }
    UBool resize(int32_t appendLength, UErrorCode &errorCode);

    const Normalizer2Impl &impl;
    UnicodeString &str;
    UChar *start, *reorderStart, *limit;
    int32_t remainingCapacity;
    uint8_t lastCC;

    // private backward iterator
    void setIterator() { codePointStart=limit; }
    void skipPrevious();  // Requires start<codePointStart.
    uint8_t previousCC();  // Returns 0 if there is no previous character.

    UChar *codePointStart, *codePointLimit;
};

class U_COMMON_API Normalizer2Impl : public UObject {
public:
    Normalizer2Impl() : normTrie(NULL), fCanonIterData(NULL) {
        fCanonIterDataInitOnce.reset();
    }
    virtual ~Normalizer2Impl();

    void init(const int32_t *inIndexes, const UTrie2 *inTrie,
              const uint16_t *inExtraData, const uint8_t *inSmallFCD);

    void addLcccChars(UnicodeSet &set) const;
    void addPropertyStarts(const USetAdder *sa, UErrorCode &errorCode) const;
    void addCanonIterPropertyStarts(const USetAdder *sa, UErrorCode &errorCode) const;

    // low-level properties ------------------------------------------------ ***

    const UTrie2 *getNormTrie() const { return normTrie; }

    UBool ensureCanonIterData(UErrorCode &errorCode) const;

    uint16_t getNorm16(UChar32 c) const { return UTRIE2_GET16(normTrie, c); }

    UNormalizationCheckResult getCompQuickCheck(uint16_t norm16) const {
        if(norm16<minNoNo || MIN_YES_YES_WITH_CC<=norm16) {
            return UNORM_YES;
        } else if(minMaybeYes<=norm16) {
            return UNORM_MAYBE;
        } else {
            return UNORM_NO;
        }
    }
    UBool isAlgorithmicNoNo(uint16_t norm16) const { return limitNoNo<=norm16 && norm16<minMaybeYes; }
    UBool isCompNo(uint16_t norm16) const { return minNoNo<=norm16 && norm16<minMaybeYes; }
    UBool isDecompYes(uint16_t norm16) const { return norm16<minYesNo || minMaybeYes<=norm16; }

    uint8_t getCC(uint16_t norm16) const {
        if(norm16>=MIN_NORMAL_MAYBE_YES) {
            return (uint8_t)norm16;
        }
        if(norm16<minNoNo || limitNoNo<=norm16) {
            return 0;
        }
        return getCCFromNoNo(norm16);
    }
    static uint8_t getCCFromYesOrMaybe(uint16_t norm16) {
        return norm16>=MIN_NORMAL_MAYBE_YES ? (uint8_t)norm16 : 0;
    }

    /**
     * Returns the FCD data for code point c.
     * @param c A Unicode code point.
     * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
     */
    uint16_t getFCD16(UChar32 c) const {
        if(c<0) {
            return 0;
        } else if(c<0x180) {
            return tccc180[c];
        } else if(c<=0xffff) {
            if(!singleLeadMightHaveNonZeroFCD16(c)) { return 0; }
        }
        return getFCD16FromNormData(c);
    }
    /**
     * Returns the FCD data for the next code point (post-increment).
     * Might skip only a lead surrogate rather than the whole surrogate pair if none of
     * the supplementary code points associated with the lead surrogate have non-zero FCD data.
     * @param s A valid pointer into a string. Requires s!=limit.
     * @param limit The end of the string, or NULL.
     * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
     */
    uint16_t nextFCD16(const UChar *&s, const UChar *limit) const {
        UChar32 c=*s++;
        if(c<0x180) {
            return tccc180[c];
        } else if(!singleLeadMightHaveNonZeroFCD16(c)) {
            return 0;
        }
        UChar c2;
        if(U16_IS_LEAD(c) && s!=limit && U16_IS_TRAIL(c2=*s)) {
            c=U16_GET_SUPPLEMENTARY(c, c2);
            ++s;
        }
        return getFCD16FromNormData(c);
    }
    /**
     * Returns the FCD data for the previous code point (pre-decrement).
     * @param start The start of the string.
     * @param s A valid pointer into a string. Requires start<s.
     * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
     */
    uint16_t previousFCD16(const UChar *start, const UChar *&s) const {
        UChar32 c=*--s;
        if(c<0x180) {
            return tccc180[c];
        }
        if(!U16_IS_TRAIL(c)) {
            if(!singleLeadMightHaveNonZeroFCD16(c)) {
                return 0;
            }
        } else {
            UChar c2;
            if(start<s && U16_IS_LEAD(c2=*(s-1))) {
                c=U16_GET_SUPPLEMENTARY(c2, c);
                --s;
            }
        }
        return getFCD16FromNormData(c);
    }

    /** Returns the FCD data for U+0000<=c<U+0180. */
    uint16_t getFCD16FromBelow180(UChar32 c) const { return tccc180[c]; }
    /** Returns TRUE if the single-or-lead code unit c might have non-zero FCD data. */
    UBool singleLeadMightHaveNonZeroFCD16(UChar32 lead) const {
        // 0<=lead<=0xffff
        uint8_t bits=smallFCD[lead>>8];
        if(bits==0) { return false; }
        return (UBool)((bits>>((lead>>5)&7))&1);
    }
    /** Returns the FCD value from the regular normalization data. */
    uint16_t getFCD16FromNormData(UChar32 c) const;

    void makeCanonIterDataFromNorm16(UChar32 start, UChar32 end, uint16_t norm16,
                                     CanonIterData &newData, UErrorCode &errorCode) const;

    /**
     * Gets the decomposition for one code point.
     * @param c code point
     * @param buffer out-only buffer for algorithmic decompositions
     * @param length out-only, takes the length of the decomposition, if any
     * @return pointer to the decomposition, or NULL if none
     */
    const UChar *getDecomposition(UChar32 c, UChar buffer[4], int32_t &length) const;

    /**
     * Gets the raw decomposition for one code point.
     * @param c code point
     * @param buffer out-only buffer for algorithmic decompositions
     * @param length out-only, takes the length of the decomposition, if any
     * @return pointer to the decomposition, or NULL if none
     */
    const UChar *getRawDecomposition(UChar32 c, UChar buffer[30], int32_t &length) const;

    UChar32 composePair(UChar32 a, UChar32 b) const;

    UBool isCanonSegmentStarter(UChar32 c) const;
    UBool getCanonStartSet(UChar32 c, UnicodeSet &set) const;

    enum {
        MIN_CCC_LCCC_CP=0x300
    };

    enum {
        MIN_YES_YES_WITH_CC=0xff01,
        JAMO_VT=0xff00,
        MIN_NORMAL_MAYBE_YES=0xfe00,
        JAMO_L=1,
        MAX_DELTA=0x40
    };

    enum {
        // Byte offsets from the start of the data, after the generic header.
        IX_NORM_TRIE_OFFSET,
        IX_EXTRA_DATA_OFFSET,
        IX_SMALL_FCD_OFFSET,
        IX_RESERVED3_OFFSET,
        IX_RESERVED4_OFFSET,
        IX_RESERVED5_OFFSET,
        IX_RESERVED6_OFFSET,
        IX_TOTAL_SIZE,

        // Code point thresholds for quick check codes.
        IX_MIN_DECOMP_NO_CP,
        IX_MIN_COMP_NO_MAYBE_CP,

        // Norm16 value thresholds for quick check combinations and types of extra data.
        IX_MIN_YES_NO,  // Mappings & compositions in [minYesNo..minYesNoMappingsOnly[.
        IX_MIN_NO_NO,
        IX_LIMIT_NO_NO,
        IX_MIN_MAYBE_YES,

        IX_MIN_YES_NO_MAPPINGS_ONLY,  // Mappings only in [minYesNoMappingsOnly..minNoNo[.

        IX_RESERVED15,
        IX_COUNT
    };

    enum {
        MAPPING_HAS_CCC_LCCC_WORD=0x80,
        MAPPING_HAS_RAW_MAPPING=0x40,
        MAPPING_NO_COMP_BOUNDARY_AFTER=0x20,
        MAPPING_LENGTH_MASK=0x1f
    };

    enum {
        COMP_1_LAST_TUPLE=0x8000,
        COMP_1_TRIPLE=1,
        COMP_1_TRAIL_LIMIT=0x3400,
        COMP_1_TRAIL_MASK=0x7ffe,
        COMP_1_TRAIL_SHIFT=9,  // 10-1 for the "triple" bit
        COMP_2_TRAIL_SHIFT=6,
        COMP_2_TRAIL_MASK=0xffc0
    };

    // higher-level functionality ------------------------------------------ ***

    // NFD without an NFD Normalizer2 instance.
    UnicodeString &decompose(const UnicodeString &src, UnicodeString &dest,
                             UErrorCode &errorCode) const;
    /**
     * Decomposes [src, limit[ and writes the result to dest.
     * limit can be NULL if src is NUL-terminated.
     * destLengthEstimate is the initial dest buffer capacity and can be -1.
     */
    void decompose(const UChar *src, const UChar *limit,
                   UnicodeString &dest, int32_t destLengthEstimate,
                   UErrorCode &errorCode) const;

    const UChar *decompose(const UChar *src, const UChar *limit,
                           ReorderingBuffer *buffer, UErrorCode &errorCode) const;
    void decomposeAndAppend(const UChar *src, const UChar *limit,
                            UBool doDecompose,
                            UnicodeString &safeMiddle,
                            ReorderingBuffer &buffer,
                            UErrorCode &errorCode) const;
    UBool compose(const UChar *src, const UChar *limit,
                  UBool onlyContiguous,
                  UBool doCompose,
                  ReorderingBuffer &buffer,
                  UErrorCode &errorCode) const;
    const UChar *composeQuickCheck(const UChar *src, const UChar *limit,
                                   UBool onlyContiguous,
                                   UNormalizationCheckResult *pQCResult) const;
    void composeAndAppend(const UChar *src, const UChar *limit,
                          UBool doCompose,
                          UBool onlyContiguous,
                          UnicodeString &safeMiddle,
                          ReorderingBuffer &buffer,
                          UErrorCode &errorCode) const;
    const UChar *makeFCD(const UChar *src, const UChar *limit,
                         ReorderingBuffer *buffer, UErrorCode &errorCode) const;
    void makeFCDAndAppend(const UChar *src, const UChar *limit,
                          UBool doMakeFCD,
                          UnicodeString &safeMiddle,
                          ReorderingBuffer &buffer,
                          UErrorCode &errorCode) const;

    UBool hasDecompBoundary(UChar32 c, UBool before) const;
    UBool isDecompInert(UChar32 c) const { return isDecompYesAndZeroCC(getNorm16(c)); }

    UBool hasCompBoundaryBefore(UChar32 c) const {
        return c<minCompNoMaybeCP || hasCompBoundaryBefore(c, getNorm16(c));
    }
    UBool hasCompBoundaryAfter(UChar32 c, UBool onlyContiguous, UBool testInert) const;

    UBool hasFCDBoundaryBefore(UChar32 c) const { return c<MIN_CCC_LCCC_CP || getFCD16(c)<=0xff; }
    UBool hasFCDBoundaryAfter(UChar32 c) const {
        uint16_t fcd16=getFCD16(c);
        return fcd16<=1 || (fcd16&0xff)==0;
    }
    UBool isFCDInert(UChar32 c) const { return getFCD16(c)<=1; }
private:
    UBool isMaybe(uint16_t norm16) const { return minMaybeYes<=norm16 && norm16<=JAMO_VT; }
    UBool isMaybeOrNonZeroCC(uint16_t norm16) const { return norm16>=minMaybeYes; }
    static UBool isInert(uint16_t norm16) { return norm16==0; }
    static UBool isJamoL(uint16_t norm16) { return norm16==1; }
    static UBool isJamoVT(uint16_t norm16) { return norm16==JAMO_VT; }
    UBool isHangul(uint16_t norm16) const { return norm16==minYesNo; }
    UBool isCompYesAndZeroCC(uint16_t norm16) const { return norm16<minNoNo; }
    // UBool isCompYes(uint16_t norm16) const {
    //     return norm16>=MIN_YES_YES_WITH_CC || norm16<minNoNo;
    // }
    // UBool isCompYesOrMaybe(uint16_t norm16) const {
    //     return norm16<minNoNo || minMaybeYes<=norm16;
    // }
    // UBool hasZeroCCFromDecompYes(uint16_t norm16) const {
    //     return norm16<=MIN_NORMAL_MAYBE_YES || norm16==JAMO_VT;
    // }
    UBool isDecompYesAndZeroCC(uint16_t norm16) const {
        return norm16<minYesNo ||
               norm16==JAMO_VT ||
               (minMaybeYes<=norm16 && norm16<=MIN_NORMAL_MAYBE_YES);
    }
    /**
     * A little faster and simpler than isDecompYesAndZeroCC() but does not include
     * the MaybeYes which combine-forward and have ccc=0.
     * (Standard Unicode 5.2 normalization does not have such characters.)
     */
    UBool isMostDecompYesAndZeroCC(uint16_t norm16) const {
        return norm16<minYesNo || norm16==MIN_NORMAL_MAYBE_YES || norm16==JAMO_VT;
    }
    UBool isDecompNoAlgorithmic(uint16_t norm16) const { return norm16>=limitNoNo; }

    // For use with isCompYes().
    // Perhaps the compiler can combine the two tests for MIN_YES_YES_WITH_CC.
    // static uint8_t getCCFromYes(uint16_t norm16) {
    //     return norm16>=MIN_YES_YES_WITH_CC ? (uint8_t)norm16 : 0;
    // }
    uint8_t getCCFromNoNo(uint16_t norm16) const {
        const uint16_t *mapping=getMapping(norm16);
        if(*mapping&MAPPING_HAS_CCC_LCCC_WORD) {
            return (uint8_t)*(mapping-1);
        } else {
            return 0;
        }
    }
    // requires that the [cpStart..cpLimit[ character passes isCompYesAndZeroCC()
    uint8_t getTrailCCFromCompYesAndZeroCC(const UChar *cpStart, const UChar *cpLimit) const;

    // Requires algorithmic-NoNo.
    UChar32 mapAlgorithmic(UChar32 c, uint16_t norm16) const {
        return c+norm16-(minMaybeYes-MAX_DELTA-1);
    }

    // Requires minYesNo<norm16<limitNoNo.
    const uint16_t *getMapping(uint16_t norm16) const { return extraData+norm16; }
    const uint16_t *getCompositionsListForDecompYes(uint16_t norm16) const {
        if(norm16==0 || MIN_NORMAL_MAYBE_YES<=norm16) {
            return NULL;
        } else if(norm16<minMaybeYes) {
            return extraData+norm16;  // for yesYes; if Jamo L: harmless empty list
        } else {
            return maybeYesCompositions+norm16-minMaybeYes;
        }
    }
    const uint16_t *getCompositionsListForComposite(uint16_t norm16) const {
        const uint16_t *list=extraData+norm16;  // composite has both mapping & compositions list
        return list+  // mapping pointer
            1+  // +1 to skip the first unit with the mapping lenth
            (*list&MAPPING_LENGTH_MASK);  // + mapping length
    }
    /**
     * @param c code point must have compositions
     * @return compositions list pointer
     */
    const uint16_t *getCompositionsList(uint16_t norm16) const {
        return isDecompYes(norm16) ?
                getCompositionsListForDecompYes(norm16) :
                getCompositionsListForComposite(norm16);
    }

    const UChar *copyLowPrefixFromNulTerminated(const UChar *src,
                                                UChar32 minNeedDataCP,
                                                ReorderingBuffer *buffer,
                                                UErrorCode &errorCode) const;
    UBool decomposeShort(const UChar *src, const UChar *limit,
                         ReorderingBuffer &buffer, UErrorCode &errorCode) const;
    UBool decompose(UChar32 c, uint16_t norm16,
                    ReorderingBuffer &buffer, UErrorCode &errorCode) const;

    static int32_t combine(const uint16_t *list, UChar32 trail);
    void addComposites(const uint16_t *list, UnicodeSet &set) const;
    void recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
                   UBool onlyContiguous) const;

    UBool hasCompBoundaryBefore(UChar32 c, uint16_t norm16) const;
    const UChar *findPreviousCompBoundary(const UChar *start, const UChar *p) const;
    const UChar *findNextCompBoundary(const UChar *p, const UChar *limit) const;

    const UChar *findPreviousFCDBoundary(const UChar *start, const UChar *p) const;
    const UChar *findNextFCDBoundary(const UChar *p, const UChar *limit) const;

    int32_t getCanonValue(UChar32 c) const;
    const UnicodeSet &getCanonStartSet(int32_t n) const;

    // UVersionInfo dataVersion;

    // Code point thresholds for quick check codes.
    UChar32 minDecompNoCP;
    UChar32 minCompNoMaybeCP;

    // Norm16 value thresholds for quick check combinations and types of extra data.
    uint16_t minYesNo;
    uint16_t minYesNoMappingsOnly;
    uint16_t minNoNo;
    uint16_t limitNoNo;
    uint16_t minMaybeYes;

    const UTrie2 *normTrie;
    const uint16_t *maybeYesCompositions;
    const uint16_t *extraData;  // mappings and/or compositions for yesYes, yesNo & noNo characters
    const uint8_t *smallFCD;  // [0x100] one bit per 32 BMP code points, set if any FCD!=0
    uint8_t tccc180[0x180];  // tccc values for U+0000..U+017F

public:  // CanonIterData is public to allow access from C callback functions.
    UInitOnce       fCanonIterDataInitOnce;
    CanonIterData  *fCanonIterData;
};

// bits in canonIterData
#define CANON_NOT_SEGMENT_STARTER 0x80000000
#define CANON_HAS_COMPOSITIONS 0x40000000
#define CANON_HAS_SET 0x200000
#define CANON_VALUE_MASK 0x1fffff

/**
 * ICU-internal shortcut for quick access to standard Unicode normalization.
 */
class U_COMMON_API Normalizer2Factory {
public:
    static const Normalizer2 *getFCDInstance(UErrorCode &errorCode);
    static const Normalizer2 *getFCCInstance(UErrorCode &errorCode);
    static const Normalizer2 *getNoopInstance(UErrorCode &errorCode);

    static const Normalizer2 *getInstance(UNormalizationMode mode, UErrorCode &errorCode);

    static const Normalizer2Impl *getNFCImpl(UErrorCode &errorCode);
    static const Normalizer2Impl *getNFKCImpl(UErrorCode &errorCode);
    static const Normalizer2Impl *getNFKC_CFImpl(UErrorCode &errorCode);

    // Get the Impl instance of the Normalizer2.
    // Must be used only when it is known that norm2 is a Normalizer2WithImpl instance.
    static const Normalizer2Impl *getImpl(const Normalizer2 *norm2);
private:
    Normalizer2Factory();  // No instantiation.
};

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2
unorm2_swap(const UDataSwapper *ds,
            const void *inData, int32_t length, void *outData,
            UErrorCode *pErrorCode);

/**
 * Get the NF*_QC property for a code point, for u_getIntPropertyValue().
 * @internal
 */
U_CFUNC UNormalizationCheckResult
unorm_getQuickCheck(UChar32 c, UNormalizationMode mode);

/**
 * Gets the 16-bit FCD value (lead & trail CCs) for a code point, for u_getIntPropertyValue().
 * @internal
 */
U_CFUNC uint16_t
unorm_getFCD16(UChar32 c);

/**
 * Format of Normalizer2 .nrm data files.
 * Format version 2.0.
 *
 * Normalizer2 .nrm data files provide data for the Unicode Normalization algorithms.
 * ICU ships with data files for standard Unicode Normalization Forms
 * NFC and NFD (nfc.nrm), NFKC and NFKD (nfkc.nrm) and NFKC_Casefold (nfkc_cf.nrm).
 * Custom (application-specific) data can be built into additional .nrm files
 * with the gennorm2 build tool.
 *
 * Normalizer2.getInstance() causes a .nrm file to be loaded, unless it has been
 * cached already. Internally, Normalizer2Impl.load() reads the .nrm file.
 *
 * A .nrm file begins with a standard ICU data file header
 * (DataHeader, see ucmndata.h and unicode/udata.h).
 * The UDataInfo.dataVersion field usually contains the Unicode version
 * for which the data was generated.
 *
 * After the header, the file contains the following parts.
 * Constants are defined as enum values of the Normalizer2Impl class.
 *
 * Many details of the data structures are described in the design doc
 * which is at http://site.icu-project.org/design/normalization/custom
 *
 * int32_t indexes[indexesLength]; -- indexesLength=indexes[IX_NORM_TRIE_OFFSET]/4;
 *
 *      The first eight indexes are byte offsets in ascending order.
 *      Each byte offset marks the start of the next part in the data file,
 *      and the end of the previous one.
 *      When two consecutive byte offsets are the same, then the corresponding part is empty.
 *      Byte offsets are offsets from after the header,
 *      that is, from the beginning of the indexes[].
 *      Each part starts at an offset with proper alignment for its data.
 *      If necessary, the previous part may include padding bytes to achieve this alignment.
 *
 *      minDecompNoCP=indexes[IX_MIN_DECOMP_NO_CP] is the lowest code point
 *      with a decomposition mapping, that is, with NF*D_QC=No.
 *      minCompNoMaybeCP=indexes[IX_MIN_COMP_NO_MAYBE_CP] is the lowest code point
 *      with NF*C_QC=No (has a one-way mapping) or Maybe (combines backward).
 *
 *      The next five indexes are thresholds of 16-bit trie values for ranges of
 *      values indicating multiple normalization properties.
 *          minYesNo=indexes[IX_MIN_YES_NO];
 *          minNoNo=indexes[IX_MIN_NO_NO];
 *          limitNoNo=indexes[IX_LIMIT_NO_NO];
 *          minMaybeYes=indexes[IX_MIN_MAYBE_YES];
 *          minYesNoMappingsOnly=indexes[IX_MIN_YES_NO_MAPPINGS_ONLY];
 *      See the normTrie description below and the design doc for details.
 *
 * UTrie2 normTrie; -- see utrie2_impl.h and utrie2.h
 *
 *      The trie holds the main normalization data. Each code point is mapped to a 16-bit value.
 *      Rather than using independent bits in the value (which would require more than 16 bits),
 *      information is extracted primarily via range checks.
 *      For example, a 16-bit value norm16 in the range minYesNo<=norm16<minNoNo
 *      means that the character has NF*C_QC=Yes and NF*D_QC=No properties,
 *      which means it has a two-way (round-trip) decomposition mapping.
 *      Values in the range 2<=norm16<limitNoNo are also directly indexes into the extraData
 *      pointing to mappings, compositions lists, or both.
 *      Value norm16==0 means that the character is normalization-inert, that is,
 *      it does not have a mapping, does not participate in composition, has a zero
 *      canonical combining class, and forms a boundary where text before it and after it
 *      can be normalized independently.
 *      For details about how multiple properties are encoded in 16-bit values
 *      see the design doc.
 *      Note that the encoding cannot express all combinations of the properties involved;
 *      it only supports those combinations that are allowed by
 *      the Unicode Normalization algorithms. Details are in the design doc as well.
 *      The gennorm2 tool only builds .nrm files for data that conforms to the limitations.
 *
 *      The trie has a value for each lead surrogate code unit representing the "worst case"
 *      properties of the 1024 supplementary characters whose UTF-16 form starts with
 *      the lead surrogate. If all of the 1024 supplementary characters are normalization-inert,
 *      then their lead surrogate code unit has the trie value 0.
 *      When the lead surrogate unit's value exceeds the quick check minimum during processing,
 *      the properties for the full supplementary code point need to be looked up.
 *
 * uint16_t maybeYesCompositions[MIN_NORMAL_MAYBE_YES-minMaybeYes];
 * uint16_t extraData[];
 *
 *      There is only one byte offset for the end of these two arrays.
 *      The split between them is given by the constant and variable mentioned above.
 *
 *      The maybeYesCompositions array contains compositions lists for characters that
 *      combine both forward (as starters in composition pairs)
 *      and backward (as trailing characters in composition pairs).
 *      Such characters do not occur in Unicode 5.2 but are allowed by
 *      the Unicode Normalization algorithms.
 *      If there are no such characters, then minMaybeYes==MIN_NORMAL_MAYBE_YES
 *      and the maybeYesCompositions array is empty.
 *      If there are such characters, then minMaybeYes is subtracted from their norm16 values
 *      to get the index into this array.
 *
 *      The extraData array contains compositions lists for "YesYes" characters,
 *      followed by mappings and optional compositions lists for "YesNo" characters,
 *      followed by only mappings for "NoNo" characters.
 *      (Referring to pairs of NFC/NFD quick check values.)
 *      The norm16 values of those characters are directly indexes into the extraData array.
 *
 *      The data structures for compositions lists and mappings are described in the design doc.
 *
 * uint8_t smallFCD[0x100]; -- new in format version 2
 *
 *      This is a bit set to help speed up FCD value lookups in the absence of a full
 *      UTrie2 or other large data structure with the full FCD value mapping.
 *
 *      Each smallFCD bit is set if any of the corresponding 32 BMP code points
 *      has a non-zero FCD value (lccc!=0 or tccc!=0).
 *      Bit 0 of smallFCD[0] is for U+0000..U+001F. Bit 7 of smallFCD[0xff] is for U+FFE0..U+FFFF.
 *      A bit for 32 lead surrogates is set if any of the 32k corresponding
 *      _supplementary_ code points has a non-zero FCD value.
 *
 *      This bit set is most useful for the large blocks of CJK characters with FCD=0.
 *
 * Changes from format version 1 to format version 2 ---------------------------
 *
 * - Addition of data for raw (not recursively decomposed) mappings.
 *   + The MAPPING_NO_COMP_BOUNDARY_AFTER bit in the extraData is now also set when
 *     the mapping is to an empty string or when the character combines-forward.
 *     This subsumes the one actual use of the MAPPING_PLUS_COMPOSITION_LIST bit which
 *     is then repurposed for the MAPPING_HAS_RAW_MAPPING bit.
 *   + For details see the design doc.
 * - Addition of indexes[IX_MIN_YES_NO_MAPPINGS_ONLY] and separation of the yesNo extraData into
 *   distinct ranges (combines-forward vs. not)
 *   so that a range check can be used to find out if there is a compositions list.
 *   This is fully equivalent with formatVersion 1's MAPPING_PLUS_COMPOSITION_LIST flag.
 *   It is needed for the new (in ICU 49) composePair(), not for other normalization.
 * - Addition of the smallFCD[] bit set.
 */

#endif  /* !UCONFIG_NO_NORMALIZATION */
#endif  /* __NORMALIZER2IMPL_H__ */
