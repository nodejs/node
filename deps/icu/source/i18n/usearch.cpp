// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001-2015 IBM and others. All rights reserved.
**********************************************************************
*   Date        Name        Description
*  07/02/2001   synwee      Creation.
**********************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION && !UCONFIG_NO_BREAK_ITERATION

#include "unicode/usearch.h"
#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/utf16.h"
#include "normalizer2impl.h"
#include "usrchimp.h"
#include "cmemory.h"
#include "ucln_in.h"
#include "uassert.h"
#include "ustr_imp.h"

U_NAMESPACE_USE

// don't use Boyer-Moore
// (and if we decide to turn this on again there are several new TODOs that will need to be addressed)
#define BOYER_MOORE 0

// internal definition ---------------------------------------------------

#define LAST_BYTE_MASK_          0xFF
#define SECOND_LAST_BYTE_SHIFT_  8
#define SUPPLEMENTARY_MIN_VALUE_ 0x10000

static const Normalizer2Impl *g_nfcImpl = NULL;

// internal methods -------------------------------------------------

/**
* Fast collation element iterator setOffset.
* This function does not check for bounds.
* @param coleiter collation element iterator
* @param offset to set
*/
static
inline void setColEIterOffset(UCollationElements *elems,
                      int32_t             offset)
{
    // Note: Not "fast" any more after the 2013 collation rewrite.
    // We do not want to expose more internals than necessary.
    UErrorCode status = U_ZERO_ERROR;
    ucol_setOffset(elems, offset, &status);
}

/**
* Getting the mask for collation strength
* @param strength collation strength
* @return collation element mask
*/
static
inline uint32_t getMask(UCollationStrength strength)
{
    switch (strength)
    {
    case UCOL_PRIMARY:
        return UCOL_PRIMARYORDERMASK;
    case UCOL_SECONDARY:
        return UCOL_SECONDARYORDERMASK | UCOL_PRIMARYORDERMASK;
    default:
        return UCOL_TERTIARYORDERMASK | UCOL_SECONDARYORDERMASK |
               UCOL_PRIMARYORDERMASK;
    }
}

/**
* @param ce 32-bit collation element
* @return hash code
*/
static
inline int hashFromCE32(uint32_t ce)
{
    int hc = (int)(
            ((((((ce >> 24) * 37) +
            (ce >> 16)) * 37) +
            (ce >> 8)) * 37) +
            ce);
    hc %= MAX_TABLE_SIZE_;
    if (hc < 0) {
        hc += MAX_TABLE_SIZE_;
    }
    return hc;
}

U_CDECL_BEGIN
static UBool U_CALLCONV
usearch_cleanup(void) {
    g_nfcImpl = NULL;
    return TRUE;
}
U_CDECL_END

/**
* Initializing the fcd tables.
* Internal method, status assumed to be a success.
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
*/
static
inline void initializeFCD(UErrorCode *status)
{
    if (g_nfcImpl == NULL) {
        g_nfcImpl = Normalizer2Factory::getNFCImpl(*status);
        ucln_i18n_registerCleanup(UCLN_I18N_USEARCH, usearch_cleanup);
    }
}

/**
* Gets the fcd value for a character at the argument index.
* This method takes into accounts of the supplementary characters.
* @param str UTF16 string where character for fcd retrieval resides
* @param offset position of the character whose fcd is to be retrieved, to be
*               overwritten with the next character position, taking
*               surrogate characters into consideration.
* @param strlength length of the argument string
* @return fcd value
*/
static
uint16_t getFCD(const UChar   *str, int32_t *offset,
                             int32_t  strlength)
{
    const UChar *temp = str + *offset;
    uint16_t    result = g_nfcImpl->nextFCD16(temp, str + strlength);
    *offset = (int32_t)(temp - str);
    return result;
}

/**
* Getting the modified collation elements taking into account the collation
* attributes
* @param strsrch string search data
* @param sourcece
* @return the modified collation element
*/
static
inline int32_t getCE(const UStringSearch *strsrch, uint32_t sourcece)
{
    // note for tertiary we can't use the collator->tertiaryMask, that
    // is a preprocessed mask that takes into account case options. since
    // we are only concerned with exact matches, we don't need that.
    sourcece &= strsrch->ceMask;

    if (strsrch->toShift) {
        // alternate handling here, since only the 16 most significant digits
        // is only used, we can safely do a compare without masking
        // if the ce is a variable, we mask and get only the primary values
        // no shifting to quartenary is required since all primary values
        // less than variabletop will need to be masked off anyway.
        if (strsrch->variableTop > sourcece) {
            if (strsrch->strength >= UCOL_QUATERNARY) {
                sourcece &= UCOL_PRIMARYORDERMASK;
            }
            else {
                sourcece = UCOL_IGNORABLE;
            }
        }
    } else if (strsrch->strength >= UCOL_QUATERNARY && sourcece == UCOL_IGNORABLE) {
        sourcece = 0xFFFF;
    }

    return sourcece;
}

/**
* Allocate a memory and returns NULL if it failed.
* Internal method, status assumed to be a success.
* @param size to allocate
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return newly allocated array, NULL otherwise
*/
static
inline void * allocateMemory(uint32_t size, UErrorCode *status)
{
    uint32_t *result = (uint32_t *)uprv_malloc(size);
    if (result == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

/**
* Adds a uint32_t value to a destination array.
* Creates a new array if we run out of space. The caller will have to
* manually deallocate the newly allocated array.
* Internal method, status assumed to be success, caller has to check status
* before calling this method. destination not to be NULL and has at least
* size destinationlength.
* @param destination target array
* @param offset destination offset to add value
* @param destinationlength target array size, return value for the new size
* @param value to be added
* @param increments incremental size expected
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return new destination array, destination if there was no new allocation
*/
static
inline int32_t * addTouint32_tArray(int32_t    *destination,
                                    uint32_t    offset,
                                    uint32_t   *destinationlength,
                                    uint32_t    value,
                                    uint32_t    increments,
                                    UErrorCode *status)
{
    uint32_t newlength = *destinationlength;
    if (offset + 1 == newlength) {
        newlength += increments;
        int32_t *temp = (int32_t *)allocateMemory(
                                         sizeof(int32_t) * newlength, status);
        if (U_FAILURE(*status)) {
            return NULL;
        }
        uprv_memcpy(temp, destination, sizeof(int32_t) * (size_t)offset);
        *destinationlength = newlength;
        destination        = temp;
    }
    destination[offset] = value;
    return destination;
}

/**
* Adds a uint64_t value to a destination array.
* Creates a new array if we run out of space. The caller will have to
* manually deallocate the newly allocated array.
* Internal method, status assumed to be success, caller has to check status
* before calling this method. destination not to be NULL and has at least
* size destinationlength.
* @param destination target array
* @param offset destination offset to add value
* @param destinationlength target array size, return value for the new size
* @param value to be added
* @param increments incremental size expected
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return new destination array, destination if there was no new allocation
*/
static
inline int64_t * addTouint64_tArray(int64_t    *destination,
                                    uint32_t    offset,
                                    uint32_t   *destinationlength,
                                    uint64_t    value,
                                    uint32_t    increments,
                                    UErrorCode *status)
{
    uint32_t newlength = *destinationlength;
    if (offset + 1 == newlength) {
        newlength += increments;
        int64_t *temp = (int64_t *)allocateMemory(
                                         sizeof(int64_t) * newlength, status);

        if (U_FAILURE(*status)) {
            return NULL;
        }

        uprv_memcpy(temp, destination, sizeof(int64_t) * (size_t)offset);
        *destinationlength = newlength;
        destination        = temp;
    }

    destination[offset] = value;

    return destination;
}

/**
* Initializing the ce table for a pattern.
* Stores non-ignorable collation keys.
* Table size will be estimated by the size of the pattern text. Table
* expansion will be perform as we go along. Adding 1 to ensure that the table
* size definitely increases.
* Internal method, status assumed to be a success.
* @param strsrch string search data
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return total number of expansions
*/
static
inline uint16_t initializePatternCETable(UStringSearch *strsrch,
                                         UErrorCode    *status)
{
    UPattern *pattern            = &(strsrch->pattern);
    uint32_t  cetablesize        = INITIAL_ARRAY_SIZE_;
    int32_t  *cetable            = pattern->cesBuffer;
    uint32_t  patternlength      = pattern->textLength;
    UCollationElements *coleiter = strsrch->utilIter;

    if (coleiter == NULL) {
        coleiter = ucol_openElements(strsrch->collator, pattern->text,
                                     patternlength, status);
        // status will be checked in ucol_next(..) later and if it is an
        // error UCOL_NULLORDER the result of ucol_next(..) and 0 will be
        // returned.
        strsrch->utilIter = coleiter;
    }
    else {
        ucol_setText(coleiter, pattern->text, pattern->textLength, status);
    }
    if(U_FAILURE(*status)) {
        return 0;
    }

    if (pattern->ces != cetable && pattern->ces) {
        uprv_free(pattern->ces);
    }

    uint16_t  offset      = 0;
    uint16_t  result      = 0;
    int32_t   ce;

    while ((ce = ucol_next(coleiter, status)) != UCOL_NULLORDER &&
           U_SUCCESS(*status)) {
        uint32_t newce = getCE(strsrch, ce);
        if (newce) {
            int32_t *temp = addTouint32_tArray(cetable, offset, &cetablesize,
                                  newce,
                                  patternlength - ucol_getOffset(coleiter) + 1,
                                  status);
            if (U_FAILURE(*status)) {
                return 0;
            }
            offset ++;
            if (cetable != temp && cetable != pattern->cesBuffer) {
                uprv_free(cetable);
            }
            cetable = temp;
        }
        result += (uint16_t)(ucol_getMaxExpansion(coleiter, ce) - 1);
    }

    cetable[offset]   = 0;
    pattern->ces       = cetable;
    pattern->cesLength = offset;

    return result;
}

/**
* Initializing the pce table for a pattern.
* Stores non-ignorable collation keys.
* Table size will be estimated by the size of the pattern text. Table
* expansion will be perform as we go along. Adding 1 to ensure that the table
* size definitely increases.
* Internal method, status assumed to be a success.
* @param strsrch string search data
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return total number of expansions
*/
static
inline uint16_t initializePatternPCETable(UStringSearch *strsrch,
                                          UErrorCode    *status)
{
    UPattern *pattern            = &(strsrch->pattern);
    uint32_t  pcetablesize       = INITIAL_ARRAY_SIZE_;
    int64_t  *pcetable           = pattern->pcesBuffer;
    uint32_t  patternlength      = pattern->textLength;
    UCollationElements *coleiter = strsrch->utilIter;

    if (coleiter == NULL) {
        coleiter = ucol_openElements(strsrch->collator, pattern->text,
                                     patternlength, status);
        // status will be checked in ucol_next(..) later and if it is an
        // error UCOL_NULLORDER the result of ucol_next(..) and 0 will be
        // returned.
        strsrch->utilIter = coleiter;
    } else {
        ucol_setText(coleiter, pattern->text, pattern->textLength, status);
    }
    if(U_FAILURE(*status)) {
        return 0;
    }

    if (pattern->pces != pcetable && pattern->pces != NULL) {
        uprv_free(pattern->pces);
    }

    uint16_t  offset = 0;
    uint16_t  result = 0;
    int64_t   pce;

    icu::UCollationPCE iter(coleiter);

    // ** Should processed CEs be signed or unsigned?
    // ** (the rest of the code in this file seems to play fast-and-loose with
    // **  whether a CE is signed or unsigned. For example, look at routine above this one.)
    while ((pce = iter.nextProcessed(NULL, NULL, status)) != UCOL_PROCESSED_NULLORDER &&
           U_SUCCESS(*status)) {
        int64_t *temp = addTouint64_tArray(pcetable, offset, &pcetablesize,
                              pce,
                              patternlength - ucol_getOffset(coleiter) + 1,
                              status);

        if (U_FAILURE(*status)) {
            return 0;
        }

        offset += 1;

        if (pcetable != temp && pcetable != pattern->pcesBuffer) {
            uprv_free(pcetable);
        }

        pcetable = temp;
        //result += (uint16_t)(ucol_getMaxExpansion(coleiter, ce) - 1);
    }

    pcetable[offset]   = 0;
    pattern->pces       = pcetable;
    pattern->pcesLength = offset;

    return result;
}

/**
* Initializes the pattern struct.
* Internal method, status assumed to be success.
* @param strsrch UStringSearch data storage
* @param status output error if any, caller to check status before calling
*               method, status assumed to be success when passed in.
* @return expansionsize the total expansion size of the pattern
*/
static
inline int16_t initializePattern(UStringSearch *strsrch, UErrorCode *status)
{
    if (U_FAILURE(*status)) { return 0; }
          UPattern   *pattern     = &(strsrch->pattern);
    const UChar      *patterntext = pattern->text;
          int32_t     length      = pattern->textLength;
          int32_t index       = 0;

    // Since the strength is primary, accents are ignored in the pattern.
    if (strsrch->strength == UCOL_PRIMARY) {
        pattern->hasPrefixAccents = 0;
        pattern->hasSuffixAccents = 0;
    } else {
        pattern->hasPrefixAccents = getFCD(patterntext, &index, length) >>
                                                         SECOND_LAST_BYTE_SHIFT_;
        index = length;
        U16_BACK_1(patterntext, 0, index);
        pattern->hasSuffixAccents = getFCD(patterntext, &index, length) &
                                                                 LAST_BYTE_MASK_;
    }

    // ** HACK **
    if (strsrch->pattern.pces != NULL) {
        if (strsrch->pattern.pces != strsrch->pattern.pcesBuffer) {
            uprv_free(strsrch->pattern.pces);
        }

        strsrch->pattern.pces = NULL;
    }

    // since intializePattern is an internal method status is a success.
    return initializePatternCETable(strsrch, status);
}

/**
* Initializing shift tables, with the default values.
* If a corresponding default value is 0, the shift table is not set.
* @param shift table for forwards shift
* @param backshift table for backwards shift
* @param cetable table containing pattern ce
* @param cesize size of the pattern ces
* @param expansionsize total size of the expansions
* @param defaultforward the default forward value
* @param defaultbackward the default backward value
*/
static
inline void setShiftTable(int16_t   shift[], int16_t backshift[],
                          int32_t  *cetable, int32_t cesize,
                          int16_t   expansionsize,
                          int16_t   defaultforward,
                          int16_t   defaultbackward)
{
    // estimate the value to shift. to do that we estimate the smallest
    // number of characters to give the relevant ces, ie approximately
    // the number of ces minus their expansion, since expansions can come
    // from a character.
    int32_t count;
    for (count = 0; count < MAX_TABLE_SIZE_; count ++) {
        shift[count] = defaultforward;
    }
    cesize --; // down to the last index
    for (count = 0; count < cesize; count ++) {
        // number of ces from right of array to the count
        int temp = defaultforward - count - 1;
        shift[hashFromCE32(cetable[count])] = temp > 1 ? temp : 1;
    }
    shift[hashFromCE32(cetable[cesize])] = 1;
    // for ignorables we just shift by one. see test examples.
    shift[hashFromCE32(0)] = 1;

    for (count = 0; count < MAX_TABLE_SIZE_; count ++) {
        backshift[count] = defaultbackward;
    }
    for (count = cesize; count > 0; count --) {
        // the original value count does not seem to work
        backshift[hashFromCE32(cetable[count])] = count > expansionsize ?
                                          (int16_t)(count - expansionsize) : 1;
    }
    backshift[hashFromCE32(cetable[0])] = 1;
    backshift[hashFromCE32(0)] = 1;
}

/**
* Building of the pattern collation element list and the boyer moore strsrch
* table.
* The canonical match will only be performed after the default match fails.
* For both cases we need to remember the size of the composed and decomposed
* versions of the string. Since the Boyer-Moore shift calculations shifts by
* a number of characters in the text and tries to match the pattern from that
* offset, the shift value can not be too large in case we miss some
* characters. To choose a right shift size, we estimate the NFC form of the
* and use its size as a shift guide. The NFC form should be the small
* possible representation of the pattern. Anyways, we'll err on the smaller
* shift size. Hence the calculation for minlength.
* Canonical match will be performed slightly differently. We'll split the
* pattern into 3 parts, the prefix accents (PA), the middle string bounded by
* the first and last base character (MS), the ending accents (EA). Matches
* will be done on MS first, and only when we match MS then some processing
* will be required for the prefix and end accents in order to determine if
* they match PA and EA. Hence the default shift values
* for the canonical match will take the size of either end's accent into
* consideration. Forwards search will take the end accents into consideration
* for the default shift values and the backwards search will take the prefix
* accents into consideration.
* If pattern has no non-ignorable ce, we return a illegal argument error.
* Internal method, status assumed to be success.
* @param strsrch UStringSearch data storage
* @param status  for output errors if it occurs, status is assumed to be a
*                success when it is passed in.
*/
static
inline void initialize(UStringSearch *strsrch, UErrorCode *status)
{
    int16_t expandlength  = initializePattern(strsrch, status);
    if (U_SUCCESS(*status) && strsrch->pattern.cesLength > 0) {
        UPattern *pattern = &strsrch->pattern;
        int32_t   cesize  = pattern->cesLength;

        int16_t minlength = cesize > expandlength
                            ? (int16_t)cesize - expandlength : 1;
        pattern->defaultShiftSize    = minlength;
        setShiftTable(pattern->shift, pattern->backShift, pattern->ces,
                      cesize, expandlength, minlength, minlength);
        return;
    }
    strsrch->pattern.defaultShiftSize = 0;
}

#if BOYER_MOORE
/**
* Check to make sure that the match length is at the end of the character by
* using the breakiterator.
* @param strsrch string search data
* @param start target text start offset
* @param end target text end offset
*/
static
void checkBreakBoundary(const UStringSearch *strsrch, int32_t * /*start*/,
                               int32_t *end)
{
#if !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *breakiterator = strsrch->search->internalBreakIter;
    if (breakiterator) {
        int32_t matchend = *end;
        //int32_t matchstart = *start;

        if (!ubrk_isBoundary(breakiterator, matchend)) {
            *end = ubrk_following(breakiterator, matchend);
        }

        /* Check the start of the matched text to make sure it doesn't have any accents
         * before it.  This code may not be necessary and so it is commented out */
        /*if (!ubrk_isBoundary(breakiterator, matchstart) && !ubrk_isBoundary(breakiterator, matchstart-1)) {
            *start = ubrk_preceding(breakiterator, matchstart);
        }*/
    }
#endif
}

/**
* Determine whether the target text in UStringSearch bounded by the offset
* start and end is one or more whole units of text as
* determined by the breakiterator in UStringSearch.
* @param strsrch string search data
* @param start target text start offset
* @param end target text end offset
*/
static
UBool isBreakUnit(const UStringSearch *strsrch, int32_t start,
                               int32_t    end)
{
#if !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *breakiterator = strsrch->search->breakIter;
    //TODO: Add here.
    if (breakiterator) {
        int32_t startindex = ubrk_first(breakiterator);
        int32_t endindex   = ubrk_last(breakiterator);

        // out-of-range indexes are never boundary positions
        if (start < startindex || start > endindex ||
            end < startindex || end > endindex) {
            return FALSE;
        }
        // otherwise, we can use following() on the position before the
        // specified one and return true of the position we get back is the
        // one the user specified
        UBool result = (start == startindex ||
                ubrk_following(breakiterator, start - 1) == start) &&
               (end == endindex ||
                ubrk_following(breakiterator, end - 1) == end);
        if (result) {
            // iterates the individual ces
                  UCollationElements *coleiter  = strsrch->utilIter;
            const UChar              *text      = strsrch->search->text +
                                                                      start;
                  UErrorCode          status    = U_ZERO_ERROR;
            ucol_setText(coleiter, text, end - start, &status);
            for (int32_t count = 0; count < strsrch->pattern.cesLength;
                 count ++) {
                int32_t ce = getCE(strsrch, ucol_next(coleiter, &status));
                if (ce == UCOL_IGNORABLE) {
                    count --;
                    continue;
                }
                if (U_FAILURE(status) || ce != strsrch->pattern.ces[count]) {
                    return FALSE;
                }
            }
            int32_t nextce = ucol_next(coleiter, &status);
            while (ucol_getOffset(coleiter) == (end - start)
                   && getCE(strsrch, nextce) == UCOL_IGNORABLE) {
                nextce = ucol_next(coleiter, &status);
            }
            if (ucol_getOffset(coleiter) == (end - start)
                && nextce != UCOL_NULLORDER) {
                // extra collation elements at the end of the match
                return FALSE;
            }
        }
        return result;
    }
#endif
    return TRUE;
}

/**
* Getting the next base character offset if current offset is an accent,
* or the current offset if the current character contains a base character.
* accents the following base character will be returned
* @param text string
* @param textoffset current offset
* @param textlength length of text string
* @return the next base character or the current offset
*         if the current character is contains a base character.
*/
static
inline int32_t getNextBaseOffset(const UChar       *text,
                                           int32_t  textoffset,
                                           int32_t      textlength)
{
    if (textoffset < textlength) {
        int32_t temp = textoffset;
        if (getFCD(text, &temp, textlength) >> SECOND_LAST_BYTE_SHIFT_) {
            while (temp < textlength) {
                int32_t result = temp;
                if ((getFCD(text, &temp, textlength) >>
                     SECOND_LAST_BYTE_SHIFT_) == 0) {
                    return result;
                }
            }
            return textlength;
        }
    }
    return textoffset;
}

/**
* Gets the next base character offset depending on the string search pattern
* data
* @param strsrch string search data
* @param textoffset current offset, one offset away from the last character
*                   to search for.
* @return start index of the next base character or the current offset
*         if the current character is contains a base character.
*/
static
inline int32_t getNextUStringSearchBaseOffset(UStringSearch *strsrch,
                                                  int32_t    textoffset)
{
    int32_t textlength = strsrch->search->textLength;
    if (strsrch->pattern.hasSuffixAccents &&
        textoffset < textlength) {
              int32_t  temp       = textoffset;
        const UChar       *text       = strsrch->search->text;
        U16_BACK_1(text, 0, temp);
        if (getFCD(text, &temp, textlength) & LAST_BYTE_MASK_) {
            return getNextBaseOffset(text, textoffset, textlength);
        }
    }
    return textoffset;
}

/**
* Shifting the collation element iterator position forward to prepare for
* a following match. If the last character is a unsafe character, we'll only
* shift by 1 to capture contractions, normalization etc.
* Internal method, status assumed to be success.
* @param text strsrch string search data
* @param textoffset start text position to do search
* @param ce the text ce which failed the match.
* @param patternceindex index of the ce within the pattern ce buffer which
*        failed the match
* @return final offset
*/
static
inline int32_t shiftForward(UStringSearch *strsrch,
                                int32_t    textoffset,
                                int32_t       ce,
                                int32_t        patternceindex)
{
    UPattern *pattern = &(strsrch->pattern);
    if (ce != UCOL_NULLORDER) {
        int32_t shift = pattern->shift[hashFromCE32(ce)];
        // this is to adjust for characters in the middle of the
        // substring for matching that failed.
        int32_t adjust = pattern->cesLength - patternceindex;
        if (adjust > 1 && shift >= adjust) {
            shift -= adjust - 1;
        }
        textoffset += shift;
    }
    else {
        textoffset += pattern->defaultShiftSize;
    }

    textoffset = getNextUStringSearchBaseOffset(strsrch, textoffset);
    // check for unsafe characters
    // * if it is the start or middle of a contraction: to be done after
    //   a initial match is found
    // * thai or lao base consonant character: similar to contraction
    // * high surrogate character: similar to contraction
    // * next character is a accent: shift to the next base character
    return textoffset;
}
#endif // #if BOYER_MOORE

/**
* sets match not found
* @param strsrch string search data
*/
static
inline void setMatchNotFound(UStringSearch *strsrch)
{
    // this method resets the match result regardless of the error status.
    strsrch->search->matchedIndex = USEARCH_DONE;
    strsrch->search->matchedLength = 0;
    if (strsrch->search->isForwardSearching) {
        setColEIterOffset(strsrch->textIter, strsrch->search->textLength);
    }
    else {
        setColEIterOffset(strsrch->textIter, 0);
    }
}

#if BOYER_MOORE
/**
* Gets the offset to the next safe point in text.
* ie. not the middle of a contraction, swappable characters or supplementary
* characters.
* @param collator collation sata
* @param text string to work with
* @param textoffset offset in string
* @param textlength length of text string
* @return offset to the next safe character
*/
static
inline int32_t getNextSafeOffset(const UCollator   *collator,
                                     const UChar       *text,
                                           int32_t  textoffset,
                                           int32_t      textlength)
{
    int32_t result = textoffset; // first contraction character
    while (result != textlength && ucol_unsafeCP(text[result], collator)) {
        result ++;
    }
    return result;
}

/**
* This checks for accents in the potential match started with a .
* composite character.
* This is really painful... we have to check that composite character do not
* have any extra accents. We have to normalize the potential match and find
* the immediate decomposed character before the match.
* The first composite character would have been taken care of by the fcd
* checks in checkForwardExactMatch.
* This is the slow path after the fcd of the first character and
* the last character has been checked by checkForwardExactMatch and we
* determine that the potential match has extra non-ignorable preceding
* ces.
* E.g. looking for \u0301 acute in \u01FA A ring above and acute,
* checkExtraMatchAccent should fail since there is a middle ring in \u01FA
* Note here that accents checking are slow and cautioned in the API docs.
* Internal method, status assumed to be a success, caller should check status
* before calling this method
* @param strsrch string search data
* @param start index of the potential unfriendly composite character
* @param end index of the potential unfriendly composite character
* @param status output error status if any.
* @return TRUE if there is non-ignorable accents before at the beginning
*              of the match, FALSE otherwise.
*/

static
UBool checkExtraMatchAccents(const UStringSearch *strsrch, int32_t start,
                                   int32_t    end,
                                   UErrorCode    *status)
{
    UBool result = FALSE;
    if (strsrch->pattern.hasPrefixAccents) {
              int32_t  length = end - start;
              int32_t  offset = 0;
        const UChar       *text   = strsrch->search->text + start;

        U16_FWD_1(text, offset, length);
        // we are only concerned with the first composite character
        if (unorm_quickCheck(text, offset, UNORM_NFD, status) == UNORM_NO) {
            int32_t safeoffset = getNextSafeOffset(strsrch->collator,
                                                       text, 0, length);
            if (safeoffset != length) {
                safeoffset ++;
            }
            UChar   *norm = NULL;
            UChar    buffer[INITIAL_ARRAY_SIZE_];
            int32_t  size = unorm_normalize(text, safeoffset, UNORM_NFD, 0,
                                            buffer, INITIAL_ARRAY_SIZE_,
                                            status);
            if (U_FAILURE(*status)) {
                return FALSE;
            }
            if (size >= INITIAL_ARRAY_SIZE_) {
                norm = (UChar *)allocateMemory((size + 1) * sizeof(UChar),
                                               status);
                // if allocation failed, status will be set to
                // U_MEMORY_ALLOCATION_ERROR and unorm_normalize internally
                // checks for it.
                size = unorm_normalize(text, safeoffset, UNORM_NFD, 0, norm,
                                       size, status);
                if (U_FAILURE(*status) && norm != NULL) {
                    uprv_free(norm);
                    return FALSE;
                }
            }
            else {
                norm = buffer;
            }

            UCollationElements *coleiter  = strsrch->utilIter;
            ucol_setText(coleiter, norm, size, status);
            uint32_t            firstce   = strsrch->pattern.ces[0];
            UBool               ignorable = TRUE;
            uint32_t            ce        = UCOL_IGNORABLE;
            while (U_SUCCESS(*status) && ce != firstce && ce != (uint32_t)UCOL_NULLORDER) {
                offset = ucol_getOffset(coleiter);
                if (ce != firstce && ce != UCOL_IGNORABLE) {
                    ignorable = FALSE;
                }
                ce = ucol_next(coleiter, status);
            }
            UChar32 codepoint;
            U16_PREV(norm, 0, offset, codepoint);
            result = !ignorable && (u_getCombiningClass(codepoint) != 0);

            if (norm != buffer) {
                uprv_free(norm);
            }
        }
    }

    return result;
}

/**
* Used by exact matches, checks if there are accents before the match.
* This is really painful... we have to check that composite characters at
* the start of the matches have to not have any extra accents.
* We check the FCD of the character first, if it starts with an accent and
* the first pattern ce does not match the first ce of the character, we bail.
* Otherwise we try normalizing the first composite
* character and find the immediate decomposed character before the match to
* see if it is an non-ignorable accent.
* Now normalizing the first composite character is enough because we ensure
* that when the match is passed in here with extra beginning ces, the
* first or last ce that match has to occur within the first character.
* E.g. looking for \u0301 acute in \u01FA A ring above and acute,
* checkExtraMatchAccent should fail since there is a middle ring in \u01FA
* Note here that accents checking are slow and cautioned in the API docs.
* @param strsrch string search data
* @param start offset
* @param end offset
* @return TRUE if there are accents on either side of the match,
*         FALSE otherwise
*/
static
UBool hasAccentsBeforeMatch(const UStringSearch *strsrch, int32_t start,
                                  int32_t    end)
{
    if (strsrch->pattern.hasPrefixAccents) {
        UCollationElements *coleiter  = strsrch->textIter;
        UErrorCode          status    = U_ZERO_ERROR;
        // we have been iterating forwards previously
        uint32_t            ignorable = TRUE;
        int32_t             firstce   = strsrch->pattern.ces[0];

        setColEIterOffset(coleiter, start);
        int32_t ce  = getCE(strsrch, ucol_next(coleiter, &status));
        if (U_FAILURE(status)) {
            return TRUE;
        }
        while (ce != firstce) {
            if (ce != UCOL_IGNORABLE) {
                ignorable = FALSE;
            }
            ce = getCE(strsrch, ucol_next(coleiter, &status));
            if (U_FAILURE(status) || ce == UCOL_NULLORDER) {
                return TRUE;
            }
        }
        if (!ignorable && inNormBuf(coleiter)) {
            // within normalization buffer, discontiguous handled here
            return TRUE;
        }

        // within text
        int32_t temp = start;
        // original code
        // accent = (getFCD(strsrch->search->text, &temp,
        //                  strsrch->search->textLength)
        //            >> SECOND_LAST_BYTE_SHIFT_);
        // however this code does not work well with VC7 .net in release mode.
        // maybe the inlines for getFCD combined with shifting has bugs in
        // VC7. anyways this is a work around.
        UBool accent = getFCD(strsrch->search->text, &temp,
                              strsrch->search->textLength) > 0xFF;
        if (!accent) {
            return checkExtraMatchAccents(strsrch, start, end, &status);
        }
        if (!ignorable) {
            return TRUE;
        }
        if (start > 0) {
            temp = start;
            U16_BACK_1(strsrch->search->text, 0, temp);
            if (getFCD(strsrch->search->text, &temp,
                       strsrch->search->textLength) & LAST_BYTE_MASK_) {
                setColEIterOffset(coleiter, start);
                ce = ucol_previous(coleiter, &status);
                if (U_FAILURE(status) ||
                    (ce != UCOL_NULLORDER && ce != UCOL_IGNORABLE)) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

/**
* Used by exact matches, checks if there are accents bounding the match.
* Note this is the initial boundary check. If the potential match
* starts or ends with composite characters, the accents in those
* characters will be determined later.
* Not doing backwards iteration here, since discontiguos contraction for
* backwards collation element iterator, use up too many characters.
* E.g. looking for \u030A ring in \u01FA A ring above and acute,
* should fail since there is a acute at the end of \u01FA
* Note here that accents checking are slow and cautioned in the API docs.
* @param strsrch string search data
* @param start offset of match
* @param end end offset of the match
* @return TRUE if there are accents on either side of the match,
*         FALSE otherwise
*/
static
UBool hasAccentsAfterMatch(const UStringSearch *strsrch, int32_t start,
                                 int32_t    end)
{
    if (strsrch->pattern.hasSuffixAccents) {
        const UChar       *text       = strsrch->search->text;
              int32_t  temp       = end;
              int32_t      textlength = strsrch->search->textLength;
        U16_BACK_1(text, 0, temp);
        if (getFCD(text, &temp, textlength) & LAST_BYTE_MASK_) {
            int32_t             firstce  = strsrch->pattern.ces[0];
            UCollationElements *coleiter = strsrch->textIter;
            UErrorCode          status   = U_ZERO_ERROR;
            int32_t ce;
            setColEIterOffset(coleiter, start);
            while ((ce = getCE(strsrch, ucol_next(coleiter, &status))) != firstce) {
                if (U_FAILURE(status) || ce == UCOL_NULLORDER) {
                    return TRUE;
                }
            }
            int32_t count = 1;
            while (count < strsrch->pattern.cesLength) {
                if (getCE(strsrch, ucol_next(coleiter, &status))
                    == UCOL_IGNORABLE) {
                    // Thai can give an ignorable here.
                    count --;
                }
                if (U_FAILURE(status)) {
                    return TRUE;
                }
                count ++;
            }

            ce = ucol_next(coleiter, &status);
            if (U_FAILURE(status)) {
                return TRUE;
            }
            if (ce != UCOL_NULLORDER && ce != UCOL_IGNORABLE) {
                ce = getCE(strsrch, ce);
            }
            if (ce != UCOL_NULLORDER && ce != UCOL_IGNORABLE) {
                if (ucol_getOffset(coleiter) <= end) {
                    return TRUE;
                }
                if (getFCD(text, &end, textlength) >> SECOND_LAST_BYTE_SHIFT_) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}
#endif // #if BOYER_MOORE

/**
* Checks if the offset runs out of the text string
* @param offset
* @param textlength of the text string
* @return TRUE if offset is out of bounds, FALSE otherwise
*/
static
inline UBool isOutOfBounds(int32_t textlength, int32_t offset)
{
    return offset < 0 || offset > textlength;
}

/**
* Checks for identical match
* @param strsrch string search data
* @param start offset of possible match
* @param end offset of possible match
* @return TRUE if identical match is found
*/
static
inline UBool checkIdentical(const UStringSearch *strsrch, int32_t start,
                                  int32_t    end)
{
    if (strsrch->strength != UCOL_IDENTICAL) {
        return TRUE;
    }

    // Note: We could use Normalizer::compare() or similar, but for short strings
    // which may not be in FCD it might be faster to just NFD them.
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString t2, p2;
    strsrch->nfd->normalize(
        UnicodeString(FALSE, strsrch->search->text + start, end - start), t2, status);
    strsrch->nfd->normalize(
        UnicodeString(FALSE, strsrch->pattern.text, strsrch->pattern.textLength), p2, status);
    // return FALSE if NFD failed
    return U_SUCCESS(status) && t2 == p2;
}

#if BOYER_MOORE
/**
* Checks to see if the match is repeated
* @param strsrch string search data
* @param start new match start index
* @param end new match end index
* @return TRUE if the the match is repeated, FALSE otherwise
*/
static
inline UBool checkRepeatedMatch(UStringSearch *strsrch,
                                int32_t    start,
                                int32_t    end)
{
    int32_t lastmatchindex = strsrch->search->matchedIndex;
    UBool       result;
    if (lastmatchindex == USEARCH_DONE) {
        return FALSE;
    }
    if (strsrch->search->isForwardSearching) {
        result = start <= lastmatchindex;
    }
    else {
        result = start >= lastmatchindex;
    }
    if (!result && !strsrch->search->isOverlap) {
        if (strsrch->search->isForwardSearching) {
            result = start < lastmatchindex + strsrch->search->matchedLength;
        }
        else {
            result = end > lastmatchindex;
        }
    }
    return result;
}

/**
* Gets the collation element iterator's current offset.
* @param coleiter collation element iterator
* @param forwards flag TRUE if we are moving in th forwards direction
* @return current offset
*/
static
inline int32_t getColElemIterOffset(const UCollationElements *coleiter,
                                              UBool               forwards)
{
    int32_t result = ucol_getOffset(coleiter);
    // intricacies of the the backwards collation element iterator
    if (FALSE && !forwards && inNormBuf(coleiter) && !isFCDPointerNull(coleiter)) {
        result ++;
    }
    return result;
}

/**
* Checks match for contraction.
* If the match ends with a partial contraction we fail.
* If the match starts too far off (because of backwards iteration) we try to
* chip off the extra characters depending on whether a breakiterator has
* been used.
* Internal method, error assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param start offset of potential match, to be modified if necessary
* @param end offset of potential match, to be modified if necessary
* @param status output error status if any
* @return TRUE if match passes the contraction test, FALSE otherwise
*/

static
UBool checkNextExactContractionMatch(UStringSearch *strsrch,
                                     int32_t   *start,
                                     int32_t   *end, UErrorCode  *status)
{
          UCollationElements *coleiter   = strsrch->textIter;
          int32_t             textlength = strsrch->search->textLength;
          int32_t             temp       = *start;
    const UCollator          *collator   = strsrch->collator;
    const UChar              *text       = strsrch->search->text;
    // This part checks if either ends of the match contains potential
    // contraction. If so we'll have to iterate through them
    // The start contraction needs to be checked since ucol_previous dumps
    // all characters till the first safe character into the buffer.
    // *start + 1 is used to test for the unsafe characters instead of *start
    // because ucol_prev takes all unsafe characters till the first safe
    // character ie *start. so by testing *start + 1, we can estimate if
    // excess prefix characters has been included in the potential search
    // results.
    if ((*end < textlength && ucol_unsafeCP(text[*end], collator)) ||
        (*start + 1 < textlength
         && ucol_unsafeCP(text[*start + 1], collator))) {
        int32_t expansion  = getExpansionPrefix(coleiter);
        UBool   expandflag = expansion > 0;
        setColEIterOffset(coleiter, *start);
        while (expansion > 0) {
            // getting rid of the redundant ce, caused by setOffset.
            // since backward contraction/expansion may have extra ces if we
            // are in the normalization buffer, hasAccentsBeforeMatch would
            // have taken care of it.
            // E.g. the character \u01FA will have an expansion of 3, but if
            // we are only looking for acute and ring \u030A and \u0301, we'll
            // have to skip the first ce in the expansion buffer.
            ucol_next(coleiter, status);
            if (U_FAILURE(*status)) {
                return FALSE;
            }
            if (ucol_getOffset(coleiter) != temp) {
                *start = temp;
                temp  = ucol_getOffset(coleiter);
            }
            expansion --;
        }

        int32_t  *patternce       = strsrch->pattern.ces;
        int32_t   patterncelength = strsrch->pattern.cesLength;
        int32_t   count           = 0;
        while (count < patterncelength) {
            int32_t ce = getCE(strsrch, ucol_next(coleiter, status));
            if (ce == UCOL_IGNORABLE) {
                continue;
            }
            if (expandflag && count == 0 && ucol_getOffset(coleiter) != temp) {
                *start = temp;
                temp   = ucol_getOffset(coleiter);
            }
            if (U_FAILURE(*status) || ce != patternce[count]) {
                (*end) ++;
                *end = getNextUStringSearchBaseOffset(strsrch, *end);
                return FALSE;
            }
            count ++;
        }
    }
    return TRUE;
}

/**
* Checks and sets the match information if found.
* Checks
* <ul>
* <li> the potential match does not repeat the previous match
* <li> boundaries are correct
* <li> exact matches has no extra accents
* <li> identical matchesb
* <li> potential match does not end in the middle of a contraction
* <\ul>
* Otherwise the offset will be shifted to the next character.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset offset in the collation element text. the returned value
*        will be the truncated end offset of the match or the new start
*        search offset.
* @param status output error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
inline UBool checkNextExactMatch(UStringSearch *strsrch,
                                 int32_t   *textoffset, UErrorCode *status)
{
    UCollationElements *coleiter = strsrch->textIter;
    int32_t         start    = getColElemIterOffset(coleiter, FALSE);

    if (!checkNextExactContractionMatch(strsrch, &start, textoffset, status)) {
        return FALSE;
    }

    // this totally matches, however we need to check if it is repeating
    if (!isBreakUnit(strsrch, start, *textoffset) ||
        checkRepeatedMatch(strsrch, start, *textoffset) ||
        hasAccentsBeforeMatch(strsrch, start, *textoffset) ||
        !checkIdentical(strsrch, start, *textoffset) ||
        hasAccentsAfterMatch(strsrch, start, *textoffset)) {

        (*textoffset) ++;
        *textoffset = getNextUStringSearchBaseOffset(strsrch, *textoffset);
        return FALSE;
    }

    //Add breakiterator boundary check for primary strength search.
    if (!strsrch->search->breakIter && strsrch->strength == UCOL_PRIMARY) {
        checkBreakBoundary(strsrch, &start, textoffset);
    }

    // totally match, we will get rid of the ending ignorables.
    strsrch->search->matchedIndex  = start;
    strsrch->search->matchedLength = *textoffset - start;
    return TRUE;
}

/**
* Getting the previous base character offset, or the current offset if the
* current character is a base character
* @param text string
* @param textoffset one offset after the current character
* @return the offset of the next character after the base character or the first
*         composed character with accents
*/
static
inline int32_t getPreviousBaseOffset(const UChar       *text,
                                               int32_t  textoffset)
{
    if (textoffset > 0) {
        for (;;) {
            int32_t result = textoffset;
            U16_BACK_1(text, 0, textoffset);
            int32_t temp = textoffset;
            uint16_t fcd = getFCD(text, &temp, result);
            if ((fcd >> SECOND_LAST_BYTE_SHIFT_) == 0) {
                if (fcd & LAST_BYTE_MASK_) {
                    return textoffset;
                }
                return result;
            }
            if (textoffset == 0) {
                return 0;
            }
        }
    }
    return textoffset;
}

/**
* Getting the indexes of the accents that are not blocked in the argument
* accent array
* @param accents array of accents in nfd terminated by a 0.
* @param accentsindex array of indexes of the accents that are not blocked
*/
static
inline int getUnblockedAccentIndex(UChar *accents, int32_t *accentsindex)
{
    int32_t index     = 0;
    int32_t     length    = u_strlen(accents);
    UChar32     codepoint = 0;
    int         cclass    = 0;
    int         result    = 0;
    int32_t temp;
    while (index < length) {
        temp = index;
        U16_NEXT(accents, index, length, codepoint);
        if (u_getCombiningClass(codepoint) != cclass) {
            cclass        = u_getCombiningClass(codepoint);
            accentsindex[result] = temp;
            result ++;
        }
    }
    accentsindex[result] = length;
    return result;
}

/**
* Appends 3 UChar arrays to a destination array.
* Creates a new array if we run out of space. The caller will have to
* manually deallocate the newly allocated array.
* Internal method, status assumed to be success, caller has to check status
* before calling this method. destination not to be NULL and has at least
* size destinationlength.
* @param destination target array
* @param destinationlength target array size, returning the appended length
* @param source1 null-terminated first array
* @param source2 second array
* @param source2length length of seond array
* @param source3 null-terminated third array
* @param status error status if any
* @return new destination array, destination if there was no new allocation
*/
static
inline UChar * addToUCharArray(      UChar      *destination,
                                     int32_t    *destinationlength,
                               const UChar      *source1,
                               const UChar      *source2,
                                     int32_t     source2length,
                               const UChar      *source3,
                                     UErrorCode *status)
{
    int32_t source1length = source1 ? u_strlen(source1) : 0;
    int32_t source3length = source3 ? u_strlen(source3) : 0;
    if (*destinationlength < source1length + source2length + source3length +
                                                                           1)
    {
        destination = (UChar *)allocateMemory(
          (source1length + source2length + source3length + 1) * sizeof(UChar),
          status);
        // if error allocating memory, status will be
        // U_MEMORY_ALLOCATION_ERROR
        if (U_FAILURE(*status)) {
            *destinationlength = 0;
            return NULL;
        }
    }
    if (source1length != 0) {
        u_memcpy(destination, source1, source1length);
    }
    if (source2length != 0) {
        uprv_memcpy(destination + source1length, source2,
                    sizeof(UChar) * source2length);
    }
    if (source3length != 0) {
        uprv_memcpy(destination + source1length + source2length, source3,
                    sizeof(UChar) * source3length);
    }
    *destinationlength = source1length + source2length + source3length;
    return destination;
}

/**
* Running through a collation element iterator to see if the contents matches
* pattern in string search data
* @param strsrch string search data
* @param coleiter collation element iterator
* @return TRUE if a match if found, FALSE otherwise
*/
static
inline UBool checkCollationMatch(const UStringSearch      *strsrch,
                                       UCollationElements *coleiter)
{
    int         patternceindex = strsrch->pattern.cesLength;
    int32_t    *patternce      = strsrch->pattern.ces;
    UErrorCode  status = U_ZERO_ERROR;
    while (patternceindex > 0) {
        int32_t ce = getCE(strsrch, ucol_next(coleiter, &status));
        if (ce == UCOL_IGNORABLE) {
            continue;
        }
        if (U_FAILURE(status) || ce != *patternce) {
            return FALSE;
        }
        patternce ++;
        patternceindex --;
    }
    return TRUE;
}

/**
* Rearranges the front accents to try matching.
* Prefix accents in the text will be grouped according to their combining
* class and the groups will be mixed and matched to try find the perfect
* match with the pattern.
* So for instance looking for "\u0301" in "\u030A\u0301\u0325"
* step 1: split "\u030A\u0301" into 6 other type of potential accent substrings
*         "\u030A", "\u0301", "\u0325", "\u030A\u0301", "\u030A\u0325",
*         "\u0301\u0325".
* step 2: check if any of the generated substrings matches the pattern.
* Internal method, status is assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search match
* @param start first offset of the accents to start searching
* @param end start of the last accent set
* @param status output error status if any
* @return USEARCH_DONE if a match is not found, otherwise return the starting
*         offset of the match. Note this start includes all preceding accents.
*/
static
int32_t doNextCanonicalPrefixMatch(UStringSearch *strsrch,
                                       int32_t    start,
                                       int32_t    end,
                                       UErrorCode    *status)
{
    const UChar       *text       = strsrch->search->text;
          int32_t      textlength = strsrch->search->textLength;
          int32_t  tempstart  = start;

    if ((getFCD(text, &tempstart, textlength) & LAST_BYTE_MASK_) == 0) {
        // die... failed at a base character
        return USEARCH_DONE;
    }

    int32_t offset = getNextBaseOffset(text, tempstart, textlength);
    start = getPreviousBaseOffset(text, tempstart);

    UChar       accents[INITIAL_ARRAY_SIZE_];
    // normalizing the offensive string
    unorm_normalize(text + start, offset - start, UNORM_NFD, 0, accents,
                    INITIAL_ARRAY_SIZE_, status);
    if (U_FAILURE(*status)) {
        return USEARCH_DONE;
    }

    int32_t         accentsindex[INITIAL_ARRAY_SIZE_];
    int32_t         accentsize = getUnblockedAccentIndex(accents,
                                                                 accentsindex);
    int32_t         count      = (2 << (accentsize - 1)) - 1;
    UChar               buffer[INITIAL_ARRAY_SIZE_];
    UCollationElements *coleiter   = strsrch->utilIter;
    while (U_SUCCESS(*status) && count > 0) {
        UChar *rearrange = strsrch->canonicalPrefixAccents;
        // copy the base characters
        for (int k = 0; k < accentsindex[0]; k ++) {
            *rearrange ++ = accents[k];
        }
        // forming all possible canonical rearrangement by dropping
        // sets of accents
        for (int i = 0; i <= accentsize - 1; i ++) {
            int32_t mask = 1 << (accentsize - i - 1);
            if (count & mask) {
                for (int j = accentsindex[i]; j < accentsindex[i + 1]; j ++) {
                    *rearrange ++ = accents[j];
                }
            }
        }
        *rearrange = 0;
        int32_t  matchsize = INITIAL_ARRAY_SIZE_;
        UChar   *match     = addToUCharArray(buffer, &matchsize,
                                           strsrch->canonicalPrefixAccents,
                                           strsrch->search->text + offset,
                                           end - offset,
                                           strsrch->canonicalSuffixAccents,
                                           status);

        // if status is a failure, ucol_setText does nothing.
        // run the collator iterator through this match
        ucol_setText(coleiter, match, matchsize, status);
        if (U_SUCCESS(*status)) {
            if (checkCollationMatch(strsrch, coleiter)) {
                if (match != buffer) {
                    uprv_free(match);
                }
                return start;
            }
        }
        count --;
    }
    return USEARCH_DONE;
}

/**
* Gets the offset to the safe point in text before textoffset.
* ie. not the middle of a contraction, swappable characters or supplementary
* characters.
* @param collator collation sata
* @param text string to work with
* @param textoffset offset in string
* @param textlength length of text string
* @return offset to the previous safe character
*/
static
inline uint32_t getPreviousSafeOffset(const UCollator   *collator,
                                      const UChar       *text,
                                            int32_t  textoffset)
{
    int32_t result = textoffset; // first contraction character
    while (result != 0 && ucol_unsafeCP(text[result - 1], collator)) {
        result --;
    }
    if (result != 0) {
        // the first contraction character is consider unsafe here
        result --;
    }
    return result;
}

/**
* Cleaning up after we passed the safe zone
* @param strsrch string search data
* @param safetext safe text array
* @param safebuffer safe text buffer
* @param coleiter collation element iterator for safe text
*/
static
inline void cleanUpSafeText(const UStringSearch *strsrch, UChar *safetext,
                                  UChar         *safebuffer)
{
    if (safetext != safebuffer && safetext != strsrch->canonicalSuffixAccents)
    {
       uprv_free(safetext);
    }
}

/**
* Take the rearranged end accents and tries matching. If match failed at
* a seperate preceding set of accents (seperated from the rearranged on by
* at least a base character) then we rearrange the preceding accents and
* tries matching again.
* We allow skipping of the ends of the accent set if the ces do not match.
* However if the failure is found before the accent set, it fails.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset of the start of the rearranged accent
* @param status output error status if any
* @return USEARCH_DONE if a match is not found, otherwise return the starting
*         offset of the match. Note this start includes all preceding accents.
*/
static
int32_t doNextCanonicalSuffixMatch(UStringSearch *strsrch,
                                       int32_t    textoffset,
                                       UErrorCode    *status)
{
    const UChar              *text           = strsrch->search->text;
    const UCollator          *collator       = strsrch->collator;
          int32_t             safelength     = 0;
          UChar              *safetext;
          int32_t             safetextlength;
          UChar               safebuffer[INITIAL_ARRAY_SIZE_];
          UCollationElements *coleiter       = strsrch->utilIter;
          int32_t         safeoffset     = textoffset;

    if (textoffset != 0 && ucol_unsafeCP(strsrch->canonicalSuffixAccents[0],
                                         collator)) {
        safeoffset     = getPreviousSafeOffset(collator, text, textoffset);
        safelength     = textoffset - safeoffset;
        safetextlength = INITIAL_ARRAY_SIZE_;
        safetext       = addToUCharArray(safebuffer, &safetextlength, NULL,
                                         text + safeoffset, safelength,
                                         strsrch->canonicalSuffixAccents,
                                         status);
    }
    else {
        safetextlength = u_strlen(strsrch->canonicalSuffixAccents);
        safetext       = strsrch->canonicalSuffixAccents;
    }

    // if status is a failure, ucol_setText does nothing
    ucol_setText(coleiter, safetext, safetextlength, status);
    // status checked in loop below

    int32_t  *ce        = strsrch->pattern.ces;
    int32_t   celength  = strsrch->pattern.cesLength;
    int       ceindex   = celength - 1;
    UBool     isSafe    = TRUE; // indication flag for position in safe zone

    while (ceindex >= 0) {
        int32_t textce = ucol_previous(coleiter, status);
        if (U_FAILURE(*status)) {
            if (isSafe) {
                cleanUpSafeText(strsrch, safetext, safebuffer);
            }
            return USEARCH_DONE;
        }
        if (textce == UCOL_NULLORDER) {
            // check if we have passed the safe buffer
            if (coleiter == strsrch->textIter) {
                cleanUpSafeText(strsrch, safetext, safebuffer);
                return USEARCH_DONE;
            }
            cleanUpSafeText(strsrch, safetext, safebuffer);
            safetext = safebuffer;
            coleiter = strsrch->textIter;
            setColEIterOffset(coleiter, safeoffset);
            // status checked at the start of the loop
            isSafe = FALSE;
            continue;
        }
        textce = getCE(strsrch, textce);
        if (textce != UCOL_IGNORABLE && textce != ce[ceindex]) {
            // do the beginning stuff
            int32_t failedoffset = getColElemIterOffset(coleiter, FALSE);
            if (isSafe && failedoffset >= safelength) {
                // alas... no hope. failed at rearranged accent set
                cleanUpSafeText(strsrch, safetext, safebuffer);
                return USEARCH_DONE;
            }
            else {
                if (isSafe) {
                    failedoffset += safeoffset;
                    cleanUpSafeText(strsrch, safetext, safebuffer);
                }

                // try rearranging the front accents
                int32_t result = doNextCanonicalPrefixMatch(strsrch,
                                        failedoffset, textoffset, status);
                if (result != USEARCH_DONE) {
                    // if status is a failure, ucol_setOffset does nothing
                    setColEIterOffset(strsrch->textIter, result);
                }
                if (U_FAILURE(*status)) {
                    return USEARCH_DONE;
                }
                return result;
            }
        }
        if (textce == ce[ceindex]) {
            ceindex --;
        }
    }
    // set offset here
    if (isSafe) {
        int32_t result     = getColElemIterOffset(coleiter, FALSE);
        // sets the text iterator here with the correct expansion and offset
        int32_t    leftoverces = getExpansionPrefix(coleiter);
        cleanUpSafeText(strsrch, safetext, safebuffer);
        if (result >= safelength) {
            result = textoffset;
        }
        else {
            result += safeoffset;
        }
        setColEIterOffset(strsrch->textIter, result);
        strsrch->textIter->iteratordata_.toReturn =
                       setExpansionPrefix(strsrch->textIter, leftoverces);
        return result;
    }

    return ucol_getOffset(coleiter);
}

/**
* Trying out the substring and sees if it can be a canonical match.
* This will try normalizing the end accents and arranging them into canonical
* equivalents and check their corresponding ces with the pattern ce.
* Suffix accents in the text will be grouped according to their combining
* class and the groups will be mixed and matched to try find the perfect
* match with the pattern.
* So for instance looking for "\u0301" in "\u030A\u0301\u0325"
* step 1: split "\u030A\u0301" into 6 other type of potential accent substrings
*         "\u030A", "\u0301", "\u0325", "\u030A\u0301", "\u030A\u0325",
*         "\u0301\u0325".
* step 2: check if any of the generated substrings matches the pattern.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset end offset in the collation element text that ends with
*                   the accents to be rearranged
* @param status error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
UBool doNextCanonicalMatch(UStringSearch *strsrch,
                           int32_t    textoffset,
                           UErrorCode    *status)
{
    const UChar       *text = strsrch->search->text;
          int32_t  temp = textoffset;
    U16_BACK_1(text, 0, temp);
    if ((getFCD(text, &temp, textoffset) & LAST_BYTE_MASK_) == 0) {
        UCollationElements *coleiter = strsrch->textIter;
        int32_t         offset   = getColElemIterOffset(coleiter, FALSE);
        if (strsrch->pattern.hasPrefixAccents) {
            offset = doNextCanonicalPrefixMatch(strsrch, offset, textoffset,
                                                status);
            if (U_SUCCESS(*status) && offset != USEARCH_DONE) {
                setColEIterOffset(coleiter, offset);
                return TRUE;
            }
        }
        return FALSE;
    }

    if (!strsrch->pattern.hasSuffixAccents) {
        return FALSE;
    }

    UChar       accents[INITIAL_ARRAY_SIZE_];
    // offset to the last base character in substring to search
    int32_t baseoffset = getPreviousBaseOffset(text, textoffset);
    // normalizing the offensive string
    unorm_normalize(text + baseoffset, textoffset - baseoffset, UNORM_NFD,
                               0, accents, INITIAL_ARRAY_SIZE_, status);
    // status checked in loop below

    int32_t accentsindex[INITIAL_ARRAY_SIZE_];
    int32_t size = getUnblockedAccentIndex(accents, accentsindex);

    // 2 power n - 1 plus the full set of accents
    int32_t  count = (2 << (size - 1)) - 1;
    while (U_SUCCESS(*status) && count > 0) {
        UChar *rearrange = strsrch->canonicalSuffixAccents;
        // copy the base characters
        for (int k = 0; k < accentsindex[0]; k ++) {
            *rearrange ++ = accents[k];
        }
        // forming all possible canonical rearrangement by dropping
        // sets of accents
        for (int i = 0; i <= size - 1; i ++) {
            int32_t mask = 1 << (size - i - 1);
            if (count & mask) {
                for (int j = accentsindex[i]; j < accentsindex[i + 1]; j ++) {
                    *rearrange ++ = accents[j];
                }
            }
        }
        *rearrange = 0;
        int32_t offset = doNextCanonicalSuffixMatch(strsrch, baseoffset,
                                                        status);
        if (offset != USEARCH_DONE) {
            return TRUE; // match found
        }
        count --;
    }
    return FALSE;
}

/**
* Gets the previous base character offset depending on the string search
* pattern data
* @param strsrch string search data
* @param textoffset current offset, current character
* @return the offset of the next character after this base character or itself
*         if it is a composed character with accents
*/
static
inline int32_t getPreviousUStringSearchBaseOffset(UStringSearch *strsrch,
                                                      int32_t textoffset)
{
    if (strsrch->pattern.hasPrefixAccents && textoffset > 0) {
        const UChar       *text = strsrch->search->text;
              int32_t  offset = textoffset;
        if (getFCD(text, &offset, strsrch->search->textLength) >>
                                                   SECOND_LAST_BYTE_SHIFT_) {
            return getPreviousBaseOffset(text, textoffset);
        }
    }
    return textoffset;
}

/**
* Checks match for contraction.
* If the match ends with a partial contraction we fail.
* If the match starts too far off (because of backwards iteration) we try to
* chip off the extra characters
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param start offset of potential match, to be modified if necessary
* @param end offset of potential match, to be modified if necessary
* @param status output error status if any
* @return TRUE if match passes the contraction test, FALSE otherwise
*/
static
UBool checkNextCanonicalContractionMatch(UStringSearch *strsrch,
                                         int32_t   *start,
                                         int32_t   *end,
                                         UErrorCode    *status)
{
          UCollationElements *coleiter   = strsrch->textIter;
          int32_t             textlength = strsrch->search->textLength;
          int32_t         temp       = *start;
    const UCollator          *collator   = strsrch->collator;
    const UChar              *text       = strsrch->search->text;
    // This part checks if either ends of the match contains potential
    // contraction. If so we'll have to iterate through them
    if ((*end < textlength && ucol_unsafeCP(text[*end], collator)) ||
        (*start + 1 < textlength
         && ucol_unsafeCP(text[*start + 1], collator))) {
        int32_t expansion  = getExpansionPrefix(coleiter);
        UBool   expandflag = expansion > 0;
        setColEIterOffset(coleiter, *start);
        while (expansion > 0) {
            // getting rid of the redundant ce, caused by setOffset.
            // since backward contraction/expansion may have extra ces if we
            // are in the normalization buffer, hasAccentsBeforeMatch would
            // have taken care of it.
            // E.g. the character \u01FA will have an expansion of 3, but if
            // we are only looking for acute and ring \u030A and \u0301, we'll
            // have to skip the first ce in the expansion buffer.
            ucol_next(coleiter, status);
            if (U_FAILURE(*status)) {
                return FALSE;
            }
            if (ucol_getOffset(coleiter) != temp) {
                *start = temp;
                temp  = ucol_getOffset(coleiter);
            }
            expansion --;
        }

        int32_t  *patternce       = strsrch->pattern.ces;
        int32_t   patterncelength = strsrch->pattern.cesLength;
        int32_t   count           = 0;
        int32_t   textlength      = strsrch->search->textLength;
        while (count < patterncelength) {
            int32_t ce = getCE(strsrch, ucol_next(coleiter, status));
            // status checked below, note that if status is a failure
            // ucol_next returns UCOL_NULLORDER
            if (ce == UCOL_IGNORABLE) {
                continue;
            }
            if (expandflag && count == 0 && ucol_getOffset(coleiter) != temp) {
                *start = temp;
                temp   = ucol_getOffset(coleiter);
            }

            if (count == 0 && ce != patternce[0]) {
                // accents may have extra starting ces, this occurs when a
                // pure accent pattern is matched without rearrangement
                // text \u0325\u0300 and looking for \u0300
                int32_t expected = patternce[0];
                if (getFCD(text, start, textlength) & LAST_BYTE_MASK_) {
                    ce = getCE(strsrch, ucol_next(coleiter, status));
                    while (U_SUCCESS(*status) && ce != expected &&
                           ce != UCOL_NULLORDER &&
                           ucol_getOffset(coleiter) <= *end) {
                        ce = getCE(strsrch, ucol_next(coleiter, status));
                    }
                }
            }
            if (U_FAILURE(*status) || ce != patternce[count]) {
                (*end) ++;
                *end = getNextUStringSearchBaseOffset(strsrch, *end);
                return FALSE;
            }
            count ++;
        }
    }
    return TRUE;
}

/**
* Checks and sets the match information if found.
* Checks
* <ul>
* <li> the potential match does not repeat the previous match
* <li> boundaries are correct
* <li> potential match does not end in the middle of a contraction
* <li> identical matches
* <\ul>
* Otherwise the offset will be shifted to the next character.
* Internal method, status assumed to be success, caller has to check the
* status before calling this method.
* @param strsrch string search data
* @param textoffset offset in the collation element text. the returned value
*        will be the truncated end offset of the match or the new start
*        search offset.
* @param status output error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
inline UBool checkNextCanonicalMatch(UStringSearch *strsrch,
                                     int32_t   *textoffset,
                                     UErrorCode    *status)
{
    // to ensure that the start and ends are not composite characters
    UCollationElements *coleiter = strsrch->textIter;
    // if we have a canonical accent match
    if ((strsrch->pattern.hasSuffixAccents &&
        strsrch->canonicalSuffixAccents[0]) ||
        (strsrch->pattern.hasPrefixAccents &&
        strsrch->canonicalPrefixAccents[0])) {
        strsrch->search->matchedIndex  = getPreviousUStringSearchBaseOffset(
                                                    strsrch,
                                                    ucol_getOffset(coleiter));
        strsrch->search->matchedLength = *textoffset -
                                                strsrch->search->matchedIndex;
        return TRUE;
    }

    int32_t start = getColElemIterOffset(coleiter, FALSE);
    if (!checkNextCanonicalContractionMatch(strsrch, &start, textoffset,
                                            status) || U_FAILURE(*status)) {
        return FALSE;
    }

    start = getPreviousUStringSearchBaseOffset(strsrch, start);
    // this totally matches, however we need to check if it is repeating
    if (checkRepeatedMatch(strsrch, start, *textoffset) ||
        !isBreakUnit(strsrch, start, *textoffset) ||
        !checkIdentical(strsrch, start, *textoffset)) {
        (*textoffset) ++;
        *textoffset = getNextBaseOffset(strsrch->search->text, *textoffset,
                                        strsrch->search->textLength);
        return FALSE;
    }

    strsrch->search->matchedIndex  = start;
    strsrch->search->matchedLength = *textoffset - start;
    return TRUE;
}

/**
* Shifting the collation element iterator position forward to prepare for
* a preceding match. If the first character is a unsafe character, we'll only
* shift by 1 to capture contractions, normalization etc.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param text strsrch string search data
* @param textoffset start text position to do search
* @param ce the text ce which failed the match.
* @param patternceindex index of the ce within the pattern ce buffer which
*        failed the match
* @return final offset
*/
static
inline int32_t reverseShift(UStringSearch *strsrch,
                                int32_t    textoffset,
                                int32_t       ce,
                                int32_t        patternceindex)
{
    if (strsrch->search->isOverlap) {
        if (textoffset != strsrch->search->textLength) {
            textoffset --;
        }
        else {
            textoffset -= strsrch->pattern.defaultShiftSize;
        }
    }
    else {
        if (ce != UCOL_NULLORDER) {
            int32_t shift = strsrch->pattern.backShift[hashFromCE32(ce)];

            // this is to adjust for characters in the middle of the substring
            // for matching that failed.
            int32_t adjust = patternceindex;
            if (adjust > 1 && shift > adjust) {
                shift -= adjust - 1;
            }
            textoffset -= shift;
        }
        else {
            textoffset -= strsrch->pattern.defaultShiftSize;
        }
    }
    textoffset = getPreviousUStringSearchBaseOffset(strsrch, textoffset);
    return textoffset;
}

/**
* Checks match for contraction.
* If the match starts with a partial contraction we fail.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param start offset of potential match, to be modified if necessary
* @param end offset of potential match, to be modified if necessary
* @param status output error status if any
* @return TRUE if match passes the contraction test, FALSE otherwise
*/
static
UBool checkPreviousExactContractionMatch(UStringSearch *strsrch,
                                     int32_t   *start,
                                     int32_t   *end, UErrorCode  *status)
{
          UCollationElements *coleiter   = strsrch->textIter;
          int32_t             textlength = strsrch->search->textLength;
          int32_t             temp       = *end;
    const UCollator          *collator   = strsrch->collator;
    const UChar              *text       = strsrch->search->text;
    // This part checks if either if the start of the match contains potential
    // contraction. If so we'll have to iterate through them
    // Since we used ucol_next while previously looking for the potential
    // match, this guarantees that our end will not be a partial contraction,
    // or a partial supplementary character.
    if (*start < textlength && ucol_unsafeCP(text[*start], collator)) {
        int32_t expansion  = getExpansionSuffix(coleiter);
        UBool   expandflag = expansion > 0;
        setColEIterOffset(coleiter, *end);
        while (U_SUCCESS(*status) && expansion > 0) {
            // getting rid of the redundant ce
            // since forward contraction/expansion may have extra ces
            // if we are in the normalization buffer, hasAccentsBeforeMatch
            // would have taken care of it.
            // E.g. the character \u01FA will have an expansion of 3, but if
            // we are only looking for A ring A\u030A, we'll have to skip the
            // last ce in the expansion buffer
            ucol_previous(coleiter, status);
            if (U_FAILURE(*status)) {
                return FALSE;
            }
            if (ucol_getOffset(coleiter) != temp) {
                *end = temp;
                temp  = ucol_getOffset(coleiter);
            }
            expansion --;
        }

        int32_t  *patternce       = strsrch->pattern.ces;
        int32_t   patterncelength = strsrch->pattern.cesLength;
        int32_t   count           = patterncelength;
        while (count > 0) {
            int32_t ce = getCE(strsrch, ucol_previous(coleiter, status));
            // status checked below, note that if status is a failure
            // ucol_previous returns UCOL_NULLORDER
            if (ce == UCOL_IGNORABLE) {
                continue;
            }
            if (expandflag && count == 0 &&
                getColElemIterOffset(coleiter, FALSE) != temp) {
                *end = temp;
                temp  = ucol_getOffset(coleiter);
            }
            if (U_FAILURE(*status) || ce != patternce[count - 1]) {
                (*start) --;
                *start = getPreviousBaseOffset(text, *start);
                return FALSE;
            }
            count --;
        }
    }
    return TRUE;
}

/**
* Checks and sets the match information if found.
* Checks
* <ul>
* <li> the current match does not repeat the last match
* <li> boundaries are correct
* <li> exact matches has no extra accents
* <li> identical matches
* <\ul>
* Otherwise the offset will be shifted to the preceding character.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param collator
* @param coleiter collation element iterator
* @param text string
* @param textoffset offset in the collation element text. the returned value
*        will be the truncated start offset of the match or the new start
*        search offset.
* @param status output error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
inline UBool checkPreviousExactMatch(UStringSearch *strsrch,
                                     int32_t   *textoffset,
                                     UErrorCode    *status)
{
    // to ensure that the start and ends are not composite characters
    int32_t end = ucol_getOffset(strsrch->textIter);
    if (!checkPreviousExactContractionMatch(strsrch, textoffset, &end, status)
        || U_FAILURE(*status)) {
            return FALSE;
    }

    // this totally matches, however we need to check if it is repeating
    // the old match
    if (checkRepeatedMatch(strsrch, *textoffset, end) ||
        !isBreakUnit(strsrch, *textoffset, end) ||
        hasAccentsBeforeMatch(strsrch, *textoffset, end) ||
        !checkIdentical(strsrch, *textoffset, end) ||
        hasAccentsAfterMatch(strsrch, *textoffset, end)) {
        (*textoffset) --;
        *textoffset = getPreviousBaseOffset(strsrch->search->text,
                                            *textoffset);
        return FALSE;
    }

    //Add breakiterator boundary check for primary strength search.
    if (!strsrch->search->breakIter && strsrch->strength == UCOL_PRIMARY) {
        checkBreakBoundary(strsrch, textoffset, &end);
    }

    strsrch->search->matchedIndex = *textoffset;
    strsrch->search->matchedLength = end - *textoffset;
    return TRUE;
}

/**
* Rearranges the end accents to try matching.
* Suffix accents in the text will be grouped according to their combining
* class and the groups will be mixed and matched to try find the perfect
* match with the pattern.
* So for instance looking for "\u0301" in "\u030A\u0301\u0325"
* step 1: split "\u030A\u0301" into 6 other type of potential accent substrings
*         "\u030A", "\u0301", "\u0325", "\u030A\u0301", "\u030A\u0325",
*         "\u0301\u0325".
* step 2: check if any of the generated substrings matches the pattern.
* Internal method, status assumed to be success, user has to check status
* before calling this method.
* @param strsrch string search match
* @param start offset of the first base character
* @param end start of the last accent set
* @param status only error status if any
* @return USEARCH_DONE if a match is not found, otherwise return the ending
*         offset of the match. Note this start includes all following accents.
*/
static
int32_t doPreviousCanonicalSuffixMatch(UStringSearch *strsrch,
                                           int32_t    start,
                                           int32_t    end,
                                           UErrorCode    *status)
{
    const UChar       *text       = strsrch->search->text;
          int32_t  tempend    = end;

    U16_BACK_1(text, 0, tempend);
    if (!(getFCD(text, &tempend, strsrch->search->textLength) &
                                                           LAST_BYTE_MASK_)) {
        // die... failed at a base character
        return USEARCH_DONE;
    }
    end = getNextBaseOffset(text, end, strsrch->search->textLength);

    if (U_SUCCESS(*status)) {
        UChar       accents[INITIAL_ARRAY_SIZE_];
        int32_t offset = getPreviousBaseOffset(text, end);
        // normalizing the offensive string
        unorm_normalize(text + offset, end - offset, UNORM_NFD, 0, accents,
                        INITIAL_ARRAY_SIZE_, status);

        int32_t         accentsindex[INITIAL_ARRAY_SIZE_];
        int32_t         accentsize = getUnblockedAccentIndex(accents,
                                                         accentsindex);
        int32_t         count      = (2 << (accentsize - 1)) - 1;
        UChar               buffer[INITIAL_ARRAY_SIZE_];
        UCollationElements *coleiter = strsrch->utilIter;
        while (U_SUCCESS(*status) && count > 0) {
            UChar *rearrange = strsrch->canonicalSuffixAccents;
            // copy the base characters
            for (int k = 0; k < accentsindex[0]; k ++) {
                *rearrange ++ = accents[k];
            }
            // forming all possible canonical rearrangement by dropping
            // sets of accents
            for (int i = 0; i <= accentsize - 1; i ++) {
                int32_t mask = 1 << (accentsize - i - 1);
                if (count & mask) {
                    for (int j = accentsindex[i]; j < accentsindex[i + 1]; j ++) {
                        *rearrange ++ = accents[j];
                    }
                }
            }
            *rearrange = 0;
            int32_t  matchsize = INITIAL_ARRAY_SIZE_;
            UChar   *match     = addToUCharArray(buffer, &matchsize,
                                           strsrch->canonicalPrefixAccents,
                                           strsrch->search->text + start,
                                           offset - start,
                                           strsrch->canonicalSuffixAccents,
                                           status);

            // run the collator iterator through this match
            // if status is a failure ucol_setText does nothing
            ucol_setText(coleiter, match, matchsize, status);
            if (U_SUCCESS(*status)) {
                if (checkCollationMatch(strsrch, coleiter)) {
                    if (match != buffer) {
                        uprv_free(match);
                    }
                    return end;
                }
            }
            count --;
        }
    }
    return USEARCH_DONE;
}

/**
* Take the rearranged start accents and tries matching. If match failed at
* a seperate following set of accents (seperated from the rearranged on by
* at least a base character) then we rearrange the preceding accents and
* tries matching again.
* We allow skipping of the ends of the accent set if the ces do not match.
* However if the failure is found before the accent set, it fails.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset of the ends of the rearranged accent
* @param status output error status if any
* @return USEARCH_DONE if a match is not found, otherwise return the ending
*         offset of the match. Note this start includes all following accents.
*/
static
int32_t doPreviousCanonicalPrefixMatch(UStringSearch *strsrch,
                                           int32_t    textoffset,
                                           UErrorCode    *status)
{
    const UChar       *text       = strsrch->search->text;
    const UCollator   *collator   = strsrch->collator;
          int32_t      safelength = 0;
          UChar       *safetext;
          int32_t      safetextlength;
          UChar        safebuffer[INITIAL_ARRAY_SIZE_];
          int32_t  safeoffset = textoffset;

    if (textoffset &&
        ucol_unsafeCP(strsrch->canonicalPrefixAccents[
                                 u_strlen(strsrch->canonicalPrefixAccents) - 1
                                         ], collator)) {
        safeoffset     = getNextSafeOffset(collator, text, textoffset,
                                           strsrch->search->textLength);
        safelength     = safeoffset - textoffset;
        safetextlength = INITIAL_ARRAY_SIZE_;
        safetext       = addToUCharArray(safebuffer, &safetextlength,
                                         strsrch->canonicalPrefixAccents,
                                         text + textoffset, safelength,
                                         NULL, status);
    }
    else {
        safetextlength = u_strlen(strsrch->canonicalPrefixAccents);
        safetext       = strsrch->canonicalPrefixAccents;
    }

    UCollationElements *coleiter = strsrch->utilIter;
     // if status is a failure, ucol_setText does nothing
    ucol_setText(coleiter, safetext, safetextlength, status);
    // status checked in loop below

    int32_t  *ce           = strsrch->pattern.ces;
    int32_t   celength     = strsrch->pattern.cesLength;
    int       ceindex      = 0;
    UBool     isSafe       = TRUE; // safe zone indication flag for position
    int32_t   prefixlength = u_strlen(strsrch->canonicalPrefixAccents);

    while (ceindex < celength) {
        int32_t textce = ucol_next(coleiter, status);
        if (U_FAILURE(*status)) {
            if (isSafe) {
                cleanUpSafeText(strsrch, safetext, safebuffer);
            }
            return USEARCH_DONE;
        }
        if (textce == UCOL_NULLORDER) {
            // check if we have passed the safe buffer
            if (coleiter == strsrch->textIter) {
                cleanUpSafeText(strsrch, safetext, safebuffer);
                return USEARCH_DONE;
            }
            cleanUpSafeText(strsrch, safetext, safebuffer);
            safetext = safebuffer;
            coleiter = strsrch->textIter;
            setColEIterOffset(coleiter, safeoffset);
            // status checked at the start of the loop
            isSafe = FALSE;
            continue;
        }
        textce = getCE(strsrch, textce);
        if (textce != UCOL_IGNORABLE && textce != ce[ceindex]) {
            // do the beginning stuff
            int32_t failedoffset = ucol_getOffset(coleiter);
            if (isSafe && failedoffset <= prefixlength) {
                // alas... no hope. failed at rearranged accent set
                cleanUpSafeText(strsrch, safetext, safebuffer);
                return USEARCH_DONE;
            }
            else {
                if (isSafe) {
                    failedoffset = safeoffset - failedoffset;
                    cleanUpSafeText(strsrch, safetext, safebuffer);
                }

                // try rearranging the end accents
                int32_t result = doPreviousCanonicalSuffixMatch(strsrch,
                                        textoffset, failedoffset, status);
                if (result != USEARCH_DONE) {
                    // if status is a failure, ucol_setOffset does nothing
                    setColEIterOffset(strsrch->textIter, result);
                }
                if (U_FAILURE(*status)) {
                    return USEARCH_DONE;
                }
                return result;
            }
        }
        if (textce == ce[ceindex]) {
            ceindex ++;
        }
    }
    // set offset here
    if (isSafe) {
        int32_t result      = ucol_getOffset(coleiter);
        // sets the text iterator here with the correct expansion and offset
        int32_t     leftoverces = getExpansionSuffix(coleiter);
        cleanUpSafeText(strsrch, safetext, safebuffer);
        if (result <= prefixlength) {
            result = textoffset;
        }
        else {
            result = textoffset + (safeoffset - result);
        }
        setColEIterOffset(strsrch->textIter, result);
        setExpansionSuffix(strsrch->textIter, leftoverces);
        return result;
    }

    return ucol_getOffset(coleiter);
}

/**
* Trying out the substring and sees if it can be a canonical match.
* This will try normalizing the starting accents and arranging them into
* canonical equivalents and check their corresponding ces with the pattern ce.
* Prefix accents in the text will be grouped according to their combining
* class and the groups will be mixed and matched to try find the perfect
* match with the pattern.
* So for instance looking for "\u0301" in "\u030A\u0301\u0325"
* step 1: split "\u030A\u0301" into 6 other type of potential accent substrings
*         "\u030A", "\u0301", "\u0325", "\u030A\u0301", "\u030A\u0325",
*         "\u0301\u0325".
* step 2: check if any of the generated substrings matches the pattern.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset start offset in the collation element text that starts
*                   with the accents to be rearranged
* @param status output error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
UBool doPreviousCanonicalMatch(UStringSearch *strsrch,
                               int32_t    textoffset,
                               UErrorCode    *status)
{
    const UChar       *text       = strsrch->search->text;
          int32_t  temp       = textoffset;
          int32_t      textlength = strsrch->search->textLength;
    if ((getFCD(text, &temp, textlength) >> SECOND_LAST_BYTE_SHIFT_) == 0) {
        UCollationElements *coleiter = strsrch->textIter;
        int32_t         offset   = ucol_getOffset(coleiter);
        if (strsrch->pattern.hasSuffixAccents) {
            offset = doPreviousCanonicalSuffixMatch(strsrch, textoffset,
                                                    offset, status);
            if (U_SUCCESS(*status) && offset != USEARCH_DONE) {
                setColEIterOffset(coleiter, offset);
                return TRUE;
            }
        }
        return FALSE;
    }

    if (!strsrch->pattern.hasPrefixAccents) {
        return FALSE;
    }

    UChar       accents[INITIAL_ARRAY_SIZE_];
    // offset to the last base character in substring to search
    int32_t baseoffset = getNextBaseOffset(text, textoffset, textlength);
    // normalizing the offensive string
    unorm_normalize(text + textoffset, baseoffset - textoffset, UNORM_NFD,
                               0, accents, INITIAL_ARRAY_SIZE_, status);
    // status checked in loop

    int32_t accentsindex[INITIAL_ARRAY_SIZE_];
    int32_t size = getUnblockedAccentIndex(accents, accentsindex);

    // 2 power n - 1 plus the full set of accents
    int32_t  count = (2 << (size - 1)) - 1;
    while (U_SUCCESS(*status) && count > 0) {
        UChar *rearrange = strsrch->canonicalPrefixAccents;
        // copy the base characters
        for (int k = 0; k < accentsindex[0]; k ++) {
            *rearrange ++ = accents[k];
        }
        // forming all possible canonical rearrangement by dropping
        // sets of accents
        for (int i = 0; i <= size - 1; i ++) {
            int32_t mask = 1 << (size - i - 1);
            if (count & mask) {
                for (int j = accentsindex[i]; j < accentsindex[i + 1]; j ++) {
                    *rearrange ++ = accents[j];
                }
            }
        }
        *rearrange = 0;
        int32_t offset = doPreviousCanonicalPrefixMatch(strsrch,
                                                          baseoffset, status);
        if (offset != USEARCH_DONE) {
            return TRUE; // match found
        }
        count --;
    }
    return FALSE;
}

/**
* Checks match for contraction.
* If the match starts with a partial contraction we fail.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param start offset of potential match, to be modified if necessary
* @param end offset of potential match, to be modified if necessary
* @param status only error status if any
* @return TRUE if match passes the contraction test, FALSE otherwise
*/
static
UBool checkPreviousCanonicalContractionMatch(UStringSearch *strsrch,
                                     int32_t   *start,
                                     int32_t   *end, UErrorCode  *status)
{
          UCollationElements *coleiter   = strsrch->textIter;
          int32_t             textlength = strsrch->search->textLength;
          int32_t         temp       = *end;
    const UCollator          *collator   = strsrch->collator;
    const UChar              *text       = strsrch->search->text;
    // This part checks if either if the start of the match contains potential
    // contraction. If so we'll have to iterate through them
    // Since we used ucol_next while previously looking for the potential
    // match, this guarantees that our end will not be a partial contraction,
    // or a partial supplementary character.
    if (*start < textlength && ucol_unsafeCP(text[*start], collator)) {
        int32_t expansion  = getExpansionSuffix(coleiter);
        UBool   expandflag = expansion > 0;
        setColEIterOffset(coleiter, *end);
        while (expansion > 0) {
            // getting rid of the redundant ce
            // since forward contraction/expansion may have extra ces
            // if we are in the normalization buffer, hasAccentsBeforeMatch
            // would have taken care of it.
            // E.g. the character \u01FA will have an expansion of 3, but if
            // we are only looking for A ring A\u030A, we'll have to skip the
            // last ce in the expansion buffer
            ucol_previous(coleiter, status);
            if (U_FAILURE(*status)) {
                return FALSE;
            }
            if (ucol_getOffset(coleiter) != temp) {
                *end = temp;
                temp  = ucol_getOffset(coleiter);
            }
            expansion --;
        }

        int32_t  *patternce       = strsrch->pattern.ces;
        int32_t   patterncelength = strsrch->pattern.cesLength;
        int32_t   count           = patterncelength;
        while (count > 0) {
            int32_t ce = getCE(strsrch, ucol_previous(coleiter, status));
            // status checked below, note that if status is a failure
            // ucol_previous returns UCOL_NULLORDER
            if (ce == UCOL_IGNORABLE) {
                continue;
            }
            if (expandflag && count == 0 &&
                getColElemIterOffset(coleiter, FALSE) != temp) {
                *end = temp;
                temp  = ucol_getOffset(coleiter);
            }
            if (count == patterncelength &&
                ce != patternce[patterncelength - 1]) {
                // accents may have extra starting ces, this occurs when a
                // pure accent pattern is matched without rearrangement
                int32_t    expected = patternce[patterncelength - 1];
                U16_BACK_1(text, 0, *end);
                if (getFCD(text, end, textlength) & LAST_BYTE_MASK_) {
                    ce = getCE(strsrch, ucol_previous(coleiter, status));
                    while (U_SUCCESS(*status) && ce != expected &&
                           ce != UCOL_NULLORDER &&
                           ucol_getOffset(coleiter) <= *start) {
                        ce = getCE(strsrch, ucol_previous(coleiter, status));
                    }
                }
            }
            if (U_FAILURE(*status) || ce != patternce[count - 1]) {
                (*start) --;
                *start = getPreviousBaseOffset(text, *start);
                return FALSE;
            }
            count --;
        }
    }
    return TRUE;
}

/**
* Checks and sets the match information if found.
* Checks
* <ul>
* <li> the potential match does not repeat the previous match
* <li> boundaries are correct
* <li> potential match does not end in the middle of a contraction
* <li> identical matches
* <\ul>
* Otherwise the offset will be shifted to the next character.
* Internal method, status assumed to be success, caller has to check status
* before calling this method.
* @param strsrch string search data
* @param textoffset offset in the collation element text. the returned value
*        will be the truncated start offset of the match or the new start
*        search offset.
* @param status only error status if any
* @return TRUE if the match is valid, FALSE otherwise
*/
static
inline UBool checkPreviousCanonicalMatch(UStringSearch *strsrch,
                                         int32_t   *textoffset,
                                         UErrorCode    *status)
{
    // to ensure that the start and ends are not composite characters
    UCollationElements *coleiter = strsrch->textIter;
    // if we have a canonical accent match
    if ((strsrch->pattern.hasSuffixAccents &&
        strsrch->canonicalSuffixAccents[0]) ||
        (strsrch->pattern.hasPrefixAccents &&
        strsrch->canonicalPrefixAccents[0])) {
        strsrch->search->matchedIndex  = *textoffset;
        strsrch->search->matchedLength =
            getNextUStringSearchBaseOffset(strsrch,
                                      getColElemIterOffset(coleiter, FALSE))
            - *textoffset;
        return TRUE;
    }

    int32_t end = ucol_getOffset(coleiter);
    if (!checkPreviousCanonicalContractionMatch(strsrch, textoffset, &end,
                                                status) ||
         U_FAILURE(*status)) {
        return FALSE;
    }

    end = getNextUStringSearchBaseOffset(strsrch, end);
    // this totally matches, however we need to check if it is repeating
    if (checkRepeatedMatch(strsrch, *textoffset, end) ||
        !isBreakUnit(strsrch, *textoffset, end) ||
        !checkIdentical(strsrch, *textoffset, end)) {
        (*textoffset) --;
        *textoffset = getPreviousBaseOffset(strsrch->search->text,
                                            *textoffset);
        return FALSE;
    }

    strsrch->search->matchedIndex  = *textoffset;
    strsrch->search->matchedLength = end - *textoffset;
    return TRUE;
}
#endif // #if BOYER_MOORE

// constructors and destructor -------------------------------------------

U_CAPI UStringSearch * U_EXPORT2 usearch_open(const UChar *pattern,
                                          int32_t         patternlength,
                                    const UChar          *text,
                                          int32_t         textlength,
                                    const char           *locale,
                                          UBreakIterator *breakiter,
                                          UErrorCode     *status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
#if UCONFIG_NO_BREAK_ITERATION
    if (breakiter != NULL) {
        *status = U_UNSUPPORTED_ERROR;
        return NULL;
    }
#endif
    if (locale) {
        // ucol_open internally checks for status
        UCollator     *collator = ucol_open(locale, status);
        // pattern, text checks are done in usearch_openFromCollator
        UStringSearch *result   = usearch_openFromCollator(pattern,
                                              patternlength, text, textlength,
                                              collator, breakiter, status);

        if (result == NULL || U_FAILURE(*status)) {
            if (collator) {
                ucol_close(collator);
            }
            return NULL;
        }
        else {
            result->ownCollator = TRUE;
        }
        return result;
    }
    *status = U_ILLEGAL_ARGUMENT_ERROR;
    return NULL;
}

U_CAPI UStringSearch * U_EXPORT2 usearch_openFromCollator(
                                  const UChar          *pattern,
                                        int32_t         patternlength,
                                  const UChar          *text,
                                        int32_t         textlength,
                                  const UCollator      *collator,
                                        UBreakIterator *breakiter,
                                        UErrorCode     *status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
#if UCONFIG_NO_BREAK_ITERATION
    if (breakiter != NULL) {
        *status = U_UNSUPPORTED_ERROR;
        return NULL;
    }
#endif
    if (pattern == NULL || text == NULL || collator == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    // string search does not really work when numeric collation is turned on
    if(ucol_getAttribute(collator, UCOL_NUMERIC_COLLATION, status) == UCOL_ON) {
        *status = U_UNSUPPORTED_ERROR;
        return NULL;
    }

    if (U_SUCCESS(*status)) {
        initializeFCD(status);
        if (U_FAILURE(*status)) {
            return NULL;
        }

        UStringSearch *result;
        if (textlength == -1) {
            textlength = u_strlen(text);
        }
        if (patternlength == -1) {
            patternlength = u_strlen(pattern);
        }
        if (textlength <= 0 || patternlength <= 0) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return NULL;
        }

        result = (UStringSearch *)uprv_malloc(sizeof(UStringSearch));
        if (result == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }

        result->collator    = collator;
        result->strength    = ucol_getStrength(collator);
        result->ceMask      = getMask(result->strength);
        result->toShift     =
             ucol_getAttribute(collator, UCOL_ALTERNATE_HANDLING, status) ==
                                                            UCOL_SHIFTED;
        result->variableTop = ucol_getVariableTop(collator, status);

        result->nfd         = Normalizer2::getNFDInstance(*status);

        if (U_FAILURE(*status)) {
            uprv_free(result);
            return NULL;
        }

        result->search             = (USearch *)uprv_malloc(sizeof(USearch));
        if (result->search == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            uprv_free(result);
            return NULL;
        }

        result->search->text       = text;
        result->search->textLength = textlength;

        result->pattern.text       = pattern;
        result->pattern.textLength = patternlength;
        result->pattern.ces         = NULL;
        result->pattern.pces        = NULL;

        result->search->breakIter  = breakiter;
#if !UCONFIG_NO_BREAK_ITERATION
        result->search->internalBreakIter = ubrk_open(UBRK_CHARACTER, ucol_getLocaleByType(result->collator, ULOC_VALID_LOCALE, status), text, textlength, status);
        if (breakiter) {
            ubrk_setText(breakiter, text, textlength, status);
        }
#endif

        result->ownCollator           = FALSE;
        result->search->matchedLength = 0;
        result->search->matchedIndex  = USEARCH_DONE;
        result->utilIter              = NULL;
        result->textIter              = ucol_openElements(collator, text,
                                                          textlength, status);
        result->textProcessedIter     = NULL;
        if (U_FAILURE(*status)) {
            usearch_close(result);
            return NULL;
        }

        result->search->isOverlap          = FALSE;
        result->search->isCanonicalMatch   = FALSE;
        result->search->elementComparisonType = 0;
        result->search->isForwardSearching = TRUE;
        result->search->reset              = TRUE;

        initialize(result, status);

        if (U_FAILURE(*status)) {
            usearch_close(result);
            return NULL;
        }

        return result;
    }
    return NULL;
}

U_CAPI void U_EXPORT2 usearch_close(UStringSearch *strsrch)
{
    if (strsrch) {
        if (strsrch->pattern.ces != strsrch->pattern.cesBuffer &&
            strsrch->pattern.ces) {
            uprv_free(strsrch->pattern.ces);
        }

        if (strsrch->pattern.pces != NULL &&
            strsrch->pattern.pces != strsrch->pattern.pcesBuffer) {
            uprv_free(strsrch->pattern.pces);
        }

        delete strsrch->textProcessedIter;
        ucol_closeElements(strsrch->textIter);
        ucol_closeElements(strsrch->utilIter);

        if (strsrch->ownCollator && strsrch->collator) {
            ucol_close((UCollator *)strsrch->collator);
        }

#if !UCONFIG_NO_BREAK_ITERATION
        if (strsrch->search->internalBreakIter) {
            ubrk_close(strsrch->search->internalBreakIter);
        }
#endif

        uprv_free(strsrch->search);
        uprv_free(strsrch);
    }
}

namespace {

UBool initTextProcessedIter(UStringSearch *strsrch, UErrorCode *status) {
    if (U_FAILURE(*status)) { return FALSE; }
    if (strsrch->textProcessedIter == NULL) {
        strsrch->textProcessedIter = new icu::UCollationPCE(strsrch->textIter);
        if (strsrch->textProcessedIter == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return FALSE;
        }
    } else {
        strsrch->textProcessedIter->init(strsrch->textIter);
    }
    return TRUE;
}

}

// set and get methods --------------------------------------------------

U_CAPI void U_EXPORT2 usearch_setOffset(UStringSearch *strsrch,
                                        int32_t    position,
                                        UErrorCode    *status)
{
    if (U_SUCCESS(*status) && strsrch) {
        if (isOutOfBounds(strsrch->search->textLength, position)) {
            *status = U_INDEX_OUTOFBOUNDS_ERROR;
        }
        else {
            setColEIterOffset(strsrch->textIter, position);
        }
        strsrch->search->matchedIndex  = USEARCH_DONE;
        strsrch->search->matchedLength = 0;
        strsrch->search->reset         = FALSE;
    }
}

U_CAPI int32_t U_EXPORT2 usearch_getOffset(const UStringSearch *strsrch)
{
    if (strsrch) {
        int32_t result = ucol_getOffset(strsrch->textIter);
        if (isOutOfBounds(strsrch->search->textLength, result)) {
            return USEARCH_DONE;
        }
        return result;
    }
    return USEARCH_DONE;
}

U_CAPI void U_EXPORT2 usearch_setAttribute(UStringSearch *strsrch,
                                 USearchAttribute attribute,
                                 USearchAttributeValue value,
                                 UErrorCode *status)
{
    if (U_SUCCESS(*status) && strsrch) {
        switch (attribute)
        {
        case USEARCH_OVERLAP :
            strsrch->search->isOverlap = (value == USEARCH_ON ? TRUE : FALSE);
            break;
        case USEARCH_CANONICAL_MATCH :
            strsrch->search->isCanonicalMatch = (value == USEARCH_ON ? TRUE :
                                                                      FALSE);
            break;
        case USEARCH_ELEMENT_COMPARISON :
            if (value == USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD || value == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD) {
                strsrch->search->elementComparisonType = (int16_t)value;
            } else {
                strsrch->search->elementComparisonType = 0;
            }
            break;
        case USEARCH_ATTRIBUTE_COUNT :
        default:
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
    }
    if (value == USEARCH_ATTRIBUTE_VALUE_COUNT) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    }
}

U_CAPI USearchAttributeValue U_EXPORT2 usearch_getAttribute(
                                                const UStringSearch *strsrch,
                                                USearchAttribute attribute)
{
    if (strsrch) {
        switch (attribute) {
        case USEARCH_OVERLAP :
            return (strsrch->search->isOverlap == TRUE ? USEARCH_ON :
                                                        USEARCH_OFF);
        case USEARCH_CANONICAL_MATCH :
            return (strsrch->search->isCanonicalMatch == TRUE ? USEARCH_ON :
                                                               USEARCH_OFF);
        case USEARCH_ELEMENT_COMPARISON :
            {
                int16_t value = strsrch->search->elementComparisonType;
                if (value == USEARCH_PATTERN_BASE_WEIGHT_IS_WILDCARD || value == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD) {
                    return (USearchAttributeValue)value;
                } else {
                    return USEARCH_STANDARD_ELEMENT_COMPARISON;
                }
            }
        case USEARCH_ATTRIBUTE_COUNT :
            return USEARCH_DEFAULT;
        }
    }
    return USEARCH_DEFAULT;
}

U_CAPI int32_t U_EXPORT2 usearch_getMatchedStart(
                                                const UStringSearch *strsrch)
{
    if (strsrch == NULL) {
        return USEARCH_DONE;
    }
    return strsrch->search->matchedIndex;
}


U_CAPI int32_t U_EXPORT2 usearch_getMatchedText(const UStringSearch *strsrch,
                                            UChar         *result,
                                            int32_t        resultCapacity,
                                            UErrorCode    *status)
{
    if (U_FAILURE(*status)) {
        return USEARCH_DONE;
    }
    if (strsrch == NULL || resultCapacity < 0 || (resultCapacity > 0 &&
        result == NULL)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return USEARCH_DONE;
    }

    int32_t     copylength = strsrch->search->matchedLength;
    int32_t copyindex  = strsrch->search->matchedIndex;
    if (copyindex == USEARCH_DONE) {
        u_terminateUChars(result, resultCapacity, 0, status);
        return USEARCH_DONE;
    }

    if (resultCapacity < copylength) {
        copylength = resultCapacity;
    }
    if (copylength > 0) {
        uprv_memcpy(result, strsrch->search->text + copyindex,
                    copylength * sizeof(UChar));
    }
    return u_terminateUChars(result, resultCapacity,
                             strsrch->search->matchedLength, status);
}

U_CAPI int32_t U_EXPORT2 usearch_getMatchedLength(
                                              const UStringSearch *strsrch)
{
    if (strsrch) {
        return strsrch->search->matchedLength;
    }
    return USEARCH_DONE;
}

#if !UCONFIG_NO_BREAK_ITERATION

U_CAPI void U_EXPORT2 usearch_setBreakIterator(UStringSearch  *strsrch,
                                               UBreakIterator *breakiter,
                                               UErrorCode     *status)
{
    if (U_SUCCESS(*status) && strsrch) {
        strsrch->search->breakIter = breakiter;
        if (breakiter) {
            ubrk_setText(breakiter, strsrch->search->text,
                         strsrch->search->textLength, status);
        }
    }
}

U_CAPI const UBreakIterator* U_EXPORT2
usearch_getBreakIterator(const UStringSearch *strsrch)
{
    if (strsrch) {
        return strsrch->search->breakIter;
    }
    return NULL;
}

#endif

U_CAPI void U_EXPORT2 usearch_setText(      UStringSearch *strsrch,
                                      const UChar         *text,
                                            int32_t        textlength,
                                            UErrorCode    *status)
{
    if (U_SUCCESS(*status)) {
        if (strsrch == NULL || text == NULL || textlength < -1 ||
            textlength == 0) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        else {
            if (textlength == -1) {
                textlength = u_strlen(text);
            }
            strsrch->search->text       = text;
            strsrch->search->textLength = textlength;
            ucol_setText(strsrch->textIter, text, textlength, status);
            strsrch->search->matchedIndex  = USEARCH_DONE;
            strsrch->search->matchedLength = 0;
            strsrch->search->reset         = TRUE;
#if !UCONFIG_NO_BREAK_ITERATION
            if (strsrch->search->breakIter != NULL) {
                ubrk_setText(strsrch->search->breakIter, text,
                             textlength, status);
            }
            ubrk_setText(strsrch->search->internalBreakIter, text, textlength, status);
#endif
        }
    }
}

U_CAPI const UChar * U_EXPORT2 usearch_getText(const UStringSearch *strsrch,
                                                     int32_t       *length)
{
    if (strsrch) {
        *length = strsrch->search->textLength;
        return strsrch->search->text;
    }
    return NULL;
}

U_CAPI void U_EXPORT2 usearch_setCollator(      UStringSearch *strsrch,
                                          const UCollator     *collator,
                                                UErrorCode    *status)
{
    if (U_SUCCESS(*status)) {
        if (collator == NULL) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }

        if (strsrch) {
            delete strsrch->textProcessedIter;
            strsrch->textProcessedIter = NULL;
            ucol_closeElements(strsrch->textIter);
            ucol_closeElements(strsrch->utilIter);
            strsrch->textIter = strsrch->utilIter = NULL;
            if (strsrch->ownCollator && (strsrch->collator != collator)) {
                ucol_close((UCollator *)strsrch->collator);
                strsrch->ownCollator = FALSE;
            }
            strsrch->collator    = collator;
            strsrch->strength    = ucol_getStrength(collator);
            strsrch->ceMask      = getMask(strsrch->strength);
#if !UCONFIG_NO_BREAK_ITERATION
            ubrk_close(strsrch->search->internalBreakIter);
            strsrch->search->internalBreakIter = ubrk_open(UBRK_CHARACTER, ucol_getLocaleByType(collator, ULOC_VALID_LOCALE, status),
                                                     strsrch->search->text, strsrch->search->textLength, status);
#endif
            // if status is a failure, ucol_getAttribute returns UCOL_DEFAULT
            strsrch->toShift     =
               ucol_getAttribute(collator, UCOL_ALTERNATE_HANDLING, status) ==
                                                                UCOL_SHIFTED;
            // if status is a failure, ucol_getVariableTop returns 0
            strsrch->variableTop = ucol_getVariableTop(collator, status);
            strsrch->textIter = ucol_openElements(collator,
                                      strsrch->search->text,
                                      strsrch->search->textLength,
                                      status);
            strsrch->utilIter = ucol_openElements(
                    collator, strsrch->pattern.text, strsrch->pattern.textLength, status);
            // initialize() _after_ setting the iterators for the new collator.
            initialize(strsrch, status);
        }

        // **** are these calls needed?
        // **** we call uprv_init_pce in initializePatternPCETable
        // **** and the CEIBuffer constructor...
#if 0
        uprv_init_pce(strsrch->textIter);
        uprv_init_pce(strsrch->utilIter);
#endif
    }
}

U_CAPI UCollator * U_EXPORT2 usearch_getCollator(const UStringSearch *strsrch)
{
    if (strsrch) {
        return (UCollator *)strsrch->collator;
    }
    return NULL;
}

U_CAPI void U_EXPORT2 usearch_setPattern(      UStringSearch *strsrch,
                                         const UChar         *pattern,
                                               int32_t        patternlength,
                                               UErrorCode    *status)
{
    if (U_SUCCESS(*status)) {
        if (strsrch == NULL || pattern == NULL) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        else {
            if (patternlength == -1) {
                patternlength = u_strlen(pattern);
            }
            if (patternlength == 0) {
                *status = U_ILLEGAL_ARGUMENT_ERROR;
                return;
            }
            strsrch->pattern.text       = pattern;
            strsrch->pattern.textLength = patternlength;
            initialize(strsrch, status);
        }
    }
}

U_CAPI const UChar* U_EXPORT2
usearch_getPattern(const UStringSearch *strsrch,
                   int32_t       *length)
{
    if (strsrch) {
        *length = strsrch->pattern.textLength;
        return strsrch->pattern.text;
    }
    return NULL;
}

// miscellanous methods --------------------------------------------------

U_CAPI int32_t U_EXPORT2 usearch_first(UStringSearch *strsrch,
                                           UErrorCode    *status)
{
    if (strsrch && U_SUCCESS(*status)) {
        strsrch->search->isForwardSearching = TRUE;
        usearch_setOffset(strsrch, 0, status);
        if (U_SUCCESS(*status)) {
            return usearch_next(strsrch, status);
        }
    }
    return USEARCH_DONE;
}

U_CAPI int32_t U_EXPORT2 usearch_following(UStringSearch *strsrch,
                                               int32_t    position,
                                               UErrorCode    *status)
{
    if (strsrch && U_SUCCESS(*status)) {
        strsrch->search->isForwardSearching = TRUE;
        // position checked in usearch_setOffset
        usearch_setOffset(strsrch, position, status);
        if (U_SUCCESS(*status)) {
            return usearch_next(strsrch, status);
        }
    }
    return USEARCH_DONE;
}

U_CAPI int32_t U_EXPORT2 usearch_last(UStringSearch *strsrch,
                                          UErrorCode    *status)
{
    if (strsrch && U_SUCCESS(*status)) {
        strsrch->search->isForwardSearching = FALSE;
        usearch_setOffset(strsrch, strsrch->search->textLength, status);
        if (U_SUCCESS(*status)) {
            return usearch_previous(strsrch, status);
        }
    }
    return USEARCH_DONE;
}

U_CAPI int32_t U_EXPORT2 usearch_preceding(UStringSearch *strsrch,
                                               int32_t    position,
                                               UErrorCode    *status)
{
    if (strsrch && U_SUCCESS(*status)) {
        strsrch->search->isForwardSearching = FALSE;
        // position checked in usearch_setOffset
        usearch_setOffset(strsrch, position, status);
        if (U_SUCCESS(*status)) {
            return usearch_previous(strsrch, status);
        }
    }
    return USEARCH_DONE;
}

/**
* If a direction switch is required, we'll count the number of ces till the
* beginning of the collation element iterator and iterate forwards that
* number of times. This is so that we get to the correct point within the
* string to continue the search in. Imagine when we are in the middle of the
* normalization buffer when the change in direction is request. arrrgghh....
* After searching the offset within the collation element iterator will be
* shifted to the start of the match. If a match is not found, the offset would
* have been set to the end of the text string in the collation element
* iterator.
* Okay, here's my take on normalization buffer. The only time when there can
* be 2 matches within the same normalization is when the pattern is consists
* of all accents. But since the offset returned is from the text string, we
* should not confuse the caller by returning the second match within the
* same normalization buffer. If we do, the 2 results will have the same match
* offsets, and that'll be confusing. I'll return the next match that doesn't
* fall within the same normalization buffer. Note this does not affect the
* results of matches spanning the text and the normalization buffer.
* The position to start searching is taken from the collation element
* iterator. Callers of this API would have to set the offset in the collation
* element iterator before using this method.
*/
U_CAPI int32_t U_EXPORT2 usearch_next(UStringSearch *strsrch,
                                          UErrorCode    *status)
{
    if (U_SUCCESS(*status) && strsrch) {
        // note offset is either equivalent to the start of the previous match
        // or is set by the user
        int32_t      offset       = usearch_getOffset(strsrch);
        USearch     *search       = strsrch->search;
        search->reset             = FALSE;
        int32_t      textlength   = search->textLength;
        if (search->isForwardSearching) {
#if BOYER_MOORE
            if (offset == textlength
                || (!search->isOverlap &&
                    (offset + strsrch->pattern.defaultShiftSize > textlength ||
                    (search->matchedIndex != USEARCH_DONE &&
                     offset + search->matchedLength >= textlength)))) {
                // not enough characters to match
                setMatchNotFound(strsrch);
                return USEARCH_DONE;
            }
#else
            if (offset == textlength ||
                (! search->isOverlap &&
                (search->matchedIndex != USEARCH_DONE &&
                offset + search->matchedLength > textlength))) {
                    // not enough characters to match
                    setMatchNotFound(strsrch);
                    return USEARCH_DONE;
            }
#endif
        }
        else {
            // switching direction.
            // if matchedIndex == USEARCH_DONE, it means that either a
            // setOffset has been called or that previous ran off the text
            // string. the iterator would have been set to offset 0 if a
            // match is not found.
            search->isForwardSearching = TRUE;
            if (search->matchedIndex != USEARCH_DONE) {
                // there's no need to set the collation element iterator
                // the next call to next will set the offset.
                return search->matchedIndex;
            }
        }

        if (U_SUCCESS(*status)) {
            if (strsrch->pattern.cesLength == 0) {
                if (search->matchedIndex == USEARCH_DONE) {
                    search->matchedIndex = offset;
                }
                else { // moves by codepoints
                    U16_FWD_1(search->text, search->matchedIndex, textlength);
                }

                search->matchedLength = 0;
                setColEIterOffset(strsrch->textIter, search->matchedIndex);
                // status checked below
                if (search->matchedIndex == textlength) {
                    search->matchedIndex = USEARCH_DONE;
                }
            }
            else {
                if (search->matchedLength > 0) {
                    // if matchlength is 0 we are at the start of the iteration
                    if (search->isOverlap) {
                        ucol_setOffset(strsrch->textIter, offset + 1, status);
                    }
                    else {
                        ucol_setOffset(strsrch->textIter,
                                       offset + search->matchedLength, status);
                    }
                }
                else {
                    // for boundary check purposes. this will ensure that the
                    // next match will not preceed the current offset
                    // note search->matchedIndex will always be set to something
                    // in the code
                    search->matchedIndex = offset - 1;
                }

                if (search->isCanonicalMatch) {
                    // can't use exact here since extra accents are allowed.
                    usearch_handleNextCanonical(strsrch, status);
                }
                else {
                    usearch_handleNextExact(strsrch, status);
                }
            }

            if (U_FAILURE(*status)) {
                return USEARCH_DONE;
            }

#if !BOYER_MOORE
            if (search->matchedIndex == USEARCH_DONE) {
                ucol_setOffset(strsrch->textIter, search->textLength, status);
            } else {
                ucol_setOffset(strsrch->textIter, search->matchedIndex, status);
            }
#endif

            return search->matchedIndex;
        }
    }
    return USEARCH_DONE;
}

U_CAPI int32_t U_EXPORT2 usearch_previous(UStringSearch *strsrch,
                                              UErrorCode *status)
{
    if (U_SUCCESS(*status) && strsrch) {
        int32_t offset;
        USearch *search = strsrch->search;
        if (search->reset) {
            offset                     = search->textLength;
            search->isForwardSearching = FALSE;
            search->reset              = FALSE;
            setColEIterOffset(strsrch->textIter, offset);
        }
        else {
            offset = usearch_getOffset(strsrch);
        }

        int32_t matchedindex = search->matchedIndex;
        if (search->isForwardSearching == TRUE) {
            // switching direction.
            // if matchedIndex == USEARCH_DONE, it means that either a
            // setOffset has been called or that next ran off the text
            // string. the iterator would have been set to offset textLength if
            // a match is not found.
            search->isForwardSearching = FALSE;
            if (matchedindex != USEARCH_DONE) {
                return matchedindex;
            }
        }
        else {
#if BOYER_MOORE
            if (offset == 0 || matchedindex == 0 ||
                (!search->isOverlap &&
                    (offset < strsrch->pattern.defaultShiftSize ||
                    (matchedindex != USEARCH_DONE &&
                    matchedindex < strsrch->pattern.defaultShiftSize)))) {
                // not enough characters to match
                setMatchNotFound(strsrch);
                return USEARCH_DONE;
            }
#else
            // Could check pattern length, but the
            // linear search will do the right thing
            if (offset == 0 || matchedindex == 0) {
                setMatchNotFound(strsrch);
                return USEARCH_DONE;
            }
#endif
        }

        if (U_SUCCESS(*status)) {
            if (strsrch->pattern.cesLength == 0) {
                search->matchedIndex =
                      (matchedindex == USEARCH_DONE ? offset : matchedindex);
                if (search->matchedIndex == 0) {
                    setMatchNotFound(strsrch);
                    // status checked below
                }
                else { // move by codepoints
                    U16_BACK_1(search->text, 0, search->matchedIndex);
                    setColEIterOffset(strsrch->textIter, search->matchedIndex);
                    // status checked below
                    search->matchedLength = 0;
                }
            }
            else {
                if (strsrch->search->isCanonicalMatch) {
                    // can't use exact here since extra accents are allowed.
                    usearch_handlePreviousCanonical(strsrch, status);
                    // status checked below
                }
                else {
                    usearch_handlePreviousExact(strsrch, status);
                    // status checked below
                }
            }

            if (U_FAILURE(*status)) {
                return USEARCH_DONE;
            }

            return search->matchedIndex;
        }
    }
    return USEARCH_DONE;
}



U_CAPI void U_EXPORT2 usearch_reset(UStringSearch *strsrch)
{
    /*
    reset is setting the attributes that are already in
    string search, hence all attributes in the collator should
    be retrieved without any problems
    */
    if (strsrch) {
        UErrorCode status            = U_ZERO_ERROR;
        UBool      sameCollAttribute = TRUE;
        uint32_t   ceMask;
        UBool      shift;
        uint32_t   varTop;

        // **** hack to deal w/ how processed CEs encode quaternary ****
        UCollationStrength newStrength = ucol_getStrength(strsrch->collator);
        if ((strsrch->strength < UCOL_QUATERNARY && newStrength >= UCOL_QUATERNARY) ||
            (strsrch->strength >= UCOL_QUATERNARY && newStrength < UCOL_QUATERNARY)) {
                sameCollAttribute = FALSE;
        }

        strsrch->strength    = ucol_getStrength(strsrch->collator);
        ceMask = getMask(strsrch->strength);
        if (strsrch->ceMask != ceMask) {
            strsrch->ceMask = ceMask;
            sameCollAttribute = FALSE;
        }

        // if status is a failure, ucol_getAttribute returns UCOL_DEFAULT
        shift = ucol_getAttribute(strsrch->collator, UCOL_ALTERNATE_HANDLING,
                                  &status) == UCOL_SHIFTED;
        if (strsrch->toShift != shift) {
            strsrch->toShift  = shift;
            sameCollAttribute = FALSE;
        }

        // if status is a failure, ucol_getVariableTop returns 0
        varTop = ucol_getVariableTop(strsrch->collator, &status);
        if (strsrch->variableTop != varTop) {
            strsrch->variableTop = varTop;
            sameCollAttribute    = FALSE;
        }
        if (!sameCollAttribute) {
            initialize(strsrch, &status);
        }
        ucol_setText(strsrch->textIter, strsrch->search->text,
                              strsrch->search->textLength,
                              &status);
        strsrch->search->matchedLength      = 0;
        strsrch->search->matchedIndex       = USEARCH_DONE;
        strsrch->search->isOverlap          = FALSE;
        strsrch->search->isCanonicalMatch   = FALSE;
        strsrch->search->elementComparisonType = 0;
        strsrch->search->isForwardSearching = TRUE;
        strsrch->search->reset              = TRUE;
    }
}

//
//  CEI  Collation Element + source text index.
//       These structs are kept in the circular buffer.
//
struct  CEI {
    int64_t ce;
    int32_t lowIndex;
    int32_t highIndex;
};

U_NAMESPACE_BEGIN

namespace {
//
//  CEIBuffer   A circular buffer of CEs-with-index from the text being searched.
//
#define   DEFAULT_CEBUFFER_SIZE 96
#define   CEBUFFER_EXTRA 32
// Some typical max values to make buffer size more reasonable for asymmetric search.
// #8694 is for a better long-term solution to allocation of this buffer.
#define   MAX_TARGET_IGNORABLES_PER_PAT_JAMO_L 8
#define   MAX_TARGET_IGNORABLES_PER_PAT_OTHER 3
#define   MIGHT_BE_JAMO_L(c) ((c >= 0x1100 && c <= 0x115E) || (c >= 0x3131 && c <= 0x314E) || (c >= 0x3165 && c <= 0x3186))
struct CEIBuffer {
    CEI                  defBuf[DEFAULT_CEBUFFER_SIZE];
    CEI                 *buf;
    int32_t              bufSize;
    int32_t              firstIx;
    int32_t              limitIx;
    UCollationElements  *ceIter;
    UStringSearch       *strSearch;



               CEIBuffer(UStringSearch *ss, UErrorCode *status);
               ~CEIBuffer();
   const CEI   *get(int32_t index);
   const CEI   *getPrevious(int32_t index);
};


CEIBuffer::CEIBuffer(UStringSearch *ss, UErrorCode *status) {
    buf = defBuf;
    strSearch = ss;
    bufSize = ss->pattern.pcesLength + CEBUFFER_EXTRA;
    if (ss->search->elementComparisonType != 0) {
        const UChar * patText = ss->pattern.text;
        if (patText) {
            const UChar * patTextLimit = patText + ss->pattern.textLength;
            while ( patText < patTextLimit ) {
                UChar c = *patText++;
                if (MIGHT_BE_JAMO_L(c)) {
                    bufSize += MAX_TARGET_IGNORABLES_PER_PAT_JAMO_L;
                } else {
                    // No check for surrogates, we might allocate slightly more buffer than necessary.
                    bufSize += MAX_TARGET_IGNORABLES_PER_PAT_OTHER;
                }
            }
        }
    }
    ceIter    = ss->textIter;
    firstIx = 0;
    limitIx = 0;

    if (!initTextProcessedIter(ss, status)) { return; }

    if (bufSize>DEFAULT_CEBUFFER_SIZE) {
        buf = (CEI *)uprv_malloc(bufSize * sizeof(CEI));
        if (buf == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
}

// TODO: add a reset or init function so that allocated
//       buffers can be retained & reused.

CEIBuffer::~CEIBuffer() {
    if (buf != defBuf) {
        uprv_free(buf);
    }
}


// Get the CE with the specified index.
//   Index must be in the range
//          n-history_size < index < n+1
//   where n is the largest index to have been fetched by some previous call to this function.
//   The CE value will be UCOL__PROCESSED_NULLORDER at end of input.
//
const CEI *CEIBuffer::get(int32_t index) {
    int i = index % bufSize;

    if (index>=firstIx && index<limitIx) {
        // The request was for an entry already in our buffer.
        //  Just return it.
        return &buf[i];
    }

    // Caller is requesting a new, never accessed before, CE.
    //   Verify that it is the next one in sequence, which is all
    //   that is allowed.
    if (index != limitIx) {
        U_ASSERT(FALSE);

        return NULL;
    }

    // Manage the circular CE buffer indexing
    limitIx++;

    if (limitIx - firstIx >= bufSize) {
        // The buffer is full, knock out the lowest-indexed entry.
        firstIx++;
    }

    UErrorCode status = U_ZERO_ERROR;

    buf[i].ce = strSearch->textProcessedIter->nextProcessed(&buf[i].lowIndex, &buf[i].highIndex, &status);

    return &buf[i];
}

// Get the CE with the specified index.
//   Index must be in the range
//          n-history_size < index < n+1
//   where n is the largest index to have been fetched by some previous call to this function.
//   The CE value will be UCOL__PROCESSED_NULLORDER at end of input.
//
const CEI *CEIBuffer::getPrevious(int32_t index) {
    int i = index % bufSize;

    if (index>=firstIx && index<limitIx) {
        // The request was for an entry already in our buffer.
        //  Just return it.
        return &buf[i];
    }

    // Caller is requesting a new, never accessed before, CE.
    //   Verify that it is the next one in sequence, which is all
    //   that is allowed.
    if (index != limitIx) {
        U_ASSERT(FALSE);

        return NULL;
    }

    // Manage the circular CE buffer indexing
    limitIx++;

    if (limitIx - firstIx >= bufSize) {
        // The buffer is full, knock out the lowest-indexed entry.
        firstIx++;
    }

    UErrorCode status = U_ZERO_ERROR;

    buf[i].ce = strSearch->textProcessedIter->previousProcessed(&buf[i].lowIndex, &buf[i].highIndex, &status);

    return &buf[i];
}

}

U_NAMESPACE_END


// #define USEARCH_DEBUG

#ifdef USEARCH_DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

/*
 * Find the next break boundary after startIndex. If the UStringSearch object
 * has an external break iterator, use that. Otherwise use the internal character
 * break iterator.
 */
static int32_t nextBoundaryAfter(UStringSearch *strsrch, int32_t startIndex) {
#if 0
    const UChar *text = strsrch->search->text;
    int32_t textLen   = strsrch->search->textLength;

    U_ASSERT(startIndex>=0);
    U_ASSERT(startIndex<=textLen);

    if (startIndex >= textLen) {
        return startIndex;
    }

    UChar32  c;
    int32_t  i = startIndex;
    U16_NEXT(text, i, textLen, c);

    // If we are on a control character, stop without looking for combining marks.
    //    Control characters do not combine.
    int32_t gcProperty = u_getIntPropertyValue(c, UCHAR_GRAPHEME_CLUSTER_BREAK);
    if (gcProperty==U_GCB_CONTROL || gcProperty==U_GCB_LF || gcProperty==U_GCB_CR) {
        return i;
    }

    // The initial character was not a control, and can thus accept trailing
    //   combining characters.  Advance over however many of them there are.
    int32_t  indexOfLastCharChecked;
    for (;;) {
        indexOfLastCharChecked = i;
        if (i>=textLen) {
            break;
        }
        U16_NEXT(text, i, textLen, c);
        gcProperty = u_getIntPropertyValue(c, UCHAR_GRAPHEME_CLUSTER_BREAK);
        if (gcProperty != U_GCB_EXTEND && gcProperty != U_GCB_SPACING_MARK) {
            break;
        }
    }
    return indexOfLastCharChecked;
#elif !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *breakiterator = strsrch->search->breakIter;

    if (breakiterator == NULL) {
        breakiterator = strsrch->search->internalBreakIter;
    }

    if (breakiterator != NULL) {
        return ubrk_following(breakiterator, startIndex);
    }

    return startIndex;
#else
    // **** or should we use the original code? ****
    return startIndex;
#endif

}

/*
 * Returns TRUE if index is on a break boundary. If the UStringSearch
 * has an external break iterator, test using that, otherwise test
 * using the internal character break iterator.
 */
static UBool isBreakBoundary(UStringSearch *strsrch, int32_t index) {
#if 0
    const UChar *text = strsrch->search->text;
    int32_t textLen   = strsrch->search->textLength;

    U_ASSERT(index>=0);
    U_ASSERT(index<=textLen);

    if (index>=textLen || index<=0) {
        return TRUE;
    }

    // If the character at the current index is not a GRAPHEME_EXTEND
    //    then we can not be within a combining sequence.
    UChar32  c;
    U16_GET(text, 0, index, textLen, c);
    int32_t gcProperty = u_getIntPropertyValue(c, UCHAR_GRAPHEME_CLUSTER_BREAK);
    if (gcProperty != U_GCB_EXTEND && gcProperty != U_GCB_SPACING_MARK) {
        return TRUE;
    }

    // We are at a combining mark.  If the preceding character is anything
    //   except a CONTROL, CR or LF, we are in a combining sequence.
    U16_PREV(text, 0, index, c);
    gcProperty = u_getIntPropertyValue(c, UCHAR_GRAPHEME_CLUSTER_BREAK);
    UBool combining =  !(gcProperty==U_GCB_CONTROL || gcProperty==U_GCB_LF || gcProperty==U_GCB_CR);
    return !combining;
#elif !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *breakiterator = strsrch->search->breakIter;

    if (breakiterator == NULL) {
        breakiterator = strsrch->search->internalBreakIter;
    }

    return (breakiterator != NULL && ubrk_isBoundary(breakiterator, index));
#else
    // **** or use the original code? ****
    return TRUE;
#endif
}

#if 0
static UBool onBreakBoundaries(const UStringSearch *strsrch, int32_t start, int32_t end)
{
#if !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *breakiterator = strsrch->search->breakIter;

    if (breakiterator != NULL) {
        int32_t startindex = ubrk_first(breakiterator);
        int32_t endindex   = ubrk_last(breakiterator);

        // out-of-range indexes are never boundary positions
        if (start < startindex || start > endindex ||
            end < startindex || end > endindex) {
            return FALSE;
        }

        return ubrk_isBoundary(breakiterator, start) &&
               ubrk_isBoundary(breakiterator, end);
    }
#endif

    return TRUE;
}
#endif

typedef enum {
    U_CE_MATCH = -1,
    U_CE_NO_MATCH = 0,
    U_CE_SKIP_TARG,
    U_CE_SKIP_PATN
} UCompareCEsResult;
#define U_CE_LEVEL2_BASE 0x00000005
#define U_CE_LEVEL3_BASE 0x00050000

static UCompareCEsResult compareCE64s(int64_t targCE, int64_t patCE, int16_t compareType) {
    if (targCE == patCE) {
        return U_CE_MATCH;
    }
    if (compareType == 0) {
        return U_CE_NO_MATCH;
    }
    
    int64_t targCEshifted = targCE >> 32;
    int64_t patCEshifted = patCE >> 32;
    int64_t mask;

    mask = 0xFFFF0000;
    int32_t targLev1 = (int32_t)(targCEshifted & mask);
    int32_t patLev1 = (int32_t)(patCEshifted & mask);
    if ( targLev1 != patLev1 ) {
        if ( targLev1 == 0 ) {
            return U_CE_SKIP_TARG;
        }
        if ( patLev1 == 0 && compareType == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD ) {
            return U_CE_SKIP_PATN;
        }
        return U_CE_NO_MATCH;
    }

    mask = 0x0000FFFF;
    int32_t targLev2 = (int32_t)(targCEshifted & mask);
    int32_t patLev2 = (int32_t)(patCEshifted & mask);
    if ( targLev2 != patLev2 ) {
        if ( targLev2 == 0 ) {
            return U_CE_SKIP_TARG;
        }
        if ( patLev2 == 0 && compareType == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD ) {
            return U_CE_SKIP_PATN;
        }
        return (patLev2 == U_CE_LEVEL2_BASE || (compareType == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD && targLev2 == U_CE_LEVEL2_BASE) )?
            U_CE_MATCH: U_CE_NO_MATCH;
    }
    
    mask = 0xFFFF0000;
    int32_t targLev3 = (int32_t)(targCE & mask);
    int32_t patLev3 = (int32_t)(patCE & mask);
    if ( targLev3 != patLev3 ) {
        return (patLev3 == U_CE_LEVEL3_BASE || (compareType == USEARCH_ANY_BASE_WEIGHT_IS_WILDCARD && targLev3 == U_CE_LEVEL3_BASE) )?
            U_CE_MATCH: U_CE_NO_MATCH;
   }

    return U_CE_MATCH;
}

#if BOYER_MOORE
// TODO: #if BOYER_MOORE, need 32-bit version of compareCE64s
#endif

namespace {

UChar32 codePointAt(const USearch &search, int32_t index) {
    if (index < search.textLength) {
        UChar32 c;
        U16_NEXT(search.text, index, search.textLength, c);
        return c;
    }
    return U_SENTINEL;
}

UChar32 codePointBefore(const USearch &search, int32_t index) {
    if (0 < index) {
        UChar32 c;
        U16_PREV(search.text, 0, index, c);
        return c;
    }
    return U_SENTINEL;
}

}  // namespace

U_CAPI UBool U_EXPORT2 usearch_search(UStringSearch  *strsrch,
                                       int32_t        startIdx,
                                       int32_t        *matchStart,
                                       int32_t        *matchLimit,
                                       UErrorCode     *status)
{
    if (U_FAILURE(*status)) {
        return FALSE;
    }

    // TODO:  reject search patterns beginning with a combining char.

#ifdef USEARCH_DEBUG
    if (getenv("USEARCH_DEBUG") != NULL) {
        printf("Pattern CEs\n");
        for (int ii=0; ii<strsrch->pattern.cesLength; ii++) {
            printf(" %8x", strsrch->pattern.ces[ii]);
        }
        printf("\n");
    }

#endif
    // Input parameter sanity check.
    //  TODO:  should input indicies clip to the text length
    //         in the same way that UText does.
    if(strsrch->pattern.cesLength == 0         ||
       startIdx < 0                           ||
       startIdx > strsrch->search->textLength ||
       strsrch->pattern.ces == NULL) {
           *status = U_ILLEGAL_ARGUMENT_ERROR;
           return FALSE;
    }

    if (strsrch->pattern.pces == NULL) {
        initializePatternPCETable(strsrch, status);
    }

    ucol_setOffset(strsrch->textIter, startIdx, status);
    CEIBuffer ceb(strsrch, status);


    int32_t    targetIx = 0;
    const CEI *targetCEI = NULL;
    int32_t    patIx;
    UBool      found;

    int32_t  mStart = -1;
    int32_t  mLimit = -1;
    int32_t  minLimit;
    int32_t  maxLimit;



    // Outer loop moves over match starting positions in the
    //      target CE space.
    // Here we see the target as a sequence of collation elements, resulting from the following:
    // 1. Target characters were decomposed, and (if appropriate) other compressions and expansions are applied
    //    (for example, digraphs such as IJ may be broken into two characters).
    // 2. An int64_t CE weight is determined for each resulting unit (high 16 bits are primary strength, next
    //    16 bits are secondary, next 16 (the high 16 bits of the low 32-bit half) are tertiary. Any of these
    //    fields that are for strengths below that of the collator are set to 0. If this makes the int64_t
    //    CE weight 0 (as for a combining diacritic with secondary weight when the collator strentgh is primary),
    //    then the CE is deleted, so the following code sees only CEs that are relevant.
    // For each CE, the lowIndex and highIndex correspond to where this CE begins and ends in the original text.
    // If lowIndex==highIndex, either the CE resulted from an expansion/decomposition of one of the original text
    // characters, or the CE marks the limit of the target text (in which case the CE weight is UCOL_PROCESSED_NULLORDER).
    //
    for(targetIx=0; ; targetIx++)
    {
        found = TRUE;
        //  Inner loop checks for a match beginning at each
        //  position from the outer loop.
        int32_t targetIxOffset = 0;
        int64_t patCE = 0;
        // For targetIx > 0, this ceb.get gets a CE that is as far back in the ring buffer
        // (compared to the last CE fetched for the previous targetIx value) as we need to go
        // for this targetIx value, so if it is non-NULL then other ceb.get calls should be OK.
        const CEI *firstCEI = ceb.get(targetIx);
        if (firstCEI == NULL) {
            *status = U_INTERNAL_PROGRAM_ERROR;
            found = FALSE;
            break;
        }
        
        for (patIx=0; patIx<strsrch->pattern.pcesLength; patIx++) {
            patCE = strsrch->pattern.pces[patIx];
            targetCEI = ceb.get(targetIx+patIx+targetIxOffset);
            //  Compare CE from target string with CE from the pattern.
            //    Note that the target CE will be UCOL_PROCESSED_NULLORDER if we reach the end of input,
            //    which will fail the compare, below.
            UCompareCEsResult ceMatch = compareCE64s(targetCEI->ce, patCE, strsrch->search->elementComparisonType);
            if ( ceMatch == U_CE_NO_MATCH ) {
                found = FALSE;
                break;
            } else if ( ceMatch > U_CE_NO_MATCH ) {
                if ( ceMatch == U_CE_SKIP_TARG ) {
                    // redo with same patCE, next targCE
                    patIx--;
                    targetIxOffset++;
                } else { // ceMatch == U_CE_SKIP_PATN
                    // redo with same targCE, next patCE
                    targetIxOffset--;
                }
            }
        }
        targetIxOffset += strsrch->pattern.pcesLength; // this is now the offset in target CE space to end of the match so far

        if (!found && ((targetCEI == NULL) || (targetCEI->ce != UCOL_PROCESSED_NULLORDER))) {
            // No match at this targetIx.  Try again at the next.
            continue;
        }

        if (!found) {
            // No match at all, we have run off the end of the target text.
            break;
        }


        // We have found a match in CE space.
        // Now determine the bounds in string index space.
        //  There still is a chance of match failure if the CE range not correspond to
        //     an acceptable character range.
        //
        const CEI *lastCEI  = ceb.get(targetIx + targetIxOffset - 1);

        mStart   = firstCEI->lowIndex;
        minLimit = lastCEI->lowIndex;

        // Look at the CE following the match.  If it is UCOL_NULLORDER the match
        //   extended to the end of input, and the match is good.

        // Look at the high and low indices of the CE following the match. If
        // they are the same it means one of two things:
        //    1. The match extended to the last CE from the target text, which is OK, or
        //    2. The last CE that was part of the match is in an expansion that extends
        //       to the first CE after the match. In this case, we reject the match.
        const CEI *nextCEI = 0;
        if (strsrch->search->elementComparisonType == 0) {
            nextCEI  = ceb.get(targetIx + targetIxOffset);
            maxLimit = nextCEI->lowIndex;
            if (nextCEI->lowIndex == nextCEI->highIndex && nextCEI->ce != UCOL_PROCESSED_NULLORDER) {
                found = FALSE;
            }
        } else {
            for ( ; ; ++targetIxOffset ) {
                nextCEI = ceb.get(targetIx + targetIxOffset);
                maxLimit = nextCEI->lowIndex;
                // If we are at the end of the target too, match succeeds
                if (  nextCEI->ce == UCOL_PROCESSED_NULLORDER ) {
                    break;
                }
                // As long as the next CE has primary weight of 0,
                // it is part of the last target element matched by the pattern;
                // make sure it can be part of a match with the last patCE
                if ( (((nextCEI->ce) >> 32) & 0xFFFF0000UL) == 0 ) {
                    UCompareCEsResult ceMatch = compareCE64s(nextCEI->ce, patCE, strsrch->search->elementComparisonType);
                    if ( ceMatch == U_CE_NO_MATCH || ceMatch == U_CE_SKIP_PATN ) {
                        found = FALSE;
                        break;
                    }
                // If lowIndex == highIndex, this target CE is part of an expansion of the last matched
                // target element, but it has non-zero primary weight => match fails
                } else if ( nextCEI->lowIndex == nextCEI->highIndex ) {
                    found = false;
                    break;
                // Else the target CE is not part of an expansion of the last matched element, match succeeds
                } else {
                    break;
                }
            }
        }


        // Check for the start of the match being within a combining sequence.
        //   This can happen if the pattern itself begins with a combining char, and
        //   the match found combining marks in the target text that were attached
        //    to something else.
        //   This type of match should be rejected for not completely consuming a
        //   combining sequence.
        if (!isBreakBoundary(strsrch, mStart)) {
            found = FALSE;
        }

        // Check for the start of the match being within an Collation Element Expansion,
        //   meaning that the first char of the match is only partially matched.
        //   With exapnsions, the first CE will report the index of the source
        //   character, and all subsequent (expansions) CEs will report the source index of the
        //    _following_ character.
        int32_t secondIx = firstCEI->highIndex;
        if (mStart == secondIx) {
            found = FALSE;
        }

        // Allow matches to end in the middle of a grapheme cluster if the following
        // conditions are met; this is needed to make prefix search work properly in
        // Indic, see #11750
        // * the default breakIter is being used
        // * the next collation element after this combining sequence
        //   - has non-zero primary weight
        //   - corresponds to a separate character following the one at end of the current match
        //   (the second of these conditions, and perhaps both, may be redundant given the
        //   subsequent check for normalization boundary; however they are likely much faster
        //   tests in any case)
        // * the match limit is a normalization boundary
        UBool allowMidclusterMatch = FALSE;
        if (strsrch->search->text != NULL && strsrch->search->textLength > maxLimit) {
            allowMidclusterMatch =
                    strsrch->search->breakIter == NULL &&
                    nextCEI != NULL && (((nextCEI->ce) >> 32) & 0xFFFF0000UL) != 0 &&
                    maxLimit >= lastCEI->highIndex && nextCEI->highIndex > maxLimit &&
                    (strsrch->nfd->hasBoundaryBefore(codePointAt(*strsrch->search, maxLimit)) ||
                        strsrch->nfd->hasBoundaryAfter(codePointBefore(*strsrch->search, maxLimit)));
        }
        // If those conditions are met, then:
        // * do NOT advance the candidate match limit (mLimit) to a break boundary; however
        //   the match limit may be backed off to a previous break boundary. This handles
        //   cases in which mLimit includes target characters that are ignorable with current
        //   settings (such as space) and which extend beyond the pattern match.
        // * do NOT require that end of the combining sequence not extend beyond the match in CE space
        // * do NOT require that match limit be on a breakIter boundary

        //  Advance the match end position to the first acceptable match boundary.
        //    This advances the index over any combining charcters.
        mLimit = maxLimit;
        if (minLimit < maxLimit) {
            // When the last CE's low index is same with its high index, the CE is likely
            // a part of expansion. In this case, the index is located just after the
            // character corresponding to the CEs compared above. If the index is right
            // at the break boundary, move the position to the next boundary will result
            // incorrect match length when there are ignorable characters exist between
            // the position and the next character produces CE(s). See ticket#8482.
            if (minLimit == lastCEI->highIndex && isBreakBoundary(strsrch, minLimit)) {
                mLimit = minLimit;
            } else {
                int32_t nba = nextBoundaryAfter(strsrch, minLimit);
                // Note that we can have nba < maxLimit && nba >= minLImit, in which
                // case we want to set mLimit to nba regardless of allowMidclusterMatch
                // (i.e. we back off mLimit to the previous breakIterator boundary).
                if (nba >= lastCEI->highIndex && (!allowMidclusterMatch || nba < maxLimit)) {
                    mLimit = nba;
                }
            }
        }

    #ifdef USEARCH_DEBUG
        if (getenv("USEARCH_DEBUG") != NULL) {
            printf("minLimit, maxLimit, mLimit = %d, %d, %d\n", minLimit, maxLimit, mLimit);
        }
    #endif

        if (!allowMidclusterMatch) {
            // If advancing to the end of a combining sequence in character indexing space
            //   advanced us beyond the end of the match in CE space, reject this match.
            if (mLimit > maxLimit) {
                found = FALSE;
            }

            if (!isBreakBoundary(strsrch, mLimit)) {
                found = FALSE;
            }
        }

        if (! checkIdentical(strsrch, mStart, mLimit)) {
            found = FALSE;
        }

        if (found) {
            break;
        }
    }

    #ifdef USEARCH_DEBUG
    if (getenv("USEARCH_DEBUG") != NULL) {
        printf("Target CEs [%d .. %d]\n", ceb.firstIx, ceb.limitIx);
        int32_t  lastToPrint = ceb.limitIx+2;
        for (int ii=ceb.firstIx; ii<lastToPrint; ii++) {
            printf("%8x@%d ", ceb.get(ii)->ce, ceb.get(ii)->srcIndex);
        }
        printf("\n%s\n", found? "match found" : "no match");
    }
    #endif

    // All Done.  Store back the match bounds to the caller.
    //
    if (found==FALSE) {
        mLimit = -1;
        mStart = -1;
    }

    if (matchStart != NULL) {
        *matchStart= mStart;
    }

    if (matchLimit != NULL) {
        *matchLimit = mLimit;
    }

    return found;
}

U_CAPI UBool U_EXPORT2 usearch_searchBackwards(UStringSearch  *strsrch,
                                                int32_t        startIdx,
                                                int32_t        *matchStart,
                                                int32_t        *matchLimit,
                                                UErrorCode     *status)
{
    if (U_FAILURE(*status)) {
        return FALSE;
    }

    // TODO:  reject search patterns beginning with a combining char.

#ifdef USEARCH_DEBUG
    if (getenv("USEARCH_DEBUG") != NULL) {
        printf("Pattern CEs\n");
        for (int ii=0; ii<strsrch->pattern.cesLength; ii++) {
            printf(" %8x", strsrch->pattern.ces[ii]);
        }
        printf("\n");
    }

#endif
    // Input parameter sanity check.
    //  TODO:  should input indicies clip to the text length
    //         in the same way that UText does.
    if(strsrch->pattern.cesLength == 0         ||
       startIdx < 0                           ||
       startIdx > strsrch->search->textLength ||
       strsrch->pattern.ces == NULL) {
           *status = U_ILLEGAL_ARGUMENT_ERROR;
           return FALSE;
    }

    if (strsrch->pattern.pces == NULL) {
        initializePatternPCETable(strsrch, status);
    }

    CEIBuffer ceb(strsrch, status);
    int32_t    targetIx = 0;

    /*
     * Pre-load the buffer with the CE's for the grapheme
     * after our starting position so that we're sure that
     * we can look at the CE following the match when we
     * check the match boundaries.
     *
     * This will also pre-fetch the first CE that we'll
     * consider for the match.
     */
    if (startIdx < strsrch->search->textLength) {
        UBreakIterator *bi = strsrch->search->internalBreakIter;
        int32_t next = ubrk_following(bi, startIdx);

        ucol_setOffset(strsrch->textIter, next, status);

        for (targetIx = 0; ; targetIx += 1) {
            if (ceb.getPrevious(targetIx)->lowIndex < startIdx) {
                break;
            }
        }
    } else {
        ucol_setOffset(strsrch->textIter, startIdx, status);
    }


    const CEI *targetCEI = NULL;
    int32_t    patIx;
    UBool      found;

    int32_t  limitIx = targetIx;
    int32_t  mStart = -1;
    int32_t  mLimit = -1;
    int32_t  minLimit;
    int32_t  maxLimit;



    // Outer loop moves over match starting positions in the
    //      target CE space.
    // Here, targetIx values increase toward the beginning of the base text (i.e. we get the text CEs in reverse order).
    // But  patIx is 0 at the beginning of the pattern and increases toward the end.
    // So this loop performs a comparison starting with the end of pattern, and prcessd toward the beginning of the pattern
    // and the beginning of the base text.
    for(targetIx = limitIx; ; targetIx += 1)
    {
        found = TRUE;
        // For targetIx > limitIx, this ceb.getPrevious gets a CE that is as far back in the ring buffer
        // (compared to the last CE fetched for the previous targetIx value) as we need to go
        // for this targetIx value, so if it is non-NULL then other ceb.getPrevious calls should be OK.
        const CEI *lastCEI  = ceb.getPrevious(targetIx);
        if (lastCEI == NULL) {
            *status = U_INTERNAL_PROGRAM_ERROR;
            found = FALSE;
             break;
        }
        //  Inner loop checks for a match beginning at each
        //  position from the outer loop.
        int32_t targetIxOffset = 0;
        for (patIx = strsrch->pattern.pcesLength - 1; patIx >= 0; patIx -= 1) {
            int64_t patCE = strsrch->pattern.pces[patIx];

            targetCEI = ceb.getPrevious(targetIx + strsrch->pattern.pcesLength - 1 - patIx + targetIxOffset);
            //  Compare CE from target string with CE from the pattern.
            //    Note that the target CE will be UCOL_NULLORDER if we reach the end of input,
            //    which will fail the compare, below.
            UCompareCEsResult ceMatch = compareCE64s(targetCEI->ce, patCE, strsrch->search->elementComparisonType);
            if ( ceMatch == U_CE_NO_MATCH ) {
                found = FALSE;
                break;
            } else if ( ceMatch > U_CE_NO_MATCH ) {
                if ( ceMatch == U_CE_SKIP_TARG ) {
                    // redo with same patCE, next targCE
                    patIx++;
                    targetIxOffset++;
                } else { // ceMatch == U_CE_SKIP_PATN
                    // redo with same targCE, next patCE
                    targetIxOffset--;
                }
            }
        }

        if (!found && ((targetCEI == NULL) || (targetCEI->ce != UCOL_PROCESSED_NULLORDER))) {
            // No match at this targetIx.  Try again at the next.
            continue;
        }

        if (!found) {
            // No match at all, we have run off the end of the target text.
            break;
        }


        // We have found a match in CE space.
        // Now determine the bounds in string index space.
        //  There still is a chance of match failure if the CE range not correspond to
        //     an acceptable character range.
        //
        const CEI *firstCEI = ceb.getPrevious(targetIx + strsrch->pattern.pcesLength - 1 + targetIxOffset);
        mStart   = firstCEI->lowIndex;

        // Check for the start of the match being within a combining sequence.
        //   This can happen if the pattern itself begins with a combining char, and
        //   the match found combining marks in the target text that were attached
        //    to something else.
        //   This type of match should be rejected for not completely consuming a
        //   combining sequence.
        if (!isBreakBoundary(strsrch, mStart)) {
            found = FALSE;
        }

        // Look at the high index of the first CE in the match. If it's the same as the
        // low index, the first CE in the match is in the middle of an expansion.
        if (mStart == firstCEI->highIndex) {
            found = FALSE;
        }


        minLimit = lastCEI->lowIndex;

        if (targetIx > 0) {
            // Look at the CE following the match.  If it is UCOL_NULLORDER the match
            //   extended to the end of input, and the match is good.

            // Look at the high and low indices of the CE following the match. If
            // they are the same it means one of two things:
            //    1. The match extended to the last CE from the target text, which is OK, or
            //    2. The last CE that was part of the match is in an expansion that extends
            //       to the first CE after the match. In this case, we reject the match.
            const CEI *nextCEI  = ceb.getPrevious(targetIx - 1);

            if (nextCEI->lowIndex == nextCEI->highIndex && nextCEI->ce != UCOL_PROCESSED_NULLORDER) {
                found = FALSE;
            }

            mLimit = maxLimit = nextCEI->lowIndex;

            // Allow matches to end in the middle of a grapheme cluster if the following
            // conditions are met; this is needed to make prefix search work properly in
            // Indic, see #11750
            // * the default breakIter is being used
            // * the next collation element after this combining sequence
            //   - has non-zero primary weight
            //   - corresponds to a separate character following the one at end of the current match
            //   (the second of these conditions, and perhaps both, may be redundant given the
            //   subsequent check for normalization boundary; however they are likely much faster
            //   tests in any case)
            // * the match limit is a normalization boundary
            UBool allowMidclusterMatch = FALSE;
            if (strsrch->search->text != NULL && strsrch->search->textLength > maxLimit) {
                allowMidclusterMatch =
                        strsrch->search->breakIter == NULL &&
                        nextCEI != NULL && (((nextCEI->ce) >> 32) & 0xFFFF0000UL) != 0 &&
                        maxLimit >= lastCEI->highIndex && nextCEI->highIndex > maxLimit &&
                        (strsrch->nfd->hasBoundaryBefore(codePointAt(*strsrch->search, maxLimit)) ||
                            strsrch->nfd->hasBoundaryAfter(codePointBefore(*strsrch->search, maxLimit)));
            }
            // If those conditions are met, then:
            // * do NOT advance the candidate match limit (mLimit) to a break boundary; however
            //   the match limit may be backed off to a previous break boundary. This handles
            //   cases in which mLimit includes target characters that are ignorable with current
            //   settings (such as space) and which extend beyond the pattern match.
            // * do NOT require that end of the combining sequence not extend beyond the match in CE space
            // * do NOT require that match limit be on a breakIter boundary

            //  Advance the match end position to the first acceptable match boundary.
            //    This advances the index over any combining characters.
            if (minLimit < maxLimit) {
                int32_t nba = nextBoundaryAfter(strsrch, minLimit);
                // Note that we can have nba < maxLimit && nba >= minLImit, in which
                // case we want to set mLimit to nba regardless of allowMidclusterMatch
                // (i.e. we back off mLimit to the previous breakIterator boundary).
                if (nba >= lastCEI->highIndex && (!allowMidclusterMatch || nba < maxLimit)) {
                    mLimit = nba;
                }
            }

            if (!allowMidclusterMatch) {
                // If advancing to the end of a combining sequence in character indexing space
                //   advanced us beyond the end of the match in CE space, reject this match.
                if (mLimit > maxLimit) {
                    found = FALSE;
                }

                // Make sure the end of the match is on a break boundary
                if (!isBreakBoundary(strsrch, mLimit)) {
                    found = FALSE;
                }
            }

        } else {
            // No non-ignorable CEs after this point.
            // The maximum position is detected by boundary after
            // the last non-ignorable CE. Combining sequence
            // across the start index will be truncated.
            int32_t nba = nextBoundaryAfter(strsrch, minLimit);
            mLimit = maxLimit = (nba > 0) && (startIdx > nba) ? nba : startIdx;
        }

    #ifdef USEARCH_DEBUG
        if (getenv("USEARCH_DEBUG") != NULL) {
            printf("minLimit, maxLimit, mLimit = %d, %d, %d\n", minLimit, maxLimit, mLimit);
        }
    #endif


        if (! checkIdentical(strsrch, mStart, mLimit)) {
            found = FALSE;
        }

        if (found) {
            break;
        }
    }

    #ifdef USEARCH_DEBUG
    if (getenv("USEARCH_DEBUG") != NULL) {
        printf("Target CEs [%d .. %d]\n", ceb.firstIx, ceb.limitIx);
        int32_t  lastToPrint = ceb.limitIx+2;
        for (int ii=ceb.firstIx; ii<lastToPrint; ii++) {
            printf("%8x@%d ", ceb.get(ii)->ce, ceb.get(ii)->srcIndex);
        }
        printf("\n%s\n", found? "match found" : "no match");
    }
    #endif

    // All Done.  Store back the match bounds to the caller.
    //
    if (found==FALSE) {
        mLimit = -1;
        mStart = -1;
    }

    if (matchStart != NULL) {
        *matchStart= mStart;
    }

    if (matchLimit != NULL) {
        *matchLimit = mLimit;
    }

    return found;
}

// internal use methods declared in usrchimp.h -----------------------------

UBool usearch_handleNextExact(UStringSearch *strsrch, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        setMatchNotFound(strsrch);
        return FALSE;
    }

#if BOYER_MOORE
    UCollationElements *coleiter        = strsrch->textIter;
    int32_t             textlength      = strsrch->search->textLength;
    int32_t            *patternce       = strsrch->pattern.ces;
    int32_t             patterncelength = strsrch->pattern.cesLength;
    int32_t             textoffset      = ucol_getOffset(coleiter);

    // status used in setting coleiter offset, since offset is checked in
    // shiftForward before setting the coleiter offset, status never
    // a failure
    textoffset = shiftForward(strsrch, textoffset, UCOL_NULLORDER,
                              patterncelength);
    while (textoffset <= textlength)
    {
        uint32_t    patternceindex = patterncelength - 1;
        int32_t     targetce;
        UBool       found          = FALSE;
        int32_t    lastce          = UCOL_NULLORDER;

        setColEIterOffset(coleiter, textoffset);

        for (;;) {
            // finding the last pattern ce match, imagine composite characters
            // for example: search for pattern A in text \u00C0
            // we'll have to skip \u0300 the grave first before we get to A
            targetce = ucol_previous(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce = getCE(strsrch, targetce);
            if (targetce == UCOL_IGNORABLE && inNormBuf(coleiter)) {
                // this is for the text \u0315\u0300 that requires
                // normalization and pattern \u0300, where \u0315 is ignorable
                continue;
            }
            if (lastce == UCOL_NULLORDER || lastce == UCOL_IGNORABLE) {
                lastce = targetce;
            }
            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            if (targetce == patternce[patternceindex]) {
                // the first ce can be a contraction
                found = TRUE;
                break;
            }
            if (!hasExpansion(coleiter)) {
                found = FALSE;
                break;
            }
        }

        //targetce = lastce;

        while (found && patternceindex > 0) {
            lastce = targetce;
            targetce    = ucol_previous(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce    = getCE(strsrch, targetce);
            if (targetce == UCOL_IGNORABLE) {
                continue;
            }

            patternceindex --;
            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            found = found && targetce == patternce[patternceindex];
        }

        targetce = lastce;

        if (!found) {
            if (U_FAILURE(*status)) {
                break;
            }
            textoffset = shiftForward(strsrch, textoffset, lastce,
                                      patternceindex);
            // status checked at loop.
            patternceindex = patterncelength;
            continue;
        }

        if (checkNextExactMatch(strsrch, &textoffset, status)) {
            // status checked in ucol_setOffset
            setColEIterOffset(coleiter, strsrch->search->matchedIndex);
            return TRUE;
        }
    }
    setMatchNotFound(strsrch);
    return FALSE;
#else
    int32_t textOffset = ucol_getOffset(strsrch->textIter);
    int32_t start = -1;
    int32_t end = -1;

    if (usearch_search(strsrch, textOffset, &start, &end, status)) {
        strsrch->search->matchedIndex  = start;
        strsrch->search->matchedLength = end - start;
        return TRUE;
    } else {
        setMatchNotFound(strsrch);
        return FALSE;
    }
#endif
}

UBool usearch_handleNextCanonical(UStringSearch *strsrch, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        setMatchNotFound(strsrch);
        return FALSE;
    }

#if BOYER_MOORE
    UCollationElements *coleiter        = strsrch->textIter;
    int32_t             textlength      = strsrch->search->textLength;
    int32_t            *patternce       = strsrch->pattern.ces;
    int32_t             patterncelength = strsrch->pattern.cesLength;
    int32_t             textoffset      = ucol_getOffset(coleiter);
    UBool               hasPatternAccents =
       strsrch->pattern.hasSuffixAccents || strsrch->pattern.hasPrefixAccents;

    textoffset = shiftForward(strsrch, textoffset, UCOL_NULLORDER,
                              patterncelength);
    strsrch->canonicalPrefixAccents[0] = 0;
    strsrch->canonicalSuffixAccents[0] = 0;

    while (textoffset <= textlength)
    {
        int32_t     patternceindex = patterncelength - 1;
        int32_t     targetce;
        UBool       found          = FALSE;
        int32_t     lastce         = UCOL_NULLORDER;

        setColEIterOffset(coleiter, textoffset);

        for (;;) {
            // finding the last pattern ce match, imagine composite characters
            // for example: search for pattern A in text \u00C0
            // we'll have to skip \u0300 the grave first before we get to A
            targetce = ucol_previous(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce = getCE(strsrch, targetce);
            if (lastce == UCOL_NULLORDER || lastce == UCOL_IGNORABLE) {
                lastce = targetce;
            }
            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            if (targetce == patternce[patternceindex]) {
                // the first ce can be a contraction
                found = TRUE;
                break;
            }
            if (!hasExpansion(coleiter)) {
                found = FALSE;
                break;
            }
        }

        while (found && patternceindex > 0) {
            targetce    = ucol_previous(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce    = getCE(strsrch, targetce);
            if (targetce == UCOL_IGNORABLE) {
                continue;
            }

            patternceindex --;
            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            found = found && targetce == patternce[patternceindex];
        }

        // initializing the rearranged accent array
        if (hasPatternAccents && !found) {
            strsrch->canonicalPrefixAccents[0] = 0;
            strsrch->canonicalSuffixAccents[0] = 0;
            if (U_FAILURE(*status)) {
                break;
            }
            found = doNextCanonicalMatch(strsrch, textoffset, status);
        }

        if (!found) {
            if (U_FAILURE(*status)) {
                break;
            }
            textoffset = shiftForward(strsrch, textoffset, lastce,
                                      patternceindex);
            // status checked at loop
            patternceindex = patterncelength;
            continue;
        }

        if (checkNextCanonicalMatch(strsrch, &textoffset, status)) {
            setColEIterOffset(coleiter, strsrch->search->matchedIndex);
            return TRUE;
        }
    }
    setMatchNotFound(strsrch);
    return FALSE;
#else
    int32_t textOffset = ucol_getOffset(strsrch->textIter);
    int32_t start = -1;
    int32_t end = -1;

    if (usearch_search(strsrch, textOffset, &start, &end, status)) {
        strsrch->search->matchedIndex  = start;
        strsrch->search->matchedLength = end - start;
        return TRUE;
    } else {
        setMatchNotFound(strsrch);
        return FALSE;
    }
#endif
}

UBool usearch_handlePreviousExact(UStringSearch *strsrch, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        setMatchNotFound(strsrch);
        return FALSE;
    }

#if BOYER_MOORE
    UCollationElements *coleiter        = strsrch->textIter;
    int32_t            *patternce       = strsrch->pattern.ces;
    int32_t             patterncelength = strsrch->pattern.cesLength;
    int32_t             textoffset      = ucol_getOffset(coleiter);

    // shifting it check for setting offset
    // if setOffset is called previously or there was no previous match, we
    // leave the offset as it is.
    if (strsrch->search->matchedIndex != USEARCH_DONE) {
        textoffset = strsrch->search->matchedIndex;
    }

    textoffset = reverseShift(strsrch, textoffset, UCOL_NULLORDER,
                              patterncelength);

    while (textoffset >= 0)
    {
        int32_t     patternceindex = 1;
        int32_t     targetce;
        UBool       found          = FALSE;
        int32_t     firstce        = UCOL_NULLORDER;

        // if status is a failure, ucol_setOffset does nothing
        setColEIterOffset(coleiter, textoffset);

        for (;;) {
            // finding the first pattern ce match, imagine composite
            // characters. for example: search for pattern \u0300 in text
            // \u00C0, we'll have to skip A first before we get to
            // \u0300 the grave accent
            targetce = ucol_next(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce = getCE(strsrch, targetce);
            if (firstce == UCOL_NULLORDER || firstce == UCOL_IGNORABLE) {
                firstce = targetce;
            }
            if (targetce == UCOL_IGNORABLE && strsrch->strength != UCOL_PRIMARY) {
                continue;
            }
            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            if (targetce == patternce[0]) {
                found = TRUE;
                break;
            }
            if (!hasExpansion(coleiter)) {
                // checking for accents in composite character
                found = FALSE;
                break;
            }
        }

        //targetce = firstce;

        while (found && (patternceindex < patterncelength)) {
            firstce = targetce;
            targetce    = ucol_next(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce    = getCE(strsrch, targetce);
            if (targetce == UCOL_IGNORABLE) {
                continue;
            }

            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            found = found && targetce == patternce[patternceindex];
            patternceindex ++;
        }

        targetce = firstce;

        if (!found) {
            if (U_FAILURE(*status)) {
                break;
            }

            textoffset = reverseShift(strsrch, textoffset, targetce,
                                      patternceindex);
            patternceindex = 0;
            continue;
        }

        if (checkPreviousExactMatch(strsrch, &textoffset, status)) {
            setColEIterOffset(coleiter, textoffset);
            return TRUE;
        }
    }
    setMatchNotFound(strsrch);
    return FALSE;
#else
    int32_t textOffset;

    if (strsrch->search->isOverlap) {
        if (strsrch->search->matchedIndex != USEARCH_DONE) {
            textOffset = strsrch->search->matchedIndex + strsrch->search->matchedLength - 1;
        } else {
            // move the start position at the end of possible match
            initializePatternPCETable(strsrch, status);
            if (!initTextProcessedIter(strsrch, status)) {
                setMatchNotFound(strsrch);
                return FALSE;
            }
            for (int32_t nPCEs = 0; nPCEs < strsrch->pattern.pcesLength - 1; nPCEs++) {
                int64_t pce = strsrch->textProcessedIter->nextProcessed(NULL, NULL, status);
                if (pce == UCOL_PROCESSED_NULLORDER) {
                    // at the end of the text
                    break;
                }
            }
            if (U_FAILURE(*status)) {
                setMatchNotFound(strsrch);
                return FALSE;
            }
            textOffset = ucol_getOffset(strsrch->textIter);
        }
    } else {
        textOffset = ucol_getOffset(strsrch->textIter);
    }

    int32_t start = -1;
    int32_t end = -1;

    if (usearch_searchBackwards(strsrch, textOffset, &start, &end, status)) {
        strsrch->search->matchedIndex = start;
        strsrch->search->matchedLength = end - start;
        return TRUE;
    } else {
        setMatchNotFound(strsrch);
        return FALSE;
    }
#endif
}

UBool usearch_handlePreviousCanonical(UStringSearch *strsrch,
                                      UErrorCode    *status)
{
    if (U_FAILURE(*status)) {
        setMatchNotFound(strsrch);
        return FALSE;
    }

#if BOYER_MOORE
    UCollationElements *coleiter        = strsrch->textIter;
    int32_t            *patternce       = strsrch->pattern.ces;
    int32_t             patterncelength = strsrch->pattern.cesLength;
    int32_t             textoffset      = ucol_getOffset(coleiter);
    UBool               hasPatternAccents =
       strsrch->pattern.hasSuffixAccents || strsrch->pattern.hasPrefixAccents;

    // shifting it check for setting offset
    // if setOffset is called previously or there was no previous match, we
    // leave the offset as it is.
    if (strsrch->search->matchedIndex != USEARCH_DONE) {
        textoffset = strsrch->search->matchedIndex;
    }

    textoffset = reverseShift(strsrch, textoffset, UCOL_NULLORDER,
                              patterncelength);
    strsrch->canonicalPrefixAccents[0] = 0;
    strsrch->canonicalSuffixAccents[0] = 0;

    while (textoffset >= 0)
    {
        int32_t     patternceindex = 1;
        int32_t     targetce;
        UBool       found          = FALSE;
        int32_t     firstce        = UCOL_NULLORDER;

        setColEIterOffset(coleiter, textoffset);
        for (;;) {
            // finding the first pattern ce match, imagine composite
            // characters. for example: search for pattern \u0300 in text
            // \u00C0, we'll have to skip A first before we get to
            // \u0300 the grave accent
            targetce = ucol_next(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce = getCE(strsrch, targetce);
            if (firstce == UCOL_NULLORDER || firstce == UCOL_IGNORABLE) {
                firstce = targetce;
            }

            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            if (targetce == patternce[0]) {
                // the first ce can be a contraction
                found = TRUE;
                break;
            }
            if (!hasExpansion(coleiter)) {
                // checking for accents in composite character
                found = FALSE;
                break;
            }
        }

        targetce = firstce;

        while (found && patternceindex < patterncelength) {
            targetce    = ucol_next(coleiter, status);
            if (U_FAILURE(*status) || targetce == UCOL_NULLORDER) {
                found = FALSE;
                break;
            }
            targetce = getCE(strsrch, targetce);
            if (targetce == UCOL_IGNORABLE) {
                continue;
            }

            // TODO: #if BOYER_MOORE, replace with code using 32-bit version of compareCE64s
            found = found && targetce == patternce[patternceindex];
            patternceindex ++;
        }

        // initializing the rearranged accent array
        if (hasPatternAccents && !found) {
            strsrch->canonicalPrefixAccents[0] = 0;
            strsrch->canonicalSuffixAccents[0] = 0;
            if (U_FAILURE(*status)) {
                break;
            }
            found = doPreviousCanonicalMatch(strsrch, textoffset, status);
        }

        if (!found) {
            if (U_FAILURE(*status)) {
                break;
            }
            textoffset = reverseShift(strsrch, textoffset, targetce,
                                      patternceindex);
            patternceindex = 0;
            continue;
        }

        if (checkPreviousCanonicalMatch(strsrch, &textoffset, status)) {
            setColEIterOffset(coleiter, textoffset);
            return TRUE;
        }
    }
    setMatchNotFound(strsrch);
    return FALSE;
#else
    int32_t textOffset;

    if (strsrch->search->isOverlap) {
        if (strsrch->search->matchedIndex != USEARCH_DONE) {
            textOffset = strsrch->search->matchedIndex + strsrch->search->matchedLength - 1;
        } else {
            // move the start position at the end of possible match
            initializePatternPCETable(strsrch, status);
            if (!initTextProcessedIter(strsrch, status)) {
                setMatchNotFound(strsrch);
                return FALSE;
            }
            for (int32_t nPCEs = 0; nPCEs < strsrch->pattern.pcesLength - 1; nPCEs++) {
                int64_t pce = strsrch->textProcessedIter->nextProcessed(NULL, NULL, status);
                if (pce == UCOL_PROCESSED_NULLORDER) {
                    // at the end of the text
                    break;
                }
            }
            if (U_FAILURE(*status)) {
                setMatchNotFound(strsrch);
                return FALSE;
            }
            textOffset = ucol_getOffset(strsrch->textIter);
        }
    } else {
        textOffset = ucol_getOffset(strsrch->textIter);
    }

    int32_t start = -1;
    int32_t end = -1;

    if (usearch_searchBackwards(strsrch, textOffset, &start, &end, status)) {
        strsrch->search->matchedIndex = start;
        strsrch->search->matchedLength = end - start;
        return TRUE;
    } else {
        setMatchNotFound(strsrch);
        return FALSE;
    }
#endif
}

#endif /* #if !UCONFIG_NO_COLLATION */
