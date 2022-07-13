// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2013, International Business Machines Corporation and others.
* All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unumsys.h"
#include "unicode/numsys.h"
#include "unicode/uenum.h"

U_NAMESPACE_USE


U_CAPI UNumberingSystem* U_EXPORT2
unumsys_open(const char *locale, UErrorCode *status)
{
    // createInstance returns immediately if status indicates error
    return (UNumberingSystem*)NumberingSystem::createInstance(Locale(locale), *status);
}


U_CAPI UNumberingSystem* U_EXPORT2
unumsys_openByName(const char *name, UErrorCode *status)
{
    // createInstanceByName does NOT return immediately if status indicates error
    if (U_FAILURE(*status)) {
        return NULL;
    }
    return (UNumberingSystem*)NumberingSystem::createInstanceByName(name, *status);
}


U_CAPI void U_EXPORT2
unumsys_close(UNumberingSystem *unumsys)
{
    delete ((NumberingSystem*)unumsys);
}


U_CAPI UEnumeration* U_EXPORT2
unumsys_openAvailableNames(UErrorCode *status)
{
    // getAvailableNames returns immediately if status indicates error
    return uenum_openFromStringEnumeration(NumberingSystem::getAvailableNames(*status), status);
}


U_CAPI const char * U_EXPORT2
unumsys_getName(const UNumberingSystem *unumsys)
{
    return ((NumberingSystem*)unumsys)->getName();
}


U_CAPI int32_t U_EXPORT2
unumsys_getRadix(const UNumberingSystem *unumsys)
{
    return ((NumberingSystem*)unumsys)->getRadix();
}


U_CAPI UBool U_EXPORT2
unumsys_isAlgorithmic(const UNumberingSystem *unumsys)
{
    return ((NumberingSystem*)unumsys)->isAlgorithmic();
}

U_CAPI int32_t U_EXPORT2
unumsys_getDescription(const UNumberingSystem *unumsys, UChar *result,
                       int32_t resultLength, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return -1;
    }
    // implement
    UnicodeString descrip = ((NumberingSystem*)unumsys)->getDescription();
    return descrip.extract(result, resultLength, *status);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
