// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2015 IBM and others. All rights reserved.
**********************************************************************
*   Date        Name        Description
*  08/13/2001   synwee      Creation.
**********************************************************************
*/
#ifndef USRCHIMP_H
#define USRCHIMP_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/normalizer2.h"
#include "unicode/ucol.h"
#include "unicode/ucoleitr.h"
#include "unicode/ubrk.h"

/* mask off anything but primary order */
#define UCOL_PRIMARYORDERMASK 0xffff0000
/* mask off anything but secondary order */
#define UCOL_SECONDARYORDERMASK 0x0000ff00
/* mask off anything but tertiary order */
#define UCOL_TERTIARYORDERMASK 0x000000ff
/* primary order shift */
#define UCOL_PRIMARYORDERSHIFT 16
/* secondary order shift */
#define UCOL_SECONDARYORDERSHIFT 8

#define UCOL_IGNORABLE 0

/* get weights from a CE */
#define UCOL_PRIMARYORDER(order) (((order) >> 16) & 0xffff)
#define UCOL_SECONDARYORDER(order) (((order) & UCOL_SECONDARYORDERMASK)>> UCOL_SECONDARYORDERSHIFT)
#define UCOL_TERTIARYORDER(order) ((order) & UCOL_TERTIARYORDERMASK)

#define UCOL_CONTINUATION_MARKER 0xC0

#define isContinuation(CE) (((CE) & UCOL_CONTINUATION_MARKER) == UCOL_CONTINUATION_MARKER)

/**
 * This indicates an error has occured during processing or there are no more CEs 
 * to be returned.
 */
#define UCOL_PROCESSED_NULLORDER        ((int64_t)U_INT64_MAX)

U_NAMESPACE_BEGIN

class CollationElementIterator;
class Collator;

struct PCEI
{
    uint64_t ce;
    int32_t  low;
    int32_t  high;
};

struct PCEBuffer
{
    PCEI    defaultBuffer[16];
    PCEI   *buffer;
    int32_t bufferIndex;
    int32_t bufferSize;

    PCEBuffer();
    ~PCEBuffer();

    void  reset();
    UBool isEmpty() const;
    void  put(uint64_t ce, int32_t ixLow, int32_t ixHigh, UErrorCode &errorCode);
    const PCEI *get();
};

class UCollationPCE : public UMemory {
private:
    PCEBuffer          pceBuffer;
    CollationElementIterator *cei;
    UCollationStrength strength;
    UBool              toShift;
    UBool              isShifted;
    uint32_t           variableTop;

public:
    UCollationPCE(UCollationElements *elems);
    UCollationPCE(CollationElementIterator *iter);
    ~UCollationPCE();

    void init(UCollationElements *elems);
    void init(CollationElementIterator *iter);

    /**
     * Get the processed ordering priority of the next collation element in the text.
     * A single character may contain more than one collation element.
     *
     * @param ixLow a pointer to an int32_t to receive the iterator index before fetching the CE.
     * @param ixHigh a pointer to an int32_t to receive the iterator index after fetching the CE.
     * @param status A pointer to an UErrorCode to receive any errors.
     * @return The next collation elements ordering, otherwise returns UCOL_PROCESSED_NULLORDER 
     *         if an error has occured or if the end of string has been reached
     */
    int64_t nextProcessed(int32_t *ixLow, int32_t *ixHigh, UErrorCode *status);
    /**
     * Get the processed ordering priority of the previous collation element in the text.
     * A single character may contain more than one collation element.
     *
     * @param ixLow A pointer to an int32_t to receive the iterator index after fetching the CE
     * @param ixHigh A pointer to an int32_t to receiver the iterator index before fetching the CE
     * @param status A pointer to an UErrorCode to receive any errors. Noteably 
     *               a U_BUFFER_OVERFLOW_ERROR is returned if the internal stack
     *               buffer has been exhausted.
     * @return The previous collation elements ordering, otherwise returns 
     *         UCOL_PROCESSED_NULLORDER if an error has occured or if the start of
     *         string has been reached.
     */
    int64_t previousProcessed(int32_t *ixLow, int32_t *ixHigh, UErrorCode *status);

private:
    void init(const Collator &coll);
    uint64_t processCE(uint32_t ce);
};

U_NAMESPACE_END

#define INITIAL_ARRAY_SIZE_       256
#define MAX_TABLE_SIZE_           257

struct USearch {
    // required since collation element iterator does not have a getText API
    const UChar              *text;
          int32_t             textLength; // exact length
          UBool               isOverlap;
          UBool               isCanonicalMatch;
          int16_t             elementComparisonType;
          UBreakIterator     *internalBreakIter;  //internal character breakiterator
          UBreakIterator     *breakIter;
    // value USEARCH_DONE is the default value
    // if we are not at the start of the text or the end of the text, 
    // depending on the iteration direction and matchedIndex is USEARCH_DONE 
    // it means that we can't find any more matches in that particular direction
          int32_t             matchedIndex; 
          int32_t             matchedLength;
          UBool               isForwardSearching;
          UBool               reset;
};

struct UPattern {
    const UChar              *text;
          int32_t             textLength; // exact length
          // length required for backwards ce comparison
          int32_t             cesLength;
          int32_t            *ces;
          int32_t             cesBuffer[INITIAL_ARRAY_SIZE_];
          int32_t             pcesLength;
          int64_t            *pces;
          int64_t             pcesBuffer[INITIAL_ARRAY_SIZE_];
          UBool               hasPrefixAccents;
          UBool               hasSuffixAccents;
          int16_t             defaultShiftSize;
          int16_t             shift[MAX_TABLE_SIZE_];
          int16_t             backShift[MAX_TABLE_SIZE_];
};

struct UStringSearch {
    struct USearch            *search;
    struct UPattern            pattern;
    const  UCollator          *collator;
    const  icu::Normalizer2   *nfd;
    // positions within the collation element iterator is used to determine
    // if we are at the start of the text.
           UCollationElements *textIter;
           icu::UCollationPCE *textProcessedIter;
    // utility collation element, used throughout program for temporary 
    // iteration.
           UCollationElements *utilIter;
           UBool               ownCollator;
           UCollationStrength  strength;
           uint32_t            ceMask;
           uint32_t            variableTop;
           UBool               toShift;
           UChar               canonicalPrefixAccents[INITIAL_ARRAY_SIZE_];
           UChar               canonicalSuffixAccents[INITIAL_ARRAY_SIZE_];
};

/**
* Exact matches without checking for the ends for extra accents.
* The match after the position within the collation element iterator is to be
* found. 
* After a match is found the offset in the collation element iterator will be
* shifted to the start of the match.
* Implementation note: 
* For tertiary we can't use the collator->tertiaryMask, that is a 
* preprocessed mask that takes into account case options. since we are only 
* concerned with exact matches, we don't need that.
* Alternate handling - since only the 16 most significant digits is only used, 
* we can safely do a compare without masking if the ce is a variable, we mask 
* and get only the primary values no shifting to quartenary is required since 
* all primary values less than variabletop will need to be masked off anyway.
* If the end character is composite and the pattern ce does not match the text 
* ce, we skip it until we find a match in the end composite character or when 
* it has passed the character. This is so that we can match pattern "a" with
* the text "\u00e6" 
* @param strsrch string search data
* @param status error status if any
* @return TRUE if an exact match is found, FALSE otherwise
*/
U_CFUNC
UBool usearch_handleNextExact(UStringSearch *strsrch, UErrorCode *status);

/**
* Canonical matches.
* According to the definition, matches found here will include the whole span 
* of beginning and ending accents if it overlaps that region.
* @param strsrch string search data
* @param status error status if any
* @return TRUE if a canonical match is found, FALSE otherwise
*/
U_CFUNC
UBool usearch_handleNextCanonical(UStringSearch *strsrch, UErrorCode *status);

/**
* Gets the previous match.
* Comments follows from handleNextExact
* @param strsrch string search data
* @param status error status if any
* @return True if a exact math is found, FALSE otherwise.
*/
U_CFUNC
UBool usearch_handlePreviousExact(UStringSearch *strsrch, UErrorCode *status);

/**
* Canonical matches.
* According to the definition, matches found here will include the whole span 
* of beginning and ending accents if it overlaps that region.
* @param strsrch string search data
* @param status error status if any
* @return TRUE if a canonical match is found, FALSE otherwise
*/
U_CFUNC
UBool usearch_handlePreviousCanonical(UStringSearch *strsrch, 
                                      UErrorCode    *status);

#endif /* #if !UCONFIG_NO_COLLATION */

#endif
