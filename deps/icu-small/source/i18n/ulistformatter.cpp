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
#include "formattedval_impl.h"

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


U_CAPI UListFormatter* U_EXPORT2
ulistfmt_openForType(const char*  locale, UListFormatterType type,
                    UListFormatterWidth width, UErrorCode* status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    LocalPointer<ListFormatter> listfmt(ListFormatter::createInstance(Locale(locale), type, width, *status));
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


// Magic number: FLST in ASCII
UPRV_FORMATTED_VALUE_CAPI_AUTO_IMPL(
    FormattedList,
    UFormattedList,
    UFormattedListImpl,
    UFormattedListApiHelper,
    ulistfmt,
    0x464C5354)


static UnicodeString* getUnicodeStrings(
        const UChar* const strings[],
        const int32_t* stringLengths,
        int32_t stringCount,
        UnicodeString* length4StackBuffer,
        LocalArray<UnicodeString>& maybeOwner,
        UErrorCode& status) {
    U_ASSERT(U_SUCCESS(status));
    if (stringCount < 0 || (strings == NULL && stringCount > 0)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    UnicodeString* ustrings = length4StackBuffer;
    if (stringCount > 4) {
        maybeOwner.adoptInsteadAndCheckErrorCode(new UnicodeString[stringCount], status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        ustrings = maybeOwner.getAlias();
    }
    if (stringLengths == NULL) {
        for (int32_t stringIndex = 0; stringIndex < stringCount; stringIndex++) {
            ustrings[stringIndex].setTo(true, strings[stringIndex], -1);
        }
    } else {
        for (int32_t stringIndex = 0; stringIndex < stringCount; stringIndex++) {
            ustrings[stringIndex].setTo(stringLengths[stringIndex] < 0, strings[stringIndex], stringLengths[stringIndex]);
        }
    }
    return ustrings;
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
    if ((result == NULL) ? resultCapacity != 0 : resultCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    UnicodeString length4StackBuffer[4];
    LocalArray<UnicodeString> maybeOwner;
    UnicodeString* ustrings = getUnicodeStrings(
        strings, stringLengths, stringCount, length4StackBuffer, maybeOwner, *status);
    if (U_FAILURE(*status)) {
        return -1;
    }
    UnicodeString res;
    if (result != NULL) {
        // NULL destination for pure preflighting: empty dummy string
        // otherwise, alias the destination buffer (copied from udat_format)
        res.setTo(result, 0, resultCapacity);
    }
    reinterpret_cast<const ListFormatter*>(listfmt)->format( ustrings, stringCount, res, *status );
    return res.extract(result, resultCapacity, *status);
}


U_CAPI void U_EXPORT2
ulistfmt_formatStringsToResult(
                const UListFormatter* listfmt,
                const UChar* const strings[],
                const int32_t *    stringLengths,
                int32_t            stringCount,
                UFormattedList*    uresult,
                UErrorCode*        status) {
    auto* result = UFormattedListApiHelper::validate(uresult, *status);
    if (U_FAILURE(*status)) {
        return;
    }
    UnicodeString length4StackBuffer[4];
    LocalArray<UnicodeString> maybeOwner;
    UnicodeString* ustrings = getUnicodeStrings(
        strings, stringLengths, stringCount, length4StackBuffer, maybeOwner, *status);
    if (U_FAILURE(*status)) {
        return;
    }
    result->fImpl = reinterpret_cast<const ListFormatter*>(listfmt)
        ->formatStringsToValue(ustrings, stringCount, *status);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
