// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DCFMTSYM.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/18/97    clhuang     Implemented with C++ APIs.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   08/26/97    aliu        Added currency/intl currency symbol support.
*   07/20/98    stephen     Slightly modified initialization of monetarySeparator
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/ures.h"
#include "unicode/decimfmt.h"
#include "unicode/ucurr.h"
#include "unicode/choicfmt.h"
#include "unicode/unistr.h"
#include "unicode/numsys.h"
#include "unicode/unum.h"
#include "unicode/utf16.h"
#include "ucurrimp.h"
#include "cstring.h"
#include "locbased.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "charstr.h"
#include "uassert.h"

// *****************************************************************************
// class DecimalFormatSymbols
// *****************************************************************************

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DecimalFormatSymbols)

static const char gNumberElements[] = "NumberElements";
static const char gCurrencySpacingTag[] = "currencySpacing";
static const char gBeforeCurrencyTag[] = "beforeCurrency";
static const char gAfterCurrencyTag[] = "afterCurrency";
static const char gCurrencyMatchTag[] = "currencyMatch";
static const char gCurrencySudMatchTag[] = "surroundingMatch";
static const char gCurrencyInsertBtnTag[] = "insertBetween";
static const char gLatn[] =  "latn";
static const char gSymbols[] = "symbols";
static const char gNumberElementsLatnSymbols[] = "NumberElements/latn/symbols";

static const UChar INTL_CURRENCY_SYMBOL_STR[] = {0xa4, 0xa4, 0};

// List of field names to be loaded from the data files.
// These are parallel with the enum ENumberFormatSymbol in unicode/dcfmtsym.h.
static const char *gNumberElementKeys[DecimalFormatSymbols::kFormatSymbolCount] = {
    "decimal",
    "group",
    NULL, /* #11897: the <list> symbol is NOT the pattern separator symbol */
    "percentSign",
    NULL, /* Native zero digit is deprecated from CLDR - get it from the numbering system */
    NULL, /* Pattern digit character is deprecated from CLDR - use # by default always */
    "minusSign",
    "plusSign",
    NULL, /* currency symbol - Wait until we know the currency before loading from CLDR */
    NULL, /* intl currency symbol - Wait until we know the currency before loading from CLDR */
    "currencyDecimal",
    "exponential",
    "perMille",
    NULL, /* Escape padding character - not in CLDR */
    "infinity",
    "nan",
    NULL, /* Significant digit symbol - not in CLDR */
    "currencyGroup",
    NULL, /* one digit - get it from the numbering system */
    NULL, /* two digit - get it from the numbering system */
    NULL, /* three digit - get it from the numbering system */
    NULL, /* four digit - get it from the numbering system */
    NULL, /* five digit - get it from the numbering system */
    NULL, /* six digit - get it from the numbering system */
    NULL, /* seven digit - get it from the numbering system */
    NULL, /* eight digit - get it from the numbering system */
    NULL, /* nine digit - get it from the numbering system */
    "superscriptingExponent", /* Multiplication (x) symbol for exponents */
};

// -------------------------------------
// Initializes this with the decimal format symbols in the default locale.

DecimalFormatSymbols::DecimalFormatSymbols(UErrorCode& status)
        : UObject(), locale(), currPattern(NULL) {
    initialize(locale, status, TRUE);
}

// -------------------------------------
// Initializes this with the decimal format symbols in the desired locale.

DecimalFormatSymbols::DecimalFormatSymbols(const Locale& loc, UErrorCode& status)
        : UObject(), locale(loc), currPattern(NULL) {
    initialize(locale, status);
}

DecimalFormatSymbols::DecimalFormatSymbols(const Locale& loc, const NumberingSystem& ns, UErrorCode& status)
        : UObject(), locale(loc), currPattern(NULL) {
    initialize(locale, status, FALSE, &ns);
}

DecimalFormatSymbols::DecimalFormatSymbols()
        : UObject(), locale(Locale::getRoot()), currPattern(NULL) {
    *validLocale = *actualLocale = 0;
    initialize();
}

DecimalFormatSymbols*
DecimalFormatSymbols::createWithLastResortData(UErrorCode& status) {
    if (U_FAILURE(status)) { return NULL; }
    DecimalFormatSymbols* sym = new DecimalFormatSymbols();
    if (sym == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return sym;
}

// -------------------------------------

DecimalFormatSymbols::~DecimalFormatSymbols()
{
}

// -------------------------------------
// copy constructor

DecimalFormatSymbols::DecimalFormatSymbols(const DecimalFormatSymbols &source)
    : UObject(source)
{
    *this = source;
}

// -------------------------------------
// assignment operator

DecimalFormatSymbols&
DecimalFormatSymbols::operator=(const DecimalFormatSymbols& rhs)
{
    if (this != &rhs) {
        for(int32_t i = 0; i < (int32_t)kFormatSymbolCount; ++i) {
            // fastCopyFrom is safe, see docs on fSymbols
            fSymbols[(ENumberFormatSymbol)i].fastCopyFrom(rhs.fSymbols[(ENumberFormatSymbol)i]);
        }
        for(int32_t i = 0; i < (int32_t)UNUM_CURRENCY_SPACING_COUNT; ++i) {
            currencySpcBeforeSym[i].fastCopyFrom(rhs.currencySpcBeforeSym[i]);
            currencySpcAfterSym[i].fastCopyFrom(rhs.currencySpcAfterSym[i]);
        }
        locale = rhs.locale;
        uprv_strcpy(validLocale, rhs.validLocale);
        uprv_strcpy(actualLocale, rhs.actualLocale);
        fIsCustomCurrencySymbol = rhs.fIsCustomCurrencySymbol;
        fIsCustomIntlCurrencySymbol = rhs.fIsCustomIntlCurrencySymbol;
        fCodePointZero = rhs.fCodePointZero;
    }
    return *this;
}

// -------------------------------------

UBool
DecimalFormatSymbols::operator==(const DecimalFormatSymbols& that) const
{
    if (this == &that) {
        return TRUE;
    }
    if (fIsCustomCurrencySymbol != that.fIsCustomCurrencySymbol) {
        return FALSE;
    }
    if (fIsCustomIntlCurrencySymbol != that.fIsCustomIntlCurrencySymbol) {
        return FALSE;
    }
    for(int32_t i = 0; i < (int32_t)kFormatSymbolCount; ++i) {
        if(fSymbols[(ENumberFormatSymbol)i] != that.fSymbols[(ENumberFormatSymbol)i]) {
            return FALSE;
        }
    }
    for(int32_t i = 0; i < (int32_t)UNUM_CURRENCY_SPACING_COUNT; ++i) {
        if(currencySpcBeforeSym[i] != that.currencySpcBeforeSym[i]) {
            return FALSE;
        }
        if(currencySpcAfterSym[i] != that.currencySpcAfterSym[i]) {
            return FALSE;
        }
    }
    // No need to check fCodePointZero since it is based on fSymbols
    return locale == that.locale &&
        uprv_strcmp(validLocale, that.validLocale) == 0 &&
        uprv_strcmp(actualLocale, that.actualLocale) == 0;
}

// -------------------------------------

namespace {

/**
 * Sink for enumerating all of the decimal format symbols (more specifically, anything
 * under the "NumberElements.symbols" tree).
 *
 * More specific bundles (en_GB) are enumerated before their parents (en_001, en, root):
 * Only store a value if it is still missing, that is, it has not been overridden.
 */
struct DecFmtSymDataSink : public ResourceSink {

    // Destination for data, modified via setters.
    DecimalFormatSymbols& dfs;
    // Boolean array of whether or not we have seen a particular symbol yet.
    // Can't simpy check fSymbols because it is pre-populated with defaults.
    UBool seenSymbol[DecimalFormatSymbols::kFormatSymbolCount];

    // Constructor/Destructor
    DecFmtSymDataSink(DecimalFormatSymbols& _dfs) : dfs(_dfs) {
        uprv_memset(seenSymbol, FALSE, sizeof(seenSymbol));
    }
    virtual ~DecFmtSymDataSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        ResourceTable symbolsTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t j = 0; symbolsTable.getKeyAndValue(j, key, value); ++j) {
            for (int32_t i=0; i<DecimalFormatSymbols::kFormatSymbolCount; i++) {
                if (gNumberElementKeys[i] != NULL && uprv_strcmp(key, gNumberElementKeys[i]) == 0) {
                    if (!seenSymbol[i]) {
                        seenSymbol[i] = TRUE;
                        dfs.setSymbol(
                            (DecimalFormatSymbols::ENumberFormatSymbol) i,
                            value.getUnicodeString(errorCode));
                        if (U_FAILURE(errorCode)) { return; }
                    }
                    break;
                }
            }
        }
    }

    // Returns true if all the symbols have been seen.
    UBool seenAll() {
        for (int32_t i=0; i<DecimalFormatSymbols::kFormatSymbolCount; i++) {
            if (!seenSymbol[i]) {
                return FALSE;
            }
        }
        return TRUE;
    }

    // If monetary decimal or grouping were not explicitly set, then set them to be the
    // same as their non-monetary counterparts.
    void resolveMissingMonetarySeparators(const UnicodeString* fSymbols) {
        if (!seenSymbol[DecimalFormatSymbols::kMonetarySeparatorSymbol]) {
            dfs.setSymbol(
                DecimalFormatSymbols::kMonetarySeparatorSymbol,
                fSymbols[DecimalFormatSymbols::kDecimalSeparatorSymbol]);
        }
        if (!seenSymbol[DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol]) {
            dfs.setSymbol(
                DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol,
                fSymbols[DecimalFormatSymbols::kGroupingSeparatorSymbol]);
        }
    }
};

struct CurrencySpacingSink : public ResourceSink {
    DecimalFormatSymbols& dfs;
    UBool hasBeforeCurrency;
    UBool hasAfterCurrency;

    CurrencySpacingSink(DecimalFormatSymbols& _dfs)
        : dfs(_dfs), hasBeforeCurrency(FALSE), hasAfterCurrency(FALSE) {}
    virtual ~CurrencySpacingSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        ResourceTable spacingTypesTable = value.getTable(errorCode);
        for (int32_t i = 0; spacingTypesTable.getKeyAndValue(i, key, value); ++i) {
            UBool beforeCurrency;
            if (uprv_strcmp(key, gBeforeCurrencyTag) == 0) {
                beforeCurrency = TRUE;
                hasBeforeCurrency = TRUE;
            } else if (uprv_strcmp(key, gAfterCurrencyTag) == 0) {
                beforeCurrency = FALSE;
                hasAfterCurrency = TRUE;
            } else {
                continue;
            }

            ResourceTable patternsTable = value.getTable(errorCode);
            for (int32_t j = 0; patternsTable.getKeyAndValue(j, key, value); ++j) {
                UCurrencySpacing pattern;
                if (uprv_strcmp(key, gCurrencyMatchTag) == 0) {
                    pattern = UNUM_CURRENCY_MATCH;
                } else if (uprv_strcmp(key, gCurrencySudMatchTag) == 0) {
                    pattern = UNUM_CURRENCY_SURROUNDING_MATCH;
                } else if (uprv_strcmp(key, gCurrencyInsertBtnTag) == 0) {
                    pattern = UNUM_CURRENCY_INSERT;
                } else {
                    continue;
                }

                const UnicodeString& current = dfs.getPatternForCurrencySpacing(
                    pattern, beforeCurrency, errorCode);
                if (current.isEmpty()) {
                    dfs.setPatternForCurrencySpacing(
                        pattern, beforeCurrency, value.getUnicodeString(errorCode));
                }
            }
        }
    }

    void resolveMissing() {
        // For consistency with Java, this method overwrites everything with the defaults unless
        // both beforeCurrency and afterCurrency were found in CLDR.
        static const char* defaults[] = { "[:letter:]", "[:digit:]", " " };
        if (!hasBeforeCurrency || !hasAfterCurrency) {
            for (UBool beforeCurrency = 0; beforeCurrency <= TRUE; beforeCurrency++) {
                for (int32_t pattern = 0; pattern < UNUM_CURRENCY_SPACING_COUNT; pattern++) {
                    dfs.setPatternForCurrencySpacing((UCurrencySpacing)pattern,
                        beforeCurrency, UnicodeString(defaults[pattern], -1, US_INV));
                }
            }
        }
    }
};

// Virtual destructors must be defined out of line.
DecFmtSymDataSink::~DecFmtSymDataSink() {}
CurrencySpacingSink::~CurrencySpacingSink() {}

} // namespace

void
DecimalFormatSymbols::initialize(const Locale& loc, UErrorCode& status,
    UBool useLastResortData, const NumberingSystem* ns)
{
    if (U_FAILURE(status)) { return; }
    *validLocale = *actualLocale = 0;

    // First initialize all the symbols to the fallbacks for anything we can't find
    initialize();

    //
    // Next get the numbering system for this locale and set zero digit
    // and the digit string based on the numbering system for the locale
    //
    LocalPointer<NumberingSystem> nsLocal;
    if (ns == nullptr) {
        // Use the numbering system according to the locale.
        // Save it into a LocalPointer so it gets cleaned up.
        nsLocal.adoptInstead(NumberingSystem::createInstance(loc, status));
        ns = nsLocal.getAlias();
    }
    const char *nsName;
    if (U_SUCCESS(status) && ns->getRadix() == 10 && !ns->isAlgorithmic()) {
        nsName = ns->getName();
        UnicodeString digitString(ns->getDescription());
        int32_t digitIndex = 0;
        UChar32 digit = digitString.char32At(0);
        fSymbols[kZeroDigitSymbol].setTo(digit);
        for (int32_t i = kOneDigitSymbol; i <= kNineDigitSymbol; ++i) {
            digitIndex += U16_LENGTH(digit);
            digit = digitString.char32At(digitIndex);
            fSymbols[i].setTo(digit);
        }
    } else {
        nsName = gLatn;
    }

    // Open resource bundles
    const char* locStr = loc.getName();
    LocalUResourceBundlePointer resource(ures_open(NULL, locStr, &status));
    LocalUResourceBundlePointer numberElementsRes(
        ures_getByKeyWithFallback(resource.getAlias(), gNumberElements, NULL, &status));

    if (U_FAILURE(status)) {
        if ( useLastResortData ) {
            status = U_USING_DEFAULT_WARNING;
            initialize();
        }
        return;
    }

    // Set locale IDs
    // TODO: Is there a way to do this without depending on the resource bundle instance?
    U_LOCALE_BASED(locBased, *this);
    locBased.setLocaleIDs(
        ures_getLocaleByType(
            numberElementsRes.getAlias(),
            ULOC_VALID_LOCALE, &status),
        ures_getLocaleByType(
            numberElementsRes.getAlias(),
            ULOC_ACTUAL_LOCALE, &status));

    // Now load the rest of the data from the data sink.
    // Start with loading this nsName if it is not Latin.
    DecFmtSymDataSink sink(*this);
    if (uprv_strcmp(nsName, gLatn) != 0) {
        CharString path;
        path.append(gNumberElements, status)
            .append('/', status)
            .append(nsName, status)
            .append('/', status)
            .append(gSymbols, status);
        ures_getAllItemsWithFallback(resource.getAlias(), path.data(), sink, status);

        // If no symbols exist for the given nsName and resource bundle, silently ignore
        // and fall back to Latin.
        if (status == U_MISSING_RESOURCE_ERROR) {
            status = U_ZERO_ERROR;
        } else if (U_FAILURE(status)) {
            return;
        }
    }

    // Continue with Latin if necessary.
    if (!sink.seenAll()) {
        ures_getAllItemsWithFallback(resource.getAlias(), gNumberElementsLatnSymbols, sink, status);
        if (U_FAILURE(status)) { return; }
    }

    // Let the monetary number separators equal the default number separators if necessary.
    sink.resolveMissingMonetarySeparators(fSymbols);

    // Resolve codePointZero
    UChar32 tempCodePointZero = -1;
    for (int32_t i=0; i<=9; i++) {
        const UnicodeString& stringDigit = getConstDigitSymbol(i);
        if (stringDigit.countChar32() != 1) {
            tempCodePointZero = -1;
            break;
        }
        UChar32 cp = stringDigit.char32At(0);
        if (i == 0) {
            tempCodePointZero = cp;
        } else if (cp != tempCodePointZero + i) {
            tempCodePointZero = -1;
            break;
        }
    }
    fCodePointZero = tempCodePointZero;

    // Obtain currency data from the currency API.  This is strictly
    // for backward compatibility; we don't use DecimalFormatSymbols
    // for currency data anymore.
    UErrorCode internalStatus = U_ZERO_ERROR; // don't propagate failures out
    UChar curriso[4];
    UnicodeString tempStr;
    int32_t currisoLength = ucurr_forLocale(locStr, curriso, UPRV_LENGTHOF(curriso), &internalStatus);
    if (U_SUCCESS(internalStatus) && currisoLength == 3) {
        uprv_getStaticCurrencyName(curriso, locStr, tempStr, internalStatus);
        if (U_SUCCESS(internalStatus)) {
            fSymbols[kIntlCurrencySymbol].setTo(curriso, currisoLength);
            fSymbols[kCurrencySymbol] = tempStr;
        }
    }
    /* else use the default values. */

    //load the currency data
    UChar ucc[4]={0}; //Currency Codes are always 3 chars long
    int32_t uccLen = 4;
    const char* locName = loc.getName();
    UErrorCode localStatus = U_ZERO_ERROR;
    uccLen = ucurr_forLocale(locName, ucc, uccLen, &localStatus);

    // TODO: Currency pattern data loading is duplicated in number_formatimpl.cpp
    if(U_SUCCESS(localStatus) && uccLen > 0) {
        char cc[4]={0};
        u_UCharsToChars(ucc, cc, uccLen);
        /* An explicit currency was requested */
        LocalUResourceBundlePointer currencyResource(ures_open(U_ICUDATA_CURR, locStr, &localStatus));
        LocalUResourceBundlePointer currency(
            ures_getByKeyWithFallback(currencyResource.getAlias(), "Currencies", NULL, &localStatus));
        ures_getByKeyWithFallback(currency.getAlias(), cc, currency.getAlias(), &localStatus);
        if(U_SUCCESS(localStatus) && ures_getSize(currency.getAlias())>2) { // the length is 3 if more data is present
            ures_getByIndex(currency.getAlias(), 2, currency.getAlias(), &localStatus);
            int32_t currPatternLen = 0;
            currPattern =
                ures_getStringByIndex(currency.getAlias(), (int32_t)0, &currPatternLen, &localStatus);
            UnicodeString decimalSep =
                ures_getUnicodeStringByIndex(currency.getAlias(), (int32_t)1, &localStatus);
            UnicodeString groupingSep =
                ures_getUnicodeStringByIndex(currency.getAlias(), (int32_t)2, &localStatus);
            if(U_SUCCESS(localStatus)){
                fSymbols[kMonetaryGroupingSeparatorSymbol] = groupingSep;
                fSymbols[kMonetarySeparatorSymbol] = decimalSep;
                //pattern.setTo(TRUE, currPattern, currPatternLen);
                status = localStatus;
            }
        }
        /* else An explicit currency was requested and is unknown or locale data is malformed. */
        /* ucurr_* API will get the correct value later on. */
    }
        // else ignore the error if no currency

    // Currency Spacing.
    LocalUResourceBundlePointer currencyResource(ures_open(U_ICUDATA_CURR, locStr, &status));
    CurrencySpacingSink currencySink(*this);
    ures_getAllItemsWithFallback(currencyResource.getAlias(), gCurrencySpacingTag, currencySink, status);
    currencySink.resolveMissing();
    if (U_FAILURE(status)) { return; }
}

void
DecimalFormatSymbols::initialize() {
    /*
     * These strings used to be in static arrays, but the HP/UX aCC compiler
     * cannot initialize a static array with class constructors.
     *  markus 2000may25
     */
    fSymbols[kDecimalSeparatorSymbol] = (UChar)0x2e;    // '.' decimal separator
    fSymbols[kGroupingSeparatorSymbol].remove();        //     group (thousands) separator
    fSymbols[kPatternSeparatorSymbol] = (UChar)0x3b;    // ';' pattern separator
    fSymbols[kPercentSymbol] = (UChar)0x25;             // '%' percent sign
    fSymbols[kZeroDigitSymbol] = (UChar)0x30;           // '0' native 0 digit
    fSymbols[kOneDigitSymbol] = (UChar)0x31;            // '1' native 1 digit
    fSymbols[kTwoDigitSymbol] = (UChar)0x32;            // '2' native 2 digit
    fSymbols[kThreeDigitSymbol] = (UChar)0x33;          // '3' native 3 digit
    fSymbols[kFourDigitSymbol] = (UChar)0x34;           // '4' native 4 digit
    fSymbols[kFiveDigitSymbol] = (UChar)0x35;           // '5' native 5 digit
    fSymbols[kSixDigitSymbol] = (UChar)0x36;            // '6' native 6 digit
    fSymbols[kSevenDigitSymbol] = (UChar)0x37;          // '7' native 7 digit
    fSymbols[kEightDigitSymbol] = (UChar)0x38;          // '8' native 8 digit
    fSymbols[kNineDigitSymbol] = (UChar)0x39;           // '9' native 9 digit
    fSymbols[kDigitSymbol] = (UChar)0x23;               // '#' pattern digit
    fSymbols[kPlusSignSymbol] = (UChar)0x002b;          // '+' plus sign
    fSymbols[kMinusSignSymbol] = (UChar)0x2d;           // '-' minus sign
    fSymbols[kCurrencySymbol] = (UChar)0xa4;            // 'OX' currency symbol
    fSymbols[kIntlCurrencySymbol].setTo(TRUE, INTL_CURRENCY_SYMBOL_STR, 2);
    fSymbols[kMonetarySeparatorSymbol] = (UChar)0x2e;   // '.' monetary decimal separator
    fSymbols[kExponentialSymbol] = (UChar)0x45;         // 'E' exponential
    fSymbols[kPerMillSymbol] = (UChar)0x2030;           // '%o' per mill
    fSymbols[kPadEscapeSymbol] = (UChar)0x2a;           // '*' pad escape symbol
    fSymbols[kInfinitySymbol] = (UChar)0x221e;          // 'oo' infinite
    fSymbols[kNaNSymbol] = (UChar)0xfffd;               // SUB NaN
    fSymbols[kSignificantDigitSymbol] = (UChar)0x0040;  // '@' significant digit
    fSymbols[kMonetaryGroupingSeparatorSymbol].remove(); //
    fSymbols[kExponentMultiplicationSymbol] = (UChar)0xd7; // 'x' multiplication symbol for exponents
    fIsCustomCurrencySymbol = FALSE;
    fIsCustomIntlCurrencySymbol = FALSE;
    fCodePointZero = 0x30;
    U_ASSERT(fCodePointZero == fSymbols[kZeroDigitSymbol].char32At(0));

}

Locale
DecimalFormatSymbols::getLocale(ULocDataLocaleType type, UErrorCode& status) const {
    U_LOCALE_BASED(locBased, *this);
    return locBased.getLocale(type, status);
}

const UnicodeString&
DecimalFormatSymbols::getPatternForCurrencySpacing(UCurrencySpacing type,
                                                 UBool beforeCurrency,
                                                 UErrorCode& status) const {
    if (U_FAILURE(status)) {
      return fNoSymbol;  // always empty.
    }
    if (beforeCurrency) {
      return currencySpcBeforeSym[(int32_t)type];
    } else {
      return currencySpcAfterSym[(int32_t)type];
    }
}

void
DecimalFormatSymbols::setPatternForCurrencySpacing(UCurrencySpacing type,
                                                   UBool beforeCurrency,
                                             const UnicodeString& pattern) {
  if (beforeCurrency) {
    currencySpcBeforeSym[(int32_t)type] = pattern;
  } else {
    currencySpcAfterSym[(int32_t)type] =  pattern;
  }
}
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
