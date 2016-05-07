/*
********************************************************************************
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMFMT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if U_PLATFORM_USES_ONLY_WIN32_API

#if !UCONFIG_NO_FORMATTING

#include "winnmfmt.h"

#include "unicode/format.h"
#include "unicode/numfmt.h"
#include "unicode/locid.h"
#include "unicode/ustring.h"

#include "cmemory.h"
#include "uassert.h"
#include "locmap.h"

#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#include <windows.h>
#include <stdio.h>

U_NAMESPACE_BEGIN

union FormatInfo
{
    NUMBERFMTW   number;
    CURRENCYFMTW currency;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Win32NumberFormat)

#define NEW_ARRAY(type,count) (type *) uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))

#define STACK_BUFFER_SIZE 32

/*
 * Turns a string of the form "3;2;0" into the grouping UINT
 * needed for NUMBERFMT and CURRENCYFMT. If the string does not
 * end in ";0" then the return value should be multiplied by 10.
 * (e.g. "3" => 30, "3;2" => 320)
 */
static UINT getGrouping(const char *grouping)
{
    UINT g = 0;
	const char *s;

    for (s = grouping; *s != '\0'; s += 1) {
        if (*s > '0' && *s < '9') {
            g = g * 10 + (*s - '0');
        } else if (*s != ';') {
            break;
        }
    }

    if (*s != '0') {
        g *= 10;
    }

    return g;
}

static void getNumberFormat(NUMBERFMTW *fmt, int32_t lcid)
{
    char buf[10];

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_IDIGITS, (LPWSTR) &fmt->NumDigits, sizeof(UINT));
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ILZERO,  (LPWSTR) &fmt->LeadingZero, sizeof(UINT));

    GetLocaleInfoA(lcid, LOCALE_SGROUPING, buf, 10);
    fmt->Grouping = getGrouping(buf);

    fmt->lpDecimalSep = NEW_ARRAY(UChar, 6);
    GetLocaleInfoW(lcid, LOCALE_SDECIMAL,  fmt->lpDecimalSep,  6);

    fmt->lpThousandSep = NEW_ARRAY(UChar, 6);
    GetLocaleInfoW(lcid, LOCALE_STHOUSAND, fmt->lpThousandSep, 6);

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_INEGNUMBER, (LPWSTR) &fmt->NegativeOrder, sizeof(UINT));
}

static void freeNumberFormat(NUMBERFMTW *fmt)
{
    if (fmt != NULL) {
        DELETE_ARRAY(fmt->lpThousandSep);
        DELETE_ARRAY(fmt->lpDecimalSep);
    }
}

static void getCurrencyFormat(CURRENCYFMTW *fmt, int32_t lcid)
{
    char buf[10];

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ICURRDIGITS, (LPWSTR) &fmt->NumDigits, sizeof(UINT));
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ILZERO, (LPWSTR) &fmt->LeadingZero, sizeof(UINT));

    GetLocaleInfoA(lcid, LOCALE_SMONGROUPING, buf, sizeof(buf));
    fmt->Grouping = getGrouping(buf);

    fmt->lpDecimalSep = NEW_ARRAY(UChar, 6);
    GetLocaleInfoW(lcid, LOCALE_SMONDECIMALSEP,  fmt->lpDecimalSep,  6);

    fmt->lpThousandSep = NEW_ARRAY(UChar, 6);
    GetLocaleInfoW(lcid, LOCALE_SMONTHOUSANDSEP, fmt->lpThousandSep, 6);

    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_INEGCURR,  (LPWSTR) &fmt->NegativeOrder, sizeof(UINT));
    GetLocaleInfoW(lcid, LOCALE_RETURN_NUMBER|LOCALE_ICURRENCY, (LPWSTR) &fmt->PositiveOrder, sizeof(UINT));

    fmt->lpCurrencySymbol = NEW_ARRAY(UChar, 8);
    GetLocaleInfoW(lcid, LOCALE_SCURRENCY, (LPWSTR) fmt->lpCurrencySymbol, 8);
}

static void freeCurrencyFormat(CURRENCYFMTW *fmt)
{
    if (fmt != NULL) {
        DELETE_ARRAY(fmt->lpCurrencySymbol);
        DELETE_ARRAY(fmt->lpThousandSep);
        DELETE_ARRAY(fmt->lpDecimalSep);
    }
}

Win32NumberFormat::Win32NumberFormat(const Locale &locale, UBool currency, UErrorCode &status)
  : NumberFormat(), fCurrency(currency), fFormatInfo(NULL), fFractionDigitsSet(FALSE)
{
    if (!U_FAILURE(status)) {
        fLCID = locale.getLCID();

        // Resolve actual locale to be used later
        UErrorCode tmpsts = U_ZERO_ERROR;
        char tmpLocID[ULOC_FULLNAME_CAPACITY];
        int32_t len = uloc_getLocaleForLCID(fLCID, tmpLocID, UPRV_LENGTHOF(tmpLocID) - 1, &tmpsts);
        if (U_SUCCESS(tmpsts)) {
            tmpLocID[len] = 0;
            fLocale = Locale((const char*)tmpLocID);
        }

        fFormatInfo = (FormatInfo*)uprv_malloc(sizeof(FormatInfo));

        if (fCurrency) {
            getCurrencyFormat(&fFormatInfo->currency, fLCID);
        } else {
            getNumberFormat(&fFormatInfo->number, fLCID);
        }
    }
}

Win32NumberFormat::Win32NumberFormat(const Win32NumberFormat &other)
  : NumberFormat(other), fFormatInfo((FormatInfo*)uprv_malloc(sizeof(FormatInfo)))
{
    if (fFormatInfo != NULL) {
        uprv_memset(fFormatInfo, 0, sizeof(*fFormatInfo));
    }
    *this = other;
}

Win32NumberFormat::~Win32NumberFormat()
{
    if (fFormatInfo != NULL) {
        if (fCurrency) {
            freeCurrencyFormat(&fFormatInfo->currency);
        } else {
            freeNumberFormat(&fFormatInfo->number);
        }

        uprv_free(fFormatInfo);
    }
}

Win32NumberFormat &Win32NumberFormat::operator=(const Win32NumberFormat &other)
{
    NumberFormat::operator=(other);

    this->fCurrency          = other.fCurrency;
    this->fLocale            = other.fLocale;
    this->fLCID              = other.fLCID;
    this->fFractionDigitsSet = other.fFractionDigitsSet;

    if (fCurrency) {
        freeCurrencyFormat(&fFormatInfo->currency);
        getCurrencyFormat(&fFormatInfo->currency, fLCID);
    } else {
        freeNumberFormat(&fFormatInfo->number);
        getNumberFormat(&fFormatInfo->number, fLCID);
    }

    return *this;
}

Format *Win32NumberFormat::clone(void) const
{
    return new Win32NumberFormat(*this);
}

UnicodeString& Win32NumberFormat::format(double number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMaximumFractionDigits(), appendTo, L"%.16f", number);
}

UnicodeString& Win32NumberFormat::format(int32_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMinimumFractionDigits(), appendTo, L"%I32d", number);
}

UnicodeString& Win32NumberFormat::format(int64_t number, UnicodeString& appendTo, FieldPosition& pos) const
{
    return format(getMinimumFractionDigits(), appendTo, L"%I64d", number);
}

void Win32NumberFormat::parse(const UnicodeString& text, Formattable& result, ParsePosition& parsePosition) const
{
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat *nf = fCurrency? NumberFormat::createCurrencyInstance(fLocale, status) : NumberFormat::createInstance(fLocale, status);

    nf->parse(text, result, parsePosition);
    delete nf;
}
void Win32NumberFormat::setMaximumFractionDigits(int32_t newValue)
{
    fFractionDigitsSet = TRUE;
    NumberFormat::setMaximumFractionDigits(newValue);
}

void Win32NumberFormat::setMinimumFractionDigits(int32_t newValue)
{
    fFractionDigitsSet = TRUE;
    NumberFormat::setMinimumFractionDigits(newValue);
}

UnicodeString &Win32NumberFormat::format(int32_t numDigits, UnicodeString &appendTo, const wchar_t *fmt, ...) const
{
    wchar_t nStackBuffer[STACK_BUFFER_SIZE];
    wchar_t *nBuffer = nStackBuffer;
    va_list args;
    int result;

    nBuffer[0] = 0x0000;

    /* Due to the arguments causing a result to be <= 23 characters (+2 for NULL and minus),
    we don't need to reallocate the buffer. */
    va_start(args, fmt);
    result = _vsnwprintf(nBuffer, STACK_BUFFER_SIZE, fmt, args);
    va_end(args);

    /* Just to make sure of the above statement, we add this assert */
    U_ASSERT(result >=0);
    // The following code is not used because _vscwprintf isn't available on MinGW at the moment.
    /*if (result < 0) {
        int newLength;

        va_start(args, fmt);
        newLength = _vscwprintf(fmt, args);
        va_end(args);

        nBuffer = NEW_ARRAY(UChar, newLength + 1);

        va_start(args, fmt);
        result = _vsnwprintf(nBuffer, newLength + 1, fmt, args);
        va_end(args);
    }*/

    // vswprintf is sensitive to the locale set by setlocale. For some locales
    // it doesn't use "." as the decimal separator, which is what GetNumberFormatW
    // and GetCurrencyFormatW both expect to see.
    //
    // To fix this, we scan over the string and replace the first non-digits, except
    // for a leading "-", with a "."
    //
    // Note: (nBuffer[0] == L'-') will evaluate to 1 if there is a leading '-' in the
    // number, and 0 otherwise.
    for (wchar_t *p = &nBuffer[nBuffer[0] == L'-']; *p != L'\0'; p += 1) {
        if (*p < L'0' || *p > L'9') {
            *p = L'.';
            break;
        }
    }

    UChar stackBuffer[STACK_BUFFER_SIZE];
    UChar *buffer = stackBuffer;
    FormatInfo formatInfo;

    formatInfo = *fFormatInfo;
    buffer[0] = 0x0000;

    if (fCurrency) {
        if (fFractionDigitsSet) {
            formatInfo.currency.NumDigits = (UINT) numDigits;
        }

        if (!isGroupingUsed()) {
            formatInfo.currency.Grouping = 0;
        }

        result = GetCurrencyFormatW(fLCID, 0, nBuffer, &formatInfo.currency, buffer, STACK_BUFFER_SIZE);

        if (result == 0) {
            DWORD lastError = GetLastError();

            if (lastError == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetCurrencyFormatW(fLCID, 0, nBuffer, &formatInfo.currency, NULL, 0);

                buffer = NEW_ARRAY(UChar, newLength);
                buffer[0] = 0x0000;
                GetCurrencyFormatW(fLCID, 0, nBuffer,  &formatInfo.currency, buffer, newLength);
            }
        }
    } else {
        if (fFractionDigitsSet) {
            formatInfo.number.NumDigits = (UINT) numDigits;
        }

        if (!isGroupingUsed()) {
            formatInfo.number.Grouping = 0;
        }

        result = GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, buffer, STACK_BUFFER_SIZE);

        if (result == 0) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                int newLength = GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, NULL, 0);

                buffer = NEW_ARRAY(UChar, newLength);
                buffer[0] = 0x0000;
                GetNumberFormatW(fLCID, 0, nBuffer, &formatInfo.number, buffer, newLength);
            }
        }
    }

    appendTo.append(buffer, (int32_t) wcslen(buffer));

    if (buffer != stackBuffer) {
        DELETE_ARRAY(buffer);
    }

    /*if (nBuffer != nStackBuffer) {
        DELETE_ARRAY(nBuffer);
    }*/

    return appendTo;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM_USES_ONLY_WIN32_API
