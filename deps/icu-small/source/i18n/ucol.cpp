// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 1996-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucol.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
* Modification history
* Date        Name      Comments
* 1996-1999   various members of ICU team maintained C API for collation framework
* 02/16/2001  synwee    Added internal method getPrevSpecialCE
* 03/01/2001  synwee    Added maxexpansion functionality.
* 03/16/2001  weiv      Collation framework is rewritten in C and made UCA compliant
* 2012-2014   markus    Rewritten in C++ again.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/bytestream.h"
#include "unicode/coleitr.h"
#include "unicode/ucoleitr.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "collation.h"
#include "cstring.h"
#include "putilimp.h"
#include "uassert.h"
#include "utracimp.h"

U_NAMESPACE_USE

U_CAPI UCollator* U_EXPORT2
ucol_openBinary(const uint8_t *bin, int32_t length,
                const UCollator *base,
                UErrorCode *status)
{
    if(U_FAILURE(*status)) { return NULL; }
    RuleBasedCollator *coll = new RuleBasedCollator(
            bin, length,
            RuleBasedCollator::rbcFromUCollator(base),
            *status);
    if(coll == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if(U_FAILURE(*status)) {
        delete coll;
        return NULL;
    }
    return coll->toUCollator();
}

U_CAPI int32_t U_EXPORT2
ucol_cloneBinary(const UCollator *coll,
                 uint8_t *buffer, int32_t capacity,
                 UErrorCode *status)
{
    if(U_FAILURE(*status)) {
        return 0;
    }
    const RuleBasedCollator *rbc = RuleBasedCollator::rbcFromUCollator(coll);
    if(rbc == NULL && coll != NULL) {
        *status = U_UNSUPPORTED_ERROR;
        return 0;
    }
    return rbc->cloneBinary(buffer, capacity, *status);
}

U_CAPI UCollator* U_EXPORT2
ucol_safeClone(const UCollator *coll, void * /*stackBuffer*/, int32_t * pBufferSize, UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return NULL;
    }
    if (coll == NULL) {
       *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    if (pBufferSize != NULL) {
        int32_t inputSize = *pBufferSize;
        *pBufferSize = 1;
        if (inputSize == 0) {
            return NULL;  // preflighting for deprecated functionality
        }
    }
    Collator *newColl = Collator::fromUCollator(coll)->clone();
    if (newColl == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    } else {
        *status = U_SAFECLONE_ALLOCATED_WARNING;
    }
    return newColl->toUCollator();
}

U_CAPI void U_EXPORT2
ucol_close(UCollator *coll)
{
    UTRACE_ENTRY_OC(UTRACE_UCOL_CLOSE);
    UTRACE_DATA1(UTRACE_INFO, "coll = %p", coll);
    if(coll != NULL) {
        delete Collator::fromUCollator(coll);
    }
    UTRACE_EXIT();
}

U_CAPI int32_t U_EXPORT2
ucol_mergeSortkeys(const uint8_t *src1, int32_t src1Length,
                   const uint8_t *src2, int32_t src2Length,
                   uint8_t *dest, int32_t destCapacity) {
    /* check arguments */
    if( src1==NULL || src1Length<-1 || src1Length==0 || (src1Length>0 && src1[src1Length-1]!=0) ||
        src2==NULL || src2Length<-1 || src2Length==0 || (src2Length>0 && src2[src2Length-1]!=0) ||
        destCapacity<0 || (destCapacity>0 && dest==NULL)
    ) {
        /* error, attempt to write a zero byte and return 0 */
        if(dest!=NULL && destCapacity>0) {
            *dest=0;
        }
        return 0;
    }

    /* check lengths and capacity */
    if(src1Length<0) {
        src1Length=(int32_t)uprv_strlen((const char *)src1)+1;
    }
    if(src2Length<0) {
        src2Length=(int32_t)uprv_strlen((const char *)src2)+1;
    }

    int32_t destLength=src1Length+src2Length;
    if(destLength>destCapacity) {
        /* the merged sort key does not fit into the destination */
        return destLength;
    }

    /* merge the sort keys with the same number of levels */
    uint8_t *p=dest;
    for(;;) {
        /* copy level from src1 not including 00 or 01 */
        uint8_t b;
        while((b=*src1)>=2) {
            ++src1;
            *p++=b;
        }

        /* add a 02 merge separator */
        *p++=2;

        /* copy level from src2 not including 00 or 01 */
        while((b=*src2)>=2) {
            ++src2;
            *p++=b;
        }

        /* if both sort keys have another level, then add a 01 level separator and continue */
        if(*src1==1 && *src2==1) {
            ++src1;
            ++src2;
            *p++=1;
        } else {
            break;
        }
    }

    /*
     * here, at least one sort key is finished now, but the other one
     * might have some contents left from containing more levels;
     * that contents is just appended to the result
     */
    if(*src1!=0) {
        /* src1 is not finished, therefore *src2==0, and src1 is appended */
        src2=src1;
    }
    /* append src2, "the other, unfinished sort key" */
    while((*p++=*src2++)!=0) {}

    /* the actual length might be less than destLength if either sort key contained illegally embedded zero bytes */
    return (int32_t)(p-dest);
}

U_CAPI int32_t U_EXPORT2
ucol_getSortKey(const    UCollator    *coll,
        const    UChar        *source,
        int32_t        sourceLength,
        uint8_t        *result,
        int32_t        resultLength)
{
    UTRACE_ENTRY(UTRACE_UCOL_GET_SORTKEY);
    if (UTRACE_LEVEL(UTRACE_VERBOSE)) {
        UTRACE_DATA3(UTRACE_VERBOSE, "coll=%p, source string = %vh ", coll, source,
            ((sourceLength==-1 && source!=NULL) ? u_strlen(source) : sourceLength));
    }

    int32_t keySize = Collator::fromUCollator(coll)->
            getSortKey(source, sourceLength, result, resultLength);

    UTRACE_DATA2(UTRACE_VERBOSE, "Sort Key = %vb", result, keySize);
    UTRACE_EXIT_VALUE(keySize);
    return keySize;
}

U_CAPI int32_t U_EXPORT2
ucol_nextSortKeyPart(const UCollator *coll,
                     UCharIterator *iter,
                     uint32_t state[2],
                     uint8_t *dest, int32_t count,
                     UErrorCode *status)
{
    /* error checking */
    if(status==NULL || U_FAILURE(*status)) {
        return 0;
    }
    UTRACE_ENTRY(UTRACE_UCOL_NEXTSORTKEYPART);
    UTRACE_DATA6(UTRACE_VERBOSE, "coll=%p, iter=%p, state=%d %d, dest=%p, count=%d",
                  coll, iter, state[0], state[1], dest, count);

    int32_t i = Collator::fromUCollator(coll)->
            internalNextSortKeyPart(iter, state, dest, count, *status);

    // Return number of meaningful sortkey bytes.
    UTRACE_DATA4(UTRACE_VERBOSE, "dest = %vb, state=%d %d",
                  dest,i, state[0], state[1]);
    UTRACE_EXIT_VALUE_STATUS(i, *status);
    return i;
}

/**
 * Produce a bound for a given sortkey and a number of levels.
 */
U_CAPI int32_t U_EXPORT2
ucol_getBound(const uint8_t       *source,
        int32_t             sourceLength,
        UColBoundMode       boundType,
        uint32_t            noOfLevels,
        uint8_t             *result,
        int32_t             resultLength,
        UErrorCode          *status)
{
    // consistency checks
    if(status == NULL || U_FAILURE(*status)) {
        return 0;
    }
    if(source == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    int32_t sourceIndex = 0;
    // Scan the string until we skip enough of the key OR reach the end of the key
    do {
        sourceIndex++;
        if(source[sourceIndex] == Collation::LEVEL_SEPARATOR_BYTE) {
            noOfLevels--;
        }
    } while (noOfLevels > 0
        && (source[sourceIndex] != 0 || sourceIndex < sourceLength));

    if((source[sourceIndex] == 0 || sourceIndex == sourceLength)
        && noOfLevels > 0) {
            *status = U_SORT_KEY_TOO_SHORT_WARNING;
    }


    // READ ME: this code assumes that the values for boundType
    // enum will not changes. They are set so that the enum value
    // corresponds to the number of extra bytes each bound type
    // needs.
    if(result != NULL && resultLength >= sourceIndex+boundType) {
        uprv_memcpy(result, source, sourceIndex);
        switch(boundType) {
            // Lower bound just gets terminated. No extra bytes
        case UCOL_BOUND_LOWER: // = 0
            break;
            // Upper bound needs one extra byte
        case UCOL_BOUND_UPPER: // = 1
            result[sourceIndex++] = 2;
            break;
            // Upper long bound needs two extra bytes
        case UCOL_BOUND_UPPER_LONG: // = 2
            result[sourceIndex++] = 0xFF;
            result[sourceIndex++] = 0xFF;
            break;
        default:
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
        result[sourceIndex++] = 0;

        return sourceIndex;
    } else {
        return sourceIndex+boundType+1;
    }
}

U_CAPI void U_EXPORT2
ucol_setMaxVariable(UCollator *coll, UColReorderCode group, UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) { return; }
    Collator::fromUCollator(coll)->setMaxVariable(group, *pErrorCode);
}

U_CAPI UColReorderCode U_EXPORT2
ucol_getMaxVariable(const UCollator *coll) {
    return Collator::fromUCollator(coll)->getMaxVariable();
}

U_CAPI uint32_t  U_EXPORT2
ucol_setVariableTop(UCollator *coll, const UChar *varTop, int32_t len, UErrorCode *status) {
    if(U_FAILURE(*status) || coll == NULL) {
        return 0;
    }
    return Collator::fromUCollator(coll)->setVariableTop(varTop, len, *status);
}

U_CAPI uint32_t U_EXPORT2 ucol_getVariableTop(const UCollator *coll, UErrorCode *status) {
    if(U_FAILURE(*status) || coll == NULL) {
        return 0;
    }
    return Collator::fromUCollator(coll)->getVariableTop(*status);
}

U_CAPI void  U_EXPORT2
ucol_restoreVariableTop(UCollator *coll, const uint32_t varTop, UErrorCode *status) {
    if(U_FAILURE(*status) || coll == NULL) {
        return;
    }
    Collator::fromUCollator(coll)->setVariableTop(varTop, *status);
}

U_CAPI void  U_EXPORT2
ucol_setAttribute(UCollator *coll, UColAttribute attr, UColAttributeValue value, UErrorCode *status) {
    if(U_FAILURE(*status) || coll == NULL) {
      return;
    }

    Collator::fromUCollator(coll)->setAttribute(attr, value, *status);
}

U_CAPI UColAttributeValue  U_EXPORT2
ucol_getAttribute(const UCollator *coll, UColAttribute attr, UErrorCode *status) {
    if(U_FAILURE(*status) || coll == NULL) {
      return UCOL_DEFAULT;
    }

    return Collator::fromUCollator(coll)->getAttribute(attr, *status);
}

U_CAPI void U_EXPORT2
ucol_setStrength(    UCollator                *coll,
            UCollationStrength        strength)
{
    UErrorCode status = U_ZERO_ERROR;
    ucol_setAttribute(coll, UCOL_STRENGTH, strength, &status);
}

U_CAPI UCollationStrength U_EXPORT2
ucol_getStrength(const UCollator *coll)
{
    UErrorCode status = U_ZERO_ERROR;
    return ucol_getAttribute(coll, UCOL_STRENGTH, &status);
}

U_CAPI int32_t U_EXPORT2
ucol_getReorderCodes(const UCollator *coll,
                    int32_t *dest,
                    int32_t destCapacity,
                    UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return 0;
    }

    return Collator::fromUCollator(coll)->getReorderCodes(dest, destCapacity, *status);
}

U_CAPI void U_EXPORT2
ucol_setReorderCodes(UCollator* coll,
                    const int32_t* reorderCodes,
                    int32_t reorderCodesLength,
                    UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return;
    }

    Collator::fromUCollator(coll)->setReorderCodes(reorderCodes, reorderCodesLength, *status);
}

U_CAPI int32_t U_EXPORT2
ucol_getEquivalentReorderCodes(int32_t reorderCode,
                    int32_t* dest,
                    int32_t destCapacity,
                    UErrorCode *pErrorCode) {
    return Collator::getEquivalentReorderCodes(reorderCode, dest, destCapacity, *pErrorCode);
}

U_CAPI void U_EXPORT2
ucol_getVersion(const UCollator* coll,
                UVersionInfo versionInfo)
{
    Collator::fromUCollator(coll)->getVersion(versionInfo);
}

U_CAPI UCollationResult U_EXPORT2
ucol_strcollIter( const UCollator    *coll,
                 UCharIterator *sIter,
                 UCharIterator *tIter,
                 UErrorCode         *status)
{
    if(!status || U_FAILURE(*status)) {
        return UCOL_EQUAL;
    }

    UTRACE_ENTRY(UTRACE_UCOL_STRCOLLITER);
    UTRACE_DATA3(UTRACE_VERBOSE, "coll=%p, sIter=%p, tIter=%p", coll, sIter, tIter);

    if(sIter == NULL || tIter == NULL || coll == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        UTRACE_EXIT_VALUE_STATUS(UCOL_EQUAL, *status);
        return UCOL_EQUAL;
    }

    UCollationResult result = Collator::fromUCollator(coll)->compare(*sIter, *tIter, *status);

    UTRACE_EXIT_VALUE_STATUS(result, *status);
    return result;
}


/*                                                                      */
/* ucol_strcoll     Main public API string comparison function          */
/*                                                                      */
U_CAPI UCollationResult U_EXPORT2
ucol_strcoll( const UCollator    *coll,
              const UChar        *source,
              int32_t            sourceLength,
              const UChar        *target,
              int32_t            targetLength)
{
    UTRACE_ENTRY(UTRACE_UCOL_STRCOLL);
    if (UTRACE_LEVEL(UTRACE_VERBOSE)) {
        UTRACE_DATA3(UTRACE_VERBOSE, "coll=%p, source=%p, target=%p", coll, source, target);
        UTRACE_DATA2(UTRACE_VERBOSE, "source string = %vh ", source, sourceLength);
        UTRACE_DATA2(UTRACE_VERBOSE, "target string = %vh ", target, targetLength);
    }

    UErrorCode status = U_ZERO_ERROR;
    UCollationResult returnVal = Collator::fromUCollator(coll)->
            compare(source, sourceLength, target, targetLength, status);
    UTRACE_EXIT_VALUE_STATUS(returnVal, status);
    return returnVal;
}

U_CAPI UCollationResult U_EXPORT2
ucol_strcollUTF8(
        const UCollator *coll,
        const char      *source,
        int32_t         sourceLength,
        const char      *target,
        int32_t         targetLength,
        UErrorCode      *status)
{
    UTRACE_ENTRY(UTRACE_UCOL_STRCOLLUTF8);
    if (UTRACE_LEVEL(UTRACE_VERBOSE)) {
        UTRACE_DATA3(UTRACE_VERBOSE, "coll=%p, source=%p, target=%p", coll, source, target);
        UTRACE_DATA2(UTRACE_VERBOSE, "source string = %vb ", source, sourceLength);
        UTRACE_DATA2(UTRACE_VERBOSE, "target string = %vb ", target, targetLength);
    }

    if (U_FAILURE(*status)) {
        /* do nothing */
        UTRACE_EXIT_VALUE_STATUS(UCOL_EQUAL, *status);
        return UCOL_EQUAL;
    }

    UCollationResult returnVal = Collator::fromUCollator(coll)->internalCompareUTF8(
            source, sourceLength, target, targetLength, *status);
    UTRACE_EXIT_VALUE_STATUS(returnVal, *status);
    return returnVal;
}


/* convenience function for comparing strings */
U_CAPI UBool U_EXPORT2
ucol_greater(    const    UCollator        *coll,
        const    UChar            *source,
        int32_t            sourceLength,
        const    UChar            *target,
        int32_t            targetLength)
{
    return (ucol_strcoll(coll, source, sourceLength, target, targetLength)
        == UCOL_GREATER);
}

/* convenience function for comparing strings */
U_CAPI UBool U_EXPORT2
ucol_greaterOrEqual(    const    UCollator    *coll,
            const    UChar        *source,
            int32_t        sourceLength,
            const    UChar        *target,
            int32_t        targetLength)
{
    return (ucol_strcoll(coll, source, sourceLength, target, targetLength)
        != UCOL_LESS);
}

/* convenience function for comparing strings */
U_CAPI UBool U_EXPORT2
ucol_equal(        const    UCollator        *coll,
            const    UChar            *source,
            int32_t            sourceLength,
            const    UChar            *target,
            int32_t            targetLength)
{
    return (ucol_strcoll(coll, source, sourceLength, target, targetLength)
        == UCOL_EQUAL);
}

U_CAPI void U_EXPORT2
ucol_getUCAVersion(const UCollator* coll, UVersionInfo info) {
    const Collator *c = Collator::fromUCollator(coll);
    if(c != NULL) {
        UVersionInfo v;
        c->getVersion(v);
        // Note: This is tied to how the current implementation encodes the UCA version
        // in the overall getVersion().
        // Alternatively, we could load the root collator and get at lower-level data from there.
        // Either way, it will reflect the input collator's UCA version only
        // if it is a known implementation.
        // It would be cleaner to make this a virtual Collator method.
        info[0] = v[1] >> 3;
        info[1] = v[1] & 7;
        info[2] = v[2] >> 6;
        info[3] = 0;
    }
}

U_CAPI const UChar * U_EXPORT2
ucol_getRules(const UCollator *coll, int32_t *length) {
    const RuleBasedCollator *rbc = RuleBasedCollator::rbcFromUCollator(coll);
    // OK to crash if coll==NULL: We do not want to check "this" pointers.
    if(rbc != NULL || coll == NULL) {
        const UnicodeString &rules = rbc->getRules();
        U_ASSERT(rules.getBuffer()[rules.length()] == 0);
        *length = rules.length();
        return rules.getBuffer();
    }
    static const UChar _NUL = 0;
    *length = 0;
    return &_NUL;
}

U_CAPI int32_t U_EXPORT2
ucol_getRulesEx(const UCollator *coll, UColRuleOption delta, UChar *buffer, int32_t bufferLen) {
    UnicodeString rules;
    const RuleBasedCollator *rbc = RuleBasedCollator::rbcFromUCollator(coll);
    if(rbc != NULL || coll == NULL) {
        rbc->getRules(delta, rules);
    }
    if(buffer != NULL && bufferLen > 0) {
        UErrorCode errorCode = U_ZERO_ERROR;
        return rules.extract(buffer, bufferLen, errorCode);
    } else {
        return rules.length();
    }
}

U_CAPI const char * U_EXPORT2
ucol_getLocale(const UCollator *coll, ULocDataLocaleType type, UErrorCode *status) {
    return ucol_getLocaleByType(coll, type, status);
}

U_CAPI const char * U_EXPORT2
ucol_getLocaleByType(const UCollator *coll, ULocDataLocaleType type, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return NULL;
    }
    UTRACE_ENTRY(UTRACE_UCOL_GETLOCALE);
    UTRACE_DATA1(UTRACE_INFO, "coll=%p", coll);

    const char *result;
    const RuleBasedCollator *rbc = RuleBasedCollator::rbcFromUCollator(coll);
    if(rbc == NULL && coll != NULL) {
        *status = U_UNSUPPORTED_ERROR;
        result = NULL;
    } else {
        result = rbc->internalGetLocaleID(type, *status);
    }

    UTRACE_DATA1(UTRACE_INFO, "result = %s", result);
    UTRACE_EXIT_STATUS(*status);
    return result;
}

U_CAPI USet * U_EXPORT2
ucol_getTailoredSet(const UCollator *coll, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return NULL;
    }
    UnicodeSet *set = Collator::fromUCollator(coll)->getTailoredSet(*status);
    if(U_FAILURE(*status)) {
        delete set;
        return NULL;
    }
    return set->toUSet();
}

U_CAPI UBool U_EXPORT2
ucol_equals(const UCollator *source, const UCollator *target) {
    return source == target ||
        (*Collator::fromUCollator(source)) == (*Collator::fromUCollator(target));
}

#endif /* #if !UCONFIG_NO_COLLATION */
