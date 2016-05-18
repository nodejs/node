/*
*******************************************************************************
*
*   Copyright (C) 2009-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jul30
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udatpg.h"
#include "unicode/uenum.h"
#include "unicode/strenum.h"
#include "unicode/dtptngen.h"
#include "ustrenum.h"

U_NAMESPACE_USE

U_CAPI UDateTimePatternGenerator * U_EXPORT2
udatpg_open(const char *locale, UErrorCode *pErrorCode) {
    if(locale==NULL) {
        return (UDateTimePatternGenerator *)DateTimePatternGenerator::createInstance(*pErrorCode);
    } else {
        return (UDateTimePatternGenerator *)DateTimePatternGenerator::createInstance(Locale(locale), *pErrorCode);
    }
}

U_CAPI UDateTimePatternGenerator * U_EXPORT2
udatpg_openEmpty(UErrorCode *pErrorCode) {
    return (UDateTimePatternGenerator *)DateTimePatternGenerator::createEmptyInstance(*pErrorCode);
}

U_CAPI void U_EXPORT2
udatpg_close(UDateTimePatternGenerator *dtpg) {
    delete (DateTimePatternGenerator *)dtpg;
}

U_CAPI UDateTimePatternGenerator * U_EXPORT2
udatpg_clone(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }
    return (UDateTimePatternGenerator *)(((const DateTimePatternGenerator *)dtpg)->clone());
}

U_CAPI int32_t U_EXPORT2
udatpg_getBestPattern(UDateTimePatternGenerator *dtpg,
                      const UChar *skeleton, int32_t length,
                      UChar *bestPattern, int32_t capacity,
                      UErrorCode *pErrorCode) {
    return udatpg_getBestPatternWithOptions(dtpg, skeleton, length,
                                            UDATPG_MATCH_NO_OPTIONS,
                                            bestPattern, capacity, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
udatpg_getBestPatternWithOptions(UDateTimePatternGenerator *dtpg,
                                 const UChar *skeleton, int32_t length,
                                 UDateTimePatternMatchOptions options,
                                 UChar *bestPattern, int32_t capacity,
                                 UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(skeleton==NULL && length!=0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString skeletonString((UBool)(length<0), skeleton, length);
    UnicodeString result=((DateTimePatternGenerator *)dtpg)->getBestPattern(skeletonString, options, *pErrorCode);
    return result.extract(bestPattern, capacity, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
udatpg_getSkeleton(UDateTimePatternGenerator * /* dtpg */,
                   const UChar *pattern, int32_t length,
                   UChar *skeleton, int32_t capacity,
                   UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(pattern==NULL && length!=0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString patternString((UBool)(length<0), pattern, length);
    UnicodeString result=DateTimePatternGenerator::staticGetSkeleton(
            patternString, *pErrorCode);
    return result.extract(skeleton, capacity, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
udatpg_getBaseSkeleton(UDateTimePatternGenerator * /* dtpg */,
                       const UChar *pattern, int32_t length,
                       UChar *skeleton, int32_t capacity,
                       UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(pattern==NULL && length!=0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString patternString((UBool)(length<0), pattern, length);
    UnicodeString result=DateTimePatternGenerator::staticGetBaseSkeleton(
            patternString, *pErrorCode);
    return result.extract(skeleton, capacity, *pErrorCode);
}

U_CAPI UDateTimePatternConflict U_EXPORT2
udatpg_addPattern(UDateTimePatternGenerator *dtpg,
                  const UChar *pattern, int32_t patternLength,
                  UBool override,
                  UChar *conflictingPattern, int32_t capacity, int32_t *pLength,
                  UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return UDATPG_NO_CONFLICT;
    }
    if(pattern==NULL && patternLength!=0) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return UDATPG_NO_CONFLICT;
    }
    UnicodeString patternString((UBool)(patternLength<0), pattern, patternLength);
    UnicodeString conflictingPatternString;
    UDateTimePatternConflict result=((DateTimePatternGenerator *)dtpg)->
            addPattern(patternString, override, conflictingPatternString, *pErrorCode);
    int32_t length=conflictingPatternString.extract(conflictingPattern, capacity, *pErrorCode);
    if(pLength!=NULL) {
        *pLength=length;
    }
    return result;
}

U_CAPI void U_EXPORT2
udatpg_setAppendItemFormat(UDateTimePatternGenerator *dtpg,
                           UDateTimePatternField field,
                           const UChar *value, int32_t length) {
    UnicodeString valueString((UBool)(length<0), value, length);
    ((DateTimePatternGenerator *)dtpg)->setAppendItemFormat(field, valueString);
}

U_CAPI const UChar * U_EXPORT2
udatpg_getAppendItemFormat(const UDateTimePatternGenerator *dtpg,
                           UDateTimePatternField field,
                           int32_t *pLength) {
    const UnicodeString &result=((const DateTimePatternGenerator *)dtpg)->getAppendItemFormat(field);
    if(pLength!=NULL) {
        *pLength=result.length();
    }
    return result.getBuffer();
}

U_CAPI void U_EXPORT2
udatpg_setAppendItemName(UDateTimePatternGenerator *dtpg,
                         UDateTimePatternField field,
                         const UChar *value, int32_t length) {
    UnicodeString valueString((UBool)(length<0), value, length);
    ((DateTimePatternGenerator *)dtpg)->setAppendItemName(field, valueString);
}

U_CAPI const UChar * U_EXPORT2
udatpg_getAppendItemName(const UDateTimePatternGenerator *dtpg,
                         UDateTimePatternField field,
                         int32_t *pLength) {
    const UnicodeString &result=((const DateTimePatternGenerator *)dtpg)->getAppendItemName(field);
    if(pLength!=NULL) {
        *pLength=result.length();
    }
    return result.getBuffer();
}

U_CAPI void U_EXPORT2
udatpg_setDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         const UChar *dtFormat, int32_t length) {
    UnicodeString dtFormatString((UBool)(length<0), dtFormat, length);
    ((DateTimePatternGenerator *)dtpg)->setDateTimeFormat(dtFormatString);
}

U_CAPI const UChar * U_EXPORT2
udatpg_getDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         int32_t *pLength) {
    const UnicodeString &result=((const DateTimePatternGenerator *)dtpg)->getDateTimeFormat();
    if(pLength!=NULL) {
        *pLength=result.length();
    }
    return result.getBuffer();
}

U_CAPI void U_EXPORT2
udatpg_setDecimal(UDateTimePatternGenerator *dtpg,
                  const UChar *decimal, int32_t length) {
    UnicodeString decimalString((UBool)(length<0), decimal, length);
    ((DateTimePatternGenerator *)dtpg)->setDecimal(decimalString);
}

U_CAPI const UChar * U_EXPORT2
udatpg_getDecimal(const UDateTimePatternGenerator *dtpg,
                  int32_t *pLength) {
    const UnicodeString &result=((const DateTimePatternGenerator *)dtpg)->getDecimal();
    if(pLength!=NULL) {
        *pLength=result.length();
    }
    return result.getBuffer();
}

U_CAPI int32_t U_EXPORT2
udatpg_replaceFieldTypes(UDateTimePatternGenerator *dtpg,
                         const UChar *pattern, int32_t patternLength,
                         const UChar *skeleton, int32_t skeletonLength,
                         UChar *dest, int32_t destCapacity,
                         UErrorCode *pErrorCode) {
    return udatpg_replaceFieldTypesWithOptions(dtpg, pattern, patternLength, skeleton, skeletonLength,
                                               UDATPG_MATCH_NO_OPTIONS,
                                               dest, destCapacity, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
udatpg_replaceFieldTypesWithOptions(UDateTimePatternGenerator *dtpg,
                                    const UChar *pattern, int32_t patternLength,
                                    const UChar *skeleton, int32_t skeletonLength,
                                    UDateTimePatternMatchOptions options,
                                    UChar *dest, int32_t destCapacity,
                                    UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if((pattern==NULL && patternLength!=0) || (skeleton==NULL && skeletonLength!=0)) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    UnicodeString patternString((UBool)(patternLength<0), pattern, patternLength);
    UnicodeString skeletonString((UBool)(skeletonLength<0), skeleton, skeletonLength);
    UnicodeString result=((DateTimePatternGenerator *)dtpg)->replaceFieldTypes(patternString, skeletonString, options, *pErrorCode);
    return result.extract(dest, destCapacity, *pErrorCode);
}

U_CAPI UEnumeration * U_EXPORT2
udatpg_openSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode) {
    return uenum_openFromStringEnumeration(
                ((DateTimePatternGenerator *)dtpg)->getSkeletons(*pErrorCode),
                pErrorCode);
}

U_CAPI UEnumeration * U_EXPORT2
udatpg_openBaseSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode) {
    return uenum_openFromStringEnumeration(
                ((DateTimePatternGenerator *)dtpg)->getBaseSkeletons(*pErrorCode),
                pErrorCode);
}

U_CAPI const UChar * U_EXPORT2
udatpg_getPatternForSkeleton(const UDateTimePatternGenerator *dtpg,
                             const UChar *skeleton, int32_t skeletonLength,
                             int32_t *pLength) {
    UnicodeString skeletonString((UBool)(skeletonLength<0), skeleton, skeletonLength);
    const UnicodeString &result=((const DateTimePatternGenerator *)dtpg)->getPatternForSkeleton(skeletonString);
    if(pLength!=NULL) {
        *pLength=result.length();
    }
    return result.getBuffer();
}

#endif
