// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ulistformatter.h"
#include "unicode/listformatter.h"
#include "unicode/localpointer.h"
#include "cmemory.h"

U_NAMESPACE_USE

U_CAPI UListFormatter* U_EXPORT2
ulistfmt_open(const char*  locale,
              UErrorCode*  status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    LocalPointer<ListFormatter> listfmt(ListFormatter::createInstance(Locale(locale), *status));
    if (U_FAILURE(*status)) {
        return NULL;
    }
    return (UListFormatter*)listfmt.orphan();
}


U_CAPI void U_EXPORT2
ulistfmt_close(UListFormatter *listfmt)
{
    delete (ListFormatter*)listfmt;
}


U_CAPI int32_t U_EXPORT2
ulistfmt_format(const UListFormatter* listfmt,
                const UChar* const strings[],
                const int32_t *    stringLengths,
                int32_t            stringCount,
                UChar*             result,
                int32_t            resultCapacity,
                UErrorCode*        status)
{
    if (U_FAILURE(*status)) {
        return -1;
    }
    if (stringCount < 0 || (strings == NULL && stringCount > 0) || ((result == NULL)? resultCapacity != 0 : resultCapacity < 0)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    UnicodeString ustringsStackBuf[4];
    UnicodeString* ustrings = ustringsStackBuf;
    if (stringCount > UPRV_LENGTHOF(ustringsStackBuf)) {
        ustrings = new UnicodeString[stringCount];
        if (ustrings == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return -1;
        }
    }
    if (stringLengths == NULL) {
        for (int32_t stringIndex = 0; stringIndex < stringCount; stringIndex++) {
            ustrings[stringIndex].setTo(TRUE, strings[stringIndex], -1);
        }
    } else {
        for (int32_t stringIndex = 0; stringIndex < stringCount; stringIndex++) {
            ustrings[stringIndex].setTo(stringLengths[stringIndex] < 0, strings[stringIndex], stringLengths[stringIndex]);
        }
    }
    UnicodeString res;
    if (result != NULL) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer (copied from udat_format)
        res.setTo(result, 0, resultCapacity);
    }
    ((const ListFormatter*)listfmt)->format( ustrings, stringCount, res, *status );
    if (ustrings != ustringsStackBuf) {
        delete[] ustrings;
    }
    return res.extract(result, resultCapacity, *status);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
