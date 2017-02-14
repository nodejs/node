// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File DECIMFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/20/97    clhuang     Implemented with new APIs.
*   03/31/97    aliu        Moved isLONG_MIN to DigitList, and fixed it.
*   04/3/97     aliu        Rewrote parsing and formatting completely, and
*                           cleaned up and debugged.  Actually works now.
*                           Implemented NAN and INF handling, for both parsing
*                           and formatting.  Extensive testing & debugging.
*   04/10/97    aliu        Modified to compile on AIX.
*   04/16/97    aliu        Rewrote to use DigitList, which has been resurrected.
*                           Changed DigitCount to int per code review.
*   07/09/97    helena      Made ParsePosition into a class.
*   08/26/97    aliu        Extensive changes to applyPattern; completely
*                           rewritten from the Java.
*   09/09/97    aliu        Ported over support for exponential formats.
*   07/20/98    stephen     JDK 1.2 sync up.
*                             Various instances of '0' replaced with 'NULL'
*                             Check for grouping size in subFormat()
*                             Brought subParse() in line with Java 1.2
*                             Added method appendAffix()
*   08/24/1998  srl         Removed Mutex calls. This is not a thread safe class!
*   02/22/99    stephen     Removed character literals for EBCDIC safety
*   06/24/99    helena      Integrated Alan's NF enhancements and Java2 bug fixes
*   06/28/99    stephen     Fixed bugs in toPattern().
*   06/29/99    stephen     Fixed operator= to copy fFormatWidth, fPad,
*                             fPadPosition
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uniset.h"
#include "unicode/currpinf.h"
#include "unicode/plurrule.h"
#include "unicode/utf16.h"
#include "unicode/numsys.h"
#include "unicode/localpointer.h"
#include "uresimp.h"
#include "ucurrimp.h"
#include "charstr.h"
#include "patternprops.h"
#include "cstring.h"
#include "uassert.h"
#include "hash.h"
#include "decfmtst.h"
#include "plurrule_impl.h"
#include "decimalformatpattern.h"
#include "fmtableimp.h"
#include "decimfmtimpl.h"
#include "visibledigits.h"

/*
 * On certain platforms, round is a macro defined in math.h
 * This undefine is to avoid conflict between the macro and
 * the function defined below.
 */
#ifdef round
#undef round
#endif


U_NAMESPACE_BEGIN

#ifdef FMT_DEBUG
#include <stdio.h>
static void _debugout(const char *f, int l, const UnicodeString& s) {
    char buf[2000];
    s.extract((int32_t) 0, s.length(), buf, "utf-8");
    printf("%s:%d: %s\n", f,l, buf);
}
#define debugout(x) _debugout(__FILE__,__LINE__,x)
#define debug(x) printf("%s:%d: %s\n", __FILE__,__LINE__, x);
static const UnicodeString dbg_null("<NULL>","");
#define DEREFSTR(x)   ((x!=NULL)?(*x):(dbg_null))
#else
#define debugout(x)
#define debug(x)
#endif


/* For currency parsing purose,
 * Need to remember all prefix patterns and suffix patterns of
 * every currency format pattern,
 * including the pattern of default currecny style
 * and plural currency style. And the patterns are set through applyPattern.
 */
struct AffixPatternsForCurrency : public UMemory {
	// negative prefix pattern
	UnicodeString negPrefixPatternForCurrency;
	// negative suffix pattern
	UnicodeString negSuffixPatternForCurrency;
	// positive prefix pattern
	UnicodeString posPrefixPatternForCurrency;
	// positive suffix pattern
	UnicodeString posSuffixPatternForCurrency;
	int8_t patternType;

	AffixPatternsForCurrency(const UnicodeString& negPrefix,
							 const UnicodeString& negSuffix,
							 const UnicodeString& posPrefix,
							 const UnicodeString& posSuffix,
							 int8_t type) {
		negPrefixPatternForCurrency = negPrefix;
		negSuffixPatternForCurrency = negSuffix;
		posPrefixPatternForCurrency = posPrefix;
		posSuffixPatternForCurrency = posSuffix;
		patternType = type;
	}
#ifdef FMT_DEBUG
  void dump() const  {
    debugout( UnicodeString("AffixPatternsForCurrency( -=\"") +
              negPrefixPatternForCurrency + (UnicodeString)"\"/\"" +
              negSuffixPatternForCurrency + (UnicodeString)"\" +=\"" +
              posPrefixPatternForCurrency + (UnicodeString)"\"/\"" +
              posSuffixPatternForCurrency + (UnicodeString)"\" )");
  }
#endif
};

/* affix for currency formatting when the currency sign in the pattern
 * equals to 3, such as the pattern contains 3 currency sign or
 * the formatter style is currency plural format style.
 */
struct AffixesForCurrency : public UMemory {
	// negative prefix
	UnicodeString negPrefixForCurrency;
	// negative suffix
	UnicodeString negSuffixForCurrency;
	// positive prefix
	UnicodeString posPrefixForCurrency;
	// positive suffix
	UnicodeString posSuffixForCurrency;

	int32_t formatWidth;

	AffixesForCurrency(const UnicodeString& negPrefix,
					   const UnicodeString& negSuffix,
					   const UnicodeString& posPrefix,
					   const UnicodeString& posSuffix) {
		negPrefixForCurrency = negPrefix;
		negSuffixForCurrency = negSuffix;
		posPrefixForCurrency = posPrefix;
		posSuffixForCurrency = posSuffix;
	}
#ifdef FMT_DEBUG
  void dump() const {
    debugout( UnicodeString("AffixesForCurrency( -=\"") +
              negPrefixForCurrency + (UnicodeString)"\"/\"" +
              negSuffixForCurrency + (UnicodeString)"\" +=\"" +
              posPrefixForCurrency + (UnicodeString)"\"/\"" +
              posSuffixForCurrency + (UnicodeString)"\" )");
  }
#endif
};

U_CDECL_BEGIN

/**
 * @internal ICU 4.2
 */
static UBool U_CALLCONV decimfmtAffixPatternValueComparator(UHashTok val1, UHashTok val2);


static UBool
U_CALLCONV decimfmtAffixPatternValueComparator(UHashTok val1, UHashTok val2) {
    const AffixPatternsForCurrency* affix_1 =
        (AffixPatternsForCurrency*)val1.pointer;
    const AffixPatternsForCurrency* affix_2 =
        (AffixPatternsForCurrency*)val2.pointer;
    return affix_1->negPrefixPatternForCurrency ==
           affix_2->negPrefixPatternForCurrency &&
           affix_1->negSuffixPatternForCurrency ==
           affix_2->negSuffixPatternForCurrency &&
           affix_1->posPrefixPatternForCurrency ==
           affix_2->posPrefixPatternForCurrency &&
           affix_1->posSuffixPatternForCurrency ==
           affix_2->posSuffixPatternForCurrency &&
           affix_1->patternType == affix_2->patternType;
}

U_CDECL_END




// *****************************************************************************
// class DecimalFormat
// *****************************************************************************

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DecimalFormat)

// Constants for characters used in programmatic (unlocalized) patterns.
#define kPatternZeroDigit            ((UChar)0x0030) /*'0'*/
#define kPatternSignificantDigit     ((UChar)0x0040) /*'@'*/
#define kPatternGroupingSeparator    ((UChar)0x002C) /*','*/
#define kPatternDecimalSeparator     ((UChar)0x002E) /*'.'*/
#define kPatternPerMill              ((UChar)0x2030)
#define kPatternPercent              ((UChar)0x0025) /*'%'*/
#define kPatternDigit                ((UChar)0x0023) /*'#'*/
#define kPatternSeparator            ((UChar)0x003B) /*';'*/
#define kPatternExponent             ((UChar)0x0045) /*'E'*/
#define kPatternPlus                 ((UChar)0x002B) /*'+'*/
#define kPatternMinus                ((UChar)0x002D) /*'-'*/
#define kPatternPadEscape            ((UChar)0x002A) /*'*'*/
#define kQuote                       ((UChar)0x0027) /*'\''*/
/**
 * The CURRENCY_SIGN is the standard Unicode symbol for currency.  It
 * is used in patterns and substitued with either the currency symbol,
 * or if it is doubled, with the international currency symbol.  If the
 * CURRENCY_SIGN is seen in a pattern, then the decimal separator is
 * replaced with the monetary decimal separator.
 */
#define kCurrencySign                ((UChar)0x00A4)
#define kDefaultPad                  ((UChar)0x0020) /* */

const int32_t DecimalFormat::kDoubleIntegerDigits  = 309;
const int32_t DecimalFormat::kDoubleFractionDigits = 340;

const int32_t DecimalFormat::kMaxScientificIntegerDigits = 8;

/**
 * These are the tags we expect to see in normal resource bundle files associated
 * with a locale.
 */
const char DecimalFormat::fgNumberPatterns[]="NumberPatterns"; // Deprecated - not used
static const char fgNumberElements[]="NumberElements";
static const char fgLatn[]="latn";
static const char fgPatterns[]="patterns";
static const char fgDecimalFormat[]="decimalFormat";
static const char fgCurrencyFormat[]="currencyFormat";

inline int32_t _min(int32_t a, int32_t b) { return (a<b) ? a : b; }
inline int32_t _max(int32_t a, int32_t b) { return (a<b) ? b : a; }

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance in the default locale.

DecimalFormat::DecimalFormat(UErrorCode& status) {
    init();
    UParseError parseError;
    construct(status, parseError);
}

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance with the specified number format
// pattern in the default locale.

DecimalFormat::DecimalFormat(const UnicodeString& pattern,
                             UErrorCode& status) {
    init();
    UParseError parseError;
    construct(status, parseError, &pattern);
}

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance with the specified number format
// pattern and the number format symbols in the default locale.  The
// created instance owns the symbols.

DecimalFormat::DecimalFormat(const UnicodeString& pattern,
                             DecimalFormatSymbols* symbolsToAdopt,
                             UErrorCode& status) {
    init();
    UParseError parseError;
    if (symbolsToAdopt == NULL)
        status = U_ILLEGAL_ARGUMENT_ERROR;
    construct(status, parseError, &pattern, symbolsToAdopt);
}

DecimalFormat::DecimalFormat(  const UnicodeString& pattern,
                    DecimalFormatSymbols* symbolsToAdopt,
                    UParseError& parseErr,
                    UErrorCode& status) {
    init();
    if (symbolsToAdopt == NULL)
        status = U_ILLEGAL_ARGUMENT_ERROR;
    construct(status,parseErr, &pattern, symbolsToAdopt);
}

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance with the specified number format
// pattern and the number format symbols in the default locale.  The
// created instance owns the clone of the symbols.

DecimalFormat::DecimalFormat(const UnicodeString& pattern,
                             const DecimalFormatSymbols& symbols,
                             UErrorCode& status) {
    init();
    UParseError parseError;
    construct(status, parseError, &pattern, new DecimalFormatSymbols(symbols));
}

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance with the specified number format
// pattern, the number format symbols, and the number format style.
// The created instance owns the clone of the symbols.

DecimalFormat::DecimalFormat(const UnicodeString& pattern,
                             DecimalFormatSymbols* symbolsToAdopt,
                             UNumberFormatStyle style,
                             UErrorCode& status) {
    init();
    fStyle = style;
    UParseError parseError;
    construct(status, parseError, &pattern, symbolsToAdopt);
}

//-----------------------------------------------------------------------------
// Common DecimalFormat initialization.
//    Put all fields of an uninitialized object into a known state.
//    Common code, shared by all constructors.
//    Can not fail. Leave the object in good enough shape that the destructor
//    or assignment operator can run successfully.
void
DecimalFormat::init() {
    fBoolFlags.clear();
    fStyle = UNUM_DECIMAL;
    fAffixPatternsForCurrency = NULL;
    fCurrencyPluralInfo = NULL;
#if UCONFIG_HAVE_PARSEALLINPUT
    fParseAllInput = UNUM_MAYBE;
#endif

    fStaticSets = NULL;
    fImpl = NULL;
}

//------------------------------------------------------------------------------
// Constructs a DecimalFormat instance with the specified number format
// pattern and the number format symbols in the desired locale.  The
// created instance owns the symbols.

void
DecimalFormat::construct(UErrorCode&            status,
                         UParseError&           parseErr,
                         const UnicodeString*   pattern,
                         DecimalFormatSymbols*  symbolsToAdopt)
{
    LocalPointer<DecimalFormatSymbols> adoptedSymbols(symbolsToAdopt);
    if (U_FAILURE(status))
        return;

    if (adoptedSymbols.isNull())
    {
        adoptedSymbols.adoptInstead(
                new DecimalFormatSymbols(Locale::getDefault(), status));
        if (adoptedSymbols.isNull() && U_SUCCESS(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        if (U_FAILURE(status)) {
            return;
        }
    }
    fStaticSets = DecimalFormatStaticSets::getStaticSets(status);
    if (U_FAILURE(status)) {
        return;
    }

    UnicodeString str;
    // Uses the default locale's number format pattern if there isn't
    // one specified.
    if (pattern == NULL)
    {
        UErrorCode nsStatus = U_ZERO_ERROR;
        LocalPointer<NumberingSystem> ns(
                NumberingSystem::createInstance(nsStatus));
        if (U_FAILURE(nsStatus)) {
            status = nsStatus;
            return;
        }

        int32_t len = 0;
        UResourceBundle *top = ures_open(NULL, Locale::getDefault().getName(), &status);

        UResourceBundle *resource = ures_getByKeyWithFallback(top, fgNumberElements, NULL, &status);
        resource = ures_getByKeyWithFallback(resource, ns->getName(), resource, &status);
        resource = ures_getByKeyWithFallback(resource, fgPatterns, resource, &status);
        const UChar *resStr = ures_getStringByKeyWithFallback(resource, fgDecimalFormat, &len, &status);
        if ( status == U_MISSING_RESOURCE_ERROR && uprv_strcmp(fgLatn,ns->getName())) {
            status = U_ZERO_ERROR;
            resource = ures_getByKeyWithFallback(top, fgNumberElements, resource, &status);
            resource = ures_getByKeyWithFallback(resource, fgLatn, resource, &status);
            resource = ures_getByKeyWithFallback(resource, fgPatterns, resource, &status);
            resStr = ures_getStringByKeyWithFallback(resource, fgDecimalFormat, &len, &status);
        }
        str.setTo(TRUE, resStr, len);
        pattern = &str;
        ures_close(resource);
        ures_close(top);
    }

    fImpl = new DecimalFormatImpl(this, *pattern, adoptedSymbols.getAlias(), parseErr, status);
    if (fImpl) {
        adoptedSymbols.orphan();
    } else if (U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        return;
    }

    if (U_FAILURE(status))
    {
        return;
    }

    const UnicodeString* patternUsed;
    UnicodeString currencyPluralPatternForOther;
    // apply pattern
    if (fStyle == UNUM_CURRENCY_PLURAL) {
        fCurrencyPluralInfo = new CurrencyPluralInfo(fImpl->fSymbols->getLocale(), status);
        if (U_FAILURE(status)) {
            return;
        }

        // the pattern used in format is not fixed until formatting,
        // in which, the number is known and
        // will be used to pick the right pattern based on plural count.
        // Here, set the pattern as the pattern of plural count == "other".
        // For most locale, the patterns are probably the same for all
        // plural count. If not, the right pattern need to be re-applied
        // during format.
        fCurrencyPluralInfo->getCurrencyPluralPattern(UNICODE_STRING("other", 5), currencyPluralPatternForOther);
        // TODO(refactor): Revisit, we are setting the pattern twice.
        fImpl->applyPatternFavorCurrencyPrecision(
                currencyPluralPatternForOther, status);
        patternUsed = &currencyPluralPatternForOther;

    } else {
        patternUsed = pattern;
    }

    if (patternUsed->indexOf(kCurrencySign) != -1) {
        // initialize for currency, not only for plural format,
        // but also for mix parsing
        handleCurrencySignInPattern(status);
    }
}

void
DecimalFormat::handleCurrencySignInPattern(UErrorCode& status) {
    // initialize for currency, not only for plural format,
    // but also for mix parsing
    if (U_FAILURE(status)) {
        return;
    }
    if (fCurrencyPluralInfo == NULL) {
       fCurrencyPluralInfo = new CurrencyPluralInfo(fImpl->fSymbols->getLocale(), status);
       if (U_FAILURE(status)) {
           return;
       }
    }
    // need it for mix parsing
    if (fAffixPatternsForCurrency == NULL) {
        setupCurrencyAffixPatterns(status);
    }
}

static void
applyPatternWithNoSideEffects(
        const UnicodeString& pattern,
        UParseError& parseError,
        UnicodeString &negPrefix,
        UnicodeString &negSuffix,
        UnicodeString &posPrefix,
        UnicodeString &posSuffix,
        UErrorCode& status) {
        if (U_FAILURE(status))
    {
        return;
    }
    DecimalFormatPatternParser patternParser;
    DecimalFormatPattern out;
    patternParser.applyPatternWithoutExpandAffix(
        pattern,
        out,
        parseError,
        status);
    if (U_FAILURE(status)) {
      return;
    }
    negPrefix = out.fNegPrefixPattern;
    negSuffix = out.fNegSuffixPattern;
    posPrefix = out.fPosPrefixPattern;
    posSuffix = out.fPosSuffixPattern;
}

void
DecimalFormat::setupCurrencyAffixPatterns(UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    UParseError parseErr;
    fAffixPatternsForCurrency = initHashForAffixPattern(status);
    if (U_FAILURE(status)) {
        return;
    }

    NumberingSystem *ns = NumberingSystem::createInstance(fImpl->fSymbols->getLocale(),status);
    if (U_FAILURE(status)) {
        return;
    }

    // Save the default currency patterns of this locale.
    // Here, chose onlyApplyPatternWithoutExpandAffix without
    // expanding the affix patterns into affixes.
    UnicodeString currencyPattern;
    UErrorCode error = U_ZERO_ERROR;

    UResourceBundle *resource = ures_open(NULL, fImpl->fSymbols->getLocale().getName(), &error);
    UResourceBundle *numElements = ures_getByKeyWithFallback(resource, fgNumberElements, NULL, &error);
    resource = ures_getByKeyWithFallback(numElements, ns->getName(), resource, &error);
    resource = ures_getByKeyWithFallback(resource, fgPatterns, resource, &error);
    int32_t patLen = 0;
    const UChar *patResStr = ures_getStringByKeyWithFallback(resource, fgCurrencyFormat,  &patLen, &error);
    if ( error == U_MISSING_RESOURCE_ERROR && uprv_strcmp(ns->getName(),fgLatn)) {
        error = U_ZERO_ERROR;
        resource = ures_getByKeyWithFallback(numElements, fgLatn, resource, &error);
        resource = ures_getByKeyWithFallback(resource, fgPatterns, resource, &error);
        patResStr = ures_getStringByKeyWithFallback(resource, fgCurrencyFormat,  &patLen, &error);
    }
    ures_close(numElements);
    ures_close(resource);
    delete ns;

    if (U_SUCCESS(error)) {
        UnicodeString negPrefix;
        UnicodeString negSuffix;
        UnicodeString posPrefix;
        UnicodeString posSuffix;
        applyPatternWithNoSideEffects(UnicodeString(patResStr, patLen),
                                       parseErr,
                negPrefix, negSuffix, posPrefix, posSuffix,  status);
        AffixPatternsForCurrency* affixPtn = new AffixPatternsForCurrency(
                                                    negPrefix,
                                                    negSuffix,
                                                    posPrefix,
                                                    posSuffix,
                                                    UCURR_SYMBOL_NAME);
        fAffixPatternsForCurrency->put(UNICODE_STRING("default", 7), affixPtn, status);
    }

    // save the unique currency plural patterns of this locale.
    Hashtable* pluralPtn = fCurrencyPluralInfo->fPluralCountToCurrencyUnitPattern;
    const UHashElement* element = NULL;
    int32_t pos = UHASH_FIRST;
    Hashtable pluralPatternSet;
    while ((element = pluralPtn->nextElement(pos)) != NULL) {
        const UHashTok valueTok = element->value;
        const UnicodeString* value = (UnicodeString*)valueTok.pointer;
        const UHashTok keyTok = element->key;
        const UnicodeString* key = (UnicodeString*)keyTok.pointer;
        if (pluralPatternSet.geti(*value) != 1) {
            UnicodeString negPrefix;
            UnicodeString negSuffix;
            UnicodeString posPrefix;
            UnicodeString posSuffix;
            pluralPatternSet.puti(*value, 1, status);
            applyPatternWithNoSideEffects(
                    *value, parseErr,
                    negPrefix, negSuffix, posPrefix, posSuffix, status);
            AffixPatternsForCurrency* affixPtn = new AffixPatternsForCurrency(
                                                    negPrefix,
                                                    negSuffix,
                                                    posPrefix,
                                                    posSuffix,
                                                    UCURR_LONG_NAME);
            fAffixPatternsForCurrency->put(*key, affixPtn, status);
        }
    }
}


//------------------------------------------------------------------------------

DecimalFormat::~DecimalFormat()
{
    deleteHashForAffixPattern();
    delete fCurrencyPluralInfo;
    delete fImpl;
}

//------------------------------------------------------------------------------
// copy constructor

DecimalFormat::DecimalFormat(const DecimalFormat &source) :
    NumberFormat(source) {
    init();
    *this = source;
}

//------------------------------------------------------------------------------
// assignment operator

template <class T>
static void _clone_ptr(T** pdest, const T* source) {
    delete *pdest;
    if (source == NULL) {
        *pdest = NULL;
    } else {
        *pdest = static_cast<T*>(source->clone());
    }
}

DecimalFormat&
DecimalFormat::operator=(const DecimalFormat& rhs)
{
    if(this != &rhs) {
        UErrorCode status = U_ZERO_ERROR;
        NumberFormat::operator=(rhs);
        if (fImpl == NULL) {
            fImpl = new DecimalFormatImpl(this, *rhs.fImpl, status);
        } else {
            fImpl->assign(*rhs.fImpl, status);
        }
        fStaticSets     = DecimalFormatStaticSets::getStaticSets(status);
        fStyle = rhs.fStyle;
        _clone_ptr(&fCurrencyPluralInfo, rhs.fCurrencyPluralInfo);
        deleteHashForAffixPattern();
        if (rhs.fAffixPatternsForCurrency) {
            UErrorCode status = U_ZERO_ERROR;
            fAffixPatternsForCurrency = initHashForAffixPattern(status);
            copyHashForAffixPattern(rhs.fAffixPatternsForCurrency,
                                    fAffixPatternsForCurrency, status);
        }
    }

    return *this;
}

//------------------------------------------------------------------------------

UBool
DecimalFormat::operator==(const Format& that) const
{
    if (this == &that)
        return TRUE;

    // NumberFormat::operator== guarantees this cast is safe
    const DecimalFormat* other = (DecimalFormat*)&that;

    return (
        NumberFormat::operator==(that) &&
        fBoolFlags.getAll() == other->fBoolFlags.getAll() &&
        *fImpl == *other->fImpl);

}

//------------------------------------------------------------------------------

Format*
DecimalFormat::clone() const
{
    return new DecimalFormat(*this);
}


FixedDecimal
DecimalFormat::getFixedDecimal(double number, UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    if (U_FAILURE(status)) {
        return FixedDecimal();
    }
    return FixedDecimal(digits.getMantissa());
}

VisibleDigitsWithExponent &
DecimalFormat::initVisibleDigitsWithExponent(
        double number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    return fImpl->initVisibleDigitsWithExponent(number, digits, status);
}

FixedDecimal
DecimalFormat::getFixedDecimal(const Formattable &number, UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    if (U_FAILURE(status)) {
        return FixedDecimal();
    }
    return FixedDecimal(digits.getMantissa());
}

VisibleDigitsWithExponent &
DecimalFormat::initVisibleDigitsWithExponent(
        const Formattable &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return digits;
    }
    if (!number.isNumeric()) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return digits;
    }

    DigitList *dl = number.getDigitList();
    if (dl != NULL) {
        DigitList dlCopy(*dl);
        return fImpl->initVisibleDigitsWithExponent(
                dlCopy, digits, status);
    }

    Formattable::Type type = number.getType();
    if (type == Formattable::kDouble || type == Formattable::kLong) {
        return fImpl->initVisibleDigitsWithExponent(
                number.getDouble(status), digits, status);
    }
    return fImpl->initVisibleDigitsWithExponent(
            number.getInt64(), digits, status);
}


// Create a fixed decimal from a DigitList.
//    The digit list may be modified.
//    Internal function only.
FixedDecimal
DecimalFormat::getFixedDecimal(DigitList &number, UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    if (U_FAILURE(status)) {
        return FixedDecimal();
    }
    return FixedDecimal(digits.getMantissa());
}

VisibleDigitsWithExponent &
DecimalFormat::initVisibleDigitsWithExponent(
        DigitList &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    return fImpl->initVisibleDigitsWithExponent(
            number, digits, status);
}


//------------------------------------------------------------------------------

UnicodeString&
DecimalFormat::format(int32_t number,
                      UnicodeString& appendTo,
                      FieldPosition& fieldPosition) const
{
    UErrorCode status = U_ZERO_ERROR;
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(int32_t number,
                      UnicodeString& appendTo,
                      FieldPosition& fieldPosition,
                      UErrorCode& status) const
{
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(int32_t number,
                      UnicodeString& appendTo,
                      FieldPositionIterator* posIter,
                      UErrorCode& status) const
{
    return fImpl->format(number, appendTo, posIter, status);
}


//------------------------------------------------------------------------------

UnicodeString&
DecimalFormat::format(int64_t number,
                      UnicodeString& appendTo,
                      FieldPosition& fieldPosition) const
{
    UErrorCode status = U_ZERO_ERROR; /* ignored */
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(int64_t number,
                      UnicodeString& appendTo,
                      FieldPosition& fieldPosition,
                      UErrorCode& status) const
{
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(int64_t number,
                      UnicodeString& appendTo,
                      FieldPositionIterator* posIter,
                      UErrorCode& status) const
{
    return fImpl->format(number, appendTo, posIter, status);
}

//------------------------------------------------------------------------------

UnicodeString&
DecimalFormat::format(  double number,
                        UnicodeString& appendTo,
                        FieldPosition& fieldPosition) const
{
    UErrorCode status = U_ZERO_ERROR; /* ignored */
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(  double number,
                        UnicodeString& appendTo,
                        FieldPosition& fieldPosition,
                        UErrorCode& status) const
{
    return fImpl->format(number, appendTo, fieldPosition, status);
}

UnicodeString&
DecimalFormat::format(  double number,
                        UnicodeString& appendTo,
                        FieldPositionIterator* posIter,
                        UErrorCode& status) const
{
    return fImpl->format(number, appendTo, posIter, status);
}

//------------------------------------------------------------------------------


UnicodeString&
DecimalFormat::format(StringPiece number,
                      UnicodeString &toAppendTo,
                      FieldPositionIterator *posIter,
                      UErrorCode &status) const
{
  return fImpl->format(number, toAppendTo, posIter, status);
}


UnicodeString&
DecimalFormat::format(const DigitList &number,
                      UnicodeString &appendTo,
                      FieldPositionIterator *posIter,
                      UErrorCode &status) const {
    return fImpl->format(number, appendTo, posIter, status);
}


UnicodeString&
DecimalFormat::format(const DigitList &number,
                     UnicodeString& appendTo,
                     FieldPosition& pos,
                     UErrorCode &status) const {
    return fImpl->format(number, appendTo, pos, status);
}

UnicodeString&
DecimalFormat::format(const VisibleDigitsWithExponent &number,
                      UnicodeString &appendTo,
                      FieldPositionIterator *posIter,
                      UErrorCode &status) const {
    return fImpl->format(number, appendTo, posIter, status);
}


UnicodeString&
DecimalFormat::format(const VisibleDigitsWithExponent &number,
                     UnicodeString& appendTo,
                     FieldPosition& pos,
                     UErrorCode &status) const {
    return fImpl->format(number, appendTo, pos, status);
}

DigitList&
DecimalFormat::_round(const DigitList& number, DigitList& adjustedNum, UBool& isNegative, UErrorCode& status) const {
    adjustedNum = number;
    fImpl->round(adjustedNum, status);
    isNegative = !adjustedNum.isPositive();
    return adjustedNum;
}

void
DecimalFormat::parse(const UnicodeString& text,
                     Formattable& result,
                     ParsePosition& parsePosition) const {
    parse(text, result, parsePosition, NULL);
}

CurrencyAmount* DecimalFormat::parseCurrency(const UnicodeString& text,
                                             ParsePosition& pos) const {
    Formattable parseResult;
    int32_t start = pos.getIndex();
    UChar curbuf[4] = {};
    parse(text, parseResult, pos, curbuf);
    if (pos.getIndex() != start) {
        UErrorCode ec = U_ZERO_ERROR;
        LocalPointer<CurrencyAmount> currAmt(new CurrencyAmount(parseResult, curbuf, ec), ec);
        if (U_FAILURE(ec)) {
            pos.setIndex(start); // indicate failure
        } else {
            return currAmt.orphan();
        }
    }
    return NULL;
}

/**
 * Parses the given text as a number, optionally providing a currency amount.
 * @param text the string to parse
 * @param result output parameter for the numeric result.
 * @param parsePosition input-output position; on input, the
 * position within text to match; must have 0 <= pos.getIndex() <
 * text.length(); on output, the position after the last matched
 * character. If the parse fails, the position in unchanged upon
 * output.
 * @param currency if non-NULL, it should point to a 4-UChar buffer.
 * In this case the text is parsed as a currency format, and the
 * ISO 4217 code for the parsed currency is put into the buffer.
 * Otherwise the text is parsed as a non-currency format.
 */
void DecimalFormat::parse(const UnicodeString& text,
                          Formattable& result,
                          ParsePosition& parsePosition,
                          UChar* currency) const {
    int32_t startIdx, backup;
    int32_t i = startIdx = backup = parsePosition.getIndex();

    // clear any old contents in the result.  In particular, clears any DigitList
    //   that it may be holding.
    result.setLong(0);
    if (currency != NULL) {
        for (int32_t ci=0; ci<4; ci++) {
            currency[ci] = 0;
        }
    }

    // Handle NaN as a special case:
    int32_t formatWidth = fImpl->getOldFormatWidth();

    // Skip padding characters, if around prefix
    if (formatWidth > 0 && (
            fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforePrefix ||
            fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterPrefix)) {
        i = skipPadding(text, i);
    }

    if (isLenient()) {
        // skip any leading whitespace
        i = backup = skipUWhiteSpace(text, i);
    }

    // If the text is composed of the representation of NaN, returns NaN.length
    const UnicodeString *nan = &fImpl->getConstSymbol(DecimalFormatSymbols::kNaNSymbol);
    int32_t nanLen = (text.compare(i, nan->length(), *nan)
                      ? 0 : nan->length());
    if (nanLen) {
        i += nanLen;
        if (formatWidth > 0 && (fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforeSuffix || fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterSuffix)) {
            i = skipPadding(text, i);
        }
        parsePosition.setIndex(i);
        result.setDouble(uprv_getNaN());
        return;
    }

    // NaN parse failed; start over
    i = backup;
    parsePosition.setIndex(i);

    // status is used to record whether a number is infinite.
    UBool status[fgStatusLength];

    DigitList *digits = result.getInternalDigitList(); // get one from the stack buffer
    if (digits == NULL) {
        return;    // no way to report error from here.
    }

    if (fImpl->fMonetary) {
        if (!parseForCurrency(text, parsePosition, *digits,
                              status, currency)) {
          return;
        }
    } else {
        if (!subparse(text,
                      &fImpl->fAffixes.fNegativePrefix.getOtherVariant().toString(),
                      &fImpl->fAffixes.fNegativeSuffix.getOtherVariant().toString(),
                      &fImpl->fAffixes.fPositivePrefix.getOtherVariant().toString(),
                      &fImpl->fAffixes.fPositiveSuffix.getOtherVariant().toString(),
                      FALSE, UCURR_SYMBOL_NAME,
                      parsePosition, *digits, status, currency)) {
            debug("!subparse(...) - rewind");
            parsePosition.setIndex(startIdx);
            return;
        }
    }

    // Handle infinity
    if (status[fgStatusInfinite]) {
        double inf = uprv_getInfinity();
        result.setDouble(digits->isPositive() ? inf : -inf);
        // TODO:  set the dl to infinity, and let it fall into the code below.
    }

    else {

        if (!fImpl->fMultiplier.isZero()) {
            UErrorCode ec = U_ZERO_ERROR;
            digits->div(fImpl->fMultiplier, ec);
        }

        if (fImpl->fScale != 0) {
            DigitList ten;
            ten.set((int32_t)10);
            if (fImpl->fScale > 0) {
                for (int32_t i = fImpl->fScale; i > 0; i--) {
                    UErrorCode ec = U_ZERO_ERROR;
                    digits->div(ten,ec);
                }
            } else {
                for (int32_t i = fImpl->fScale; i < 0; i++) {
                    UErrorCode ec = U_ZERO_ERROR;
                    digits->mult(ten,ec);
                }
            }
        }

        // Negative zero special case:
        //    if parsing integerOnly, change to +0, which goes into an int32 in a Formattable.
        //    if not parsing integerOnly, leave as -0, which a double can represent.
        if (digits->isZero() && !digits->isPositive() && isParseIntegerOnly()) {
            digits->setPositive(TRUE);
        }
        result.adoptDigitList(digits);
    }
}



UBool
DecimalFormat::parseForCurrency(const UnicodeString& text,
                                ParsePosition& parsePosition,
                                DigitList& digits,
                                UBool* status,
                                UChar* currency) const {
    UnicodeString positivePrefix;
    UnicodeString positiveSuffix;
    UnicodeString negativePrefix;
    UnicodeString negativeSuffix;
    fImpl->fPositivePrefixPattern.toString(positivePrefix);
    fImpl->fPositiveSuffixPattern.toString(positiveSuffix);
    fImpl->fNegativePrefixPattern.toString(negativePrefix);
    fImpl->fNegativeSuffixPattern.toString(negativeSuffix);

    int origPos = parsePosition.getIndex();
    int maxPosIndex = origPos;
    int maxErrorPos = -1;
    // First, parse against current pattern.
    // Since current pattern could be set by applyPattern(),
    // it could be an arbitrary pattern, and it may not be the one
    // defined in current locale.
    UBool tmpStatus[fgStatusLength];
    ParsePosition tmpPos(origPos);
    DigitList tmpDigitList;
    UBool found;
    if (fStyle == UNUM_CURRENCY_PLURAL) {
        found = subparse(text,
                         &negativePrefix, &negativeSuffix,
                         &positivePrefix, &positiveSuffix,
                         TRUE, UCURR_LONG_NAME,
                         tmpPos, tmpDigitList, tmpStatus, currency);
    } else {
        found = subparse(text,
                         &negativePrefix, &negativeSuffix,
                         &positivePrefix, &positiveSuffix,
                         TRUE, UCURR_SYMBOL_NAME,
                         tmpPos, tmpDigitList, tmpStatus, currency);
    }
    if (found) {
        if (tmpPos.getIndex() > maxPosIndex) {
            maxPosIndex = tmpPos.getIndex();
            for (int32_t i = 0; i < fgStatusLength; ++i) {
                status[i] = tmpStatus[i];
            }
            digits = tmpDigitList;
        }
    } else {
        maxErrorPos = tmpPos.getErrorIndex();
    }
    // Then, parse against affix patterns.
    // Those are currency patterns and currency plural patterns.
    int32_t pos = UHASH_FIRST;
    const UHashElement* element = NULL;
    while ( (element = fAffixPatternsForCurrency->nextElement(pos)) != NULL ) {
        const UHashTok valueTok = element->value;
        const AffixPatternsForCurrency* affixPtn = (AffixPatternsForCurrency*)valueTok.pointer;
        UBool tmpStatus[fgStatusLength];
        ParsePosition tmpPos(origPos);
        DigitList tmpDigitList;

#ifdef FMT_DEBUG
        debug("trying affix for currency..");
        affixPtn->dump();
#endif

        UBool result = subparse(text,
                                &affixPtn->negPrefixPatternForCurrency,
                                &affixPtn->negSuffixPatternForCurrency,
                                &affixPtn->posPrefixPatternForCurrency,
                                &affixPtn->posSuffixPatternForCurrency,
                                TRUE, affixPtn->patternType,
                                tmpPos, tmpDigitList, tmpStatus, currency);
        if (result) {
            found = true;
            if (tmpPos.getIndex() > maxPosIndex) {
                maxPosIndex = tmpPos.getIndex();
                for (int32_t i = 0; i < fgStatusLength; ++i) {
                    status[i] = tmpStatus[i];
                }
                digits = tmpDigitList;
            }
        } else {
            maxErrorPos = (tmpPos.getErrorIndex() > maxErrorPos) ?
                          tmpPos.getErrorIndex() : maxErrorPos;
        }
    }
    // Finally, parse against simple affix to find the match.
    // For example, in TestMonster suite,
    // if the to-be-parsed text is "-\u00A40,00".
    // complexAffixCompare will not find match,
    // since there is no ISO code matches "\u00A4",
    // and the parse stops at "\u00A4".
    // We will just use simple affix comparison (look for exact match)
    // to pass it.
    //
    // TODO: We should parse against simple affix first when
    // output currency is not requested. After the complex currency
    // parsing implementation was introduced, the default currency
    // instance parsing slowed down because of the new code flow.
    // I filed #10312 - Yoshito
    UBool tmpStatus_2[fgStatusLength];
    ParsePosition tmpPos_2(origPos);
    DigitList tmpDigitList_2;

    // Disable complex currency parsing and try it again.
    UBool result = subparse(text,
                            &fImpl->fAffixes.fNegativePrefix.getOtherVariant().toString(),
                            &fImpl->fAffixes.fNegativeSuffix.getOtherVariant().toString(),
                            &fImpl->fAffixes.fPositivePrefix.getOtherVariant().toString(),
                            &fImpl->fAffixes.fPositiveSuffix.getOtherVariant().toString(),
                            FALSE /* disable complex currency parsing */, UCURR_SYMBOL_NAME,
                            tmpPos_2, tmpDigitList_2, tmpStatus_2,
                            currency);
    if (result) {
        if (tmpPos_2.getIndex() > maxPosIndex) {
            maxPosIndex = tmpPos_2.getIndex();
            for (int32_t i = 0; i < fgStatusLength; ++i) {
                status[i] = tmpStatus_2[i];
            }
            digits = tmpDigitList_2;
        }
        found = true;
    } else {
            maxErrorPos = (tmpPos_2.getErrorIndex() > maxErrorPos) ?
                          tmpPos_2.getErrorIndex() : maxErrorPos;
    }

    if (!found) {
        //parsePosition.setIndex(origPos);
        parsePosition.setErrorIndex(maxErrorPos);
    } else {
        parsePosition.setIndex(maxPosIndex);
        parsePosition.setErrorIndex(-1);
    }
    return found;
}


/**
 * Parse the given text into a number.  The text is parsed beginning at
 * parsePosition, until an unparseable character is seen.
 * @param text the string to parse.
 * @param negPrefix negative prefix.
 * @param negSuffix negative suffix.
 * @param posPrefix positive prefix.
 * @param posSuffix positive suffix.
 * @param complexCurrencyParsing whether it is complex currency parsing or not.
 * @param type the currency type to parse against, LONG_NAME only or not.
 * @param parsePosition The position at which to being parsing.  Upon
 * return, the first unparsed character.
 * @param digits the DigitList to set to the parsed value.
 * @param status output param containing boolean status flags indicating
 * whether the value was infinite and whether it was positive.
 * @param currency return value for parsed currency, for generic
 * currency parsing mode, or NULL for normal parsing. In generic
 * currency parsing mode, any currency is parsed, not just the
 * currency that this formatter is set to.
 */
UBool DecimalFormat::subparse(const UnicodeString& text,
                              const UnicodeString* negPrefix,
                              const UnicodeString* negSuffix,
                              const UnicodeString* posPrefix,
                              const UnicodeString* posSuffix,
                              UBool complexCurrencyParsing,
                              int8_t type,
                              ParsePosition& parsePosition,
                              DigitList& digits, UBool* status,
                              UChar* currency) const
{
    //  The parsing process builds up the number as char string, in the neutral format that
    //  will be acceptable to the decNumber library, then at the end passes that string
    //  off for conversion to a decNumber.
    UErrorCode err = U_ZERO_ERROR;
    CharString parsedNum;
    digits.setToZero();

    int32_t position = parsePosition.getIndex();
    int32_t oldStart = position;
    int32_t textLength = text.length(); // One less pointer to follow
    UBool strictParse = !isLenient();
    UChar32 zero = fImpl->getConstSymbol(DecimalFormatSymbols::kZeroDigitSymbol).char32At(0);
    const UnicodeString *groupingString = &fImpl->getConstSymbol(
            !fImpl->fMonetary ?
            DecimalFormatSymbols::kGroupingSeparatorSymbol : DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol);
    UChar32 groupingChar = groupingString->char32At(0);
    int32_t groupingStringLength = groupingString->length();
    int32_t groupingCharLength   = U16_LENGTH(groupingChar);
    UBool   groupingUsed = isGroupingUsed();
#ifdef FMT_DEBUG
    UChar dbgbuf[300];
    UnicodeString s(dbgbuf,0,300);;
    s.append((UnicodeString)"PARSE \"").append(text.tempSubString(position)).append((UnicodeString)"\" " );
#define DBGAPPD(x) if(x) { s.append(UnicodeString(#x "="));  if(x->isEmpty()) { s.append(UnicodeString("<empty>")); } else { s.append(*x); } s.append(UnicodeString(" ")); } else { s.append(UnicodeString(#x "=NULL ")); }
    DBGAPPD(negPrefix);
    DBGAPPD(negSuffix);
    DBGAPPD(posPrefix);
    DBGAPPD(posSuffix);
    debugout(s);
#endif

    UBool fastParseOk = false; /* TRUE iff fast parse is OK */
    // UBool fastParseHadDecimal = FALSE; /* true if fast parse saw a decimal point. */
    if((fImpl->isParseFastpath()) && !fImpl->fMonetary &&
       text.length()>0 &&
       text.length()<32 &&
       (posPrefix==NULL||posPrefix->isEmpty()) &&
       (posSuffix==NULL||posSuffix->isEmpty()) &&
       //            (negPrefix==NULL||negPrefix->isEmpty()) &&
       //            (negSuffix==NULL||(negSuffix->isEmpty()) ) &&
       TRUE) {  // optimized path
      int j=position;
      int l=text.length();
      int digitCount=0;
      UChar32 ch = text.char32At(j);
      const UnicodeString *decimalString = &fImpl->getConstSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
      UChar32 decimalChar = 0;
      UBool intOnly = FALSE;
      UChar32 lookForGroup = (groupingUsed&&intOnly&&strictParse)?groupingChar:0;

      int32_t decimalCount = decimalString->countChar32(0,3);
      if(isParseIntegerOnly()) {
        decimalChar = 0; // not allowed
        intOnly = TRUE; // Don't look for decimals.
      } else if(decimalCount==1) {
        decimalChar = decimalString->char32At(0); // Look for this decimal
      } else if(decimalCount==0) {
        decimalChar=0; // NO decimal set
      } else {
        j=l+1;//Set counter to end of line, so that we break. Unknown decimal situation.
      }

#ifdef FMT_DEBUG
      printf("Preparing to do fastpath parse: decimalChar=U+%04X, groupingChar=U+%04X, first ch=U+%04X intOnly=%c strictParse=%c\n",
        decimalChar, groupingChar, ch,
        (intOnly)?'y':'n',
        (strictParse)?'y':'n');
#endif
      if(ch==0x002D) { // '-'
        j=l+1;//=break - negative number.

        /*
          parsedNum.append('-',err);
          j+=U16_LENGTH(ch);
          if(j<l) ch = text.char32At(j);
        */
      } else {
        parsedNum.append('+',err);
      }
      while(j<l) {
        int32_t digit = ch - zero;
        if(digit >=0 && digit <= 9) {
          parsedNum.append((char)(digit + '0'), err);
          if((digitCount>0) || digit!=0 || j==(l-1)) {
            digitCount++;
          }
        } else if(ch == 0) { // break out
          digitCount=-1;
          break;
        } else if(ch == decimalChar) {
          parsedNum.append((char)('.'), err);
          decimalChar=0; // no more decimals.
          // fastParseHadDecimal=TRUE;
        } else if(ch == lookForGroup) {
          // ignore grouping char. No decimals, so it has to be an ignorable grouping sep
        } else if(intOnly && (lookForGroup!=0) && !u_isdigit(ch)) {
          // parsing integer only and can fall through
        } else {
          digitCount=-1; // fail - fall through to slow parse
          break;
        }
        j+=U16_LENGTH(ch);
        ch = text.char32At(j); // for next
      }
      if(
         ((j==l)||intOnly) // end OR only parsing integer
         && (digitCount>0)) { // and have at least one digit
        fastParseOk=true; // Fast parse OK!

#ifdef SKIP_OPT
        debug("SKIP_OPT");
        /* for testing, try it the slow way. also */
        fastParseOk=false;
        parsedNum.clear();
#else
        parsePosition.setIndex(position=j);
        status[fgStatusInfinite]=false;
#endif
      } else {
        // was not OK. reset, retry
#ifdef FMT_DEBUG
        printf("Fall through: j=%d, l=%d, digitCount=%d\n", j, l, digitCount);
#endif
        parsedNum.clear();
      }
    } else {
#ifdef FMT_DEBUG
      printf("Could not fastpath parse. ");
      printf("text.length()=%d ", text.length());
      printf("posPrefix=%p posSuffix=%p ", posPrefix, posSuffix);

      printf("\n");
#endif
    }

  UnicodeString formatPattern;
  toPattern(formatPattern);

  if(!fastParseOk
#if UCONFIG_HAVE_PARSEALLINPUT
     && fParseAllInput!=UNUM_YES
#endif
     )
  {
    int32_t formatWidth = fImpl->getOldFormatWidth();
    // Match padding before prefix
    if (formatWidth > 0 && fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforePrefix) {
        position = skipPadding(text, position);
    }

    // Match positive and negative prefixes; prefer longest match.
    int32_t posMatch = compareAffix(text, position, FALSE, TRUE, posPrefix, complexCurrencyParsing, type, currency);
    int32_t negMatch = compareAffix(text, position, TRUE,  TRUE, negPrefix, complexCurrencyParsing, type, currency);
    if (posMatch >= 0 && negMatch >= 0) {
        if (posMatch > negMatch) {
            negMatch = -1;
        } else if (negMatch > posMatch) {
            posMatch = -1;
        }
    }
    if (posMatch >= 0) {
        position += posMatch;
        parsedNum.append('+', err);
    } else if (negMatch >= 0) {
        position += negMatch;
        parsedNum.append('-', err);
    } else if (strictParse){
        parsePosition.setErrorIndex(position);
        return FALSE;
    } else {
        // Temporary set positive. This might be changed after checking suffix
        parsedNum.append('+', err);
    }

    // Match padding before prefix
    if (formatWidth > 0 && fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterPrefix) {
        position = skipPadding(text, position);
    }

    if (! strictParse) {
        position = skipUWhiteSpace(text, position);
    }

    // process digits or Inf, find decimal position
    const UnicodeString *inf = &fImpl->getConstSymbol(DecimalFormatSymbols::kInfinitySymbol);
    int32_t infLen = (text.compare(position, inf->length(), *inf)
        ? 0 : inf->length());
    position += infLen; // infLen is non-zero when it does equal to infinity
    status[fgStatusInfinite] = infLen != 0;

    if (infLen != 0) {
        parsedNum.append("Infinity", err);
    } else {
        // We now have a string of digits, possibly with grouping symbols,
        // and decimal points.  We want to process these into a DigitList.
        // We don't want to put a bunch of leading zeros into the DigitList
        // though, so we keep track of the location of the decimal point,
        // put only significant digits into the DigitList, and adjust the
        // exponent as needed.


        UBool strictFail = FALSE; // did we exit with a strict parse failure?
        int32_t lastGroup = -1; // where did we last see a grouping separator?
        int32_t digitStart = position;
        int32_t gs2 = fImpl->fEffGrouping.fGrouping2 == 0 ? fImpl->fEffGrouping.fGrouping : fImpl->fEffGrouping.fGrouping2;

        const UnicodeString *decimalString;
        if (fImpl->fMonetary) {
            decimalString = &fImpl->getConstSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol);
        } else {
            decimalString = &fImpl->getConstSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
        }
        UChar32 decimalChar = decimalString->char32At(0);
        int32_t decimalStringLength = decimalString->length();
        int32_t decimalCharLength   = U16_LENGTH(decimalChar);

        UBool sawDecimal = FALSE;
        UChar32 sawDecimalChar = 0xFFFF;
        UBool sawGrouping = FALSE;
        UChar32 sawGroupingChar = 0xFFFF;
        UBool sawDigit = FALSE;
        int32_t backup = -1;
        int32_t digit;

        // equivalent grouping and decimal support
        const UnicodeSet *decimalSet = NULL;
        const UnicodeSet *groupingSet = NULL;

        if (decimalCharLength == decimalStringLength) {
            decimalSet = DecimalFormatStaticSets::getSimilarDecimals(decimalChar, strictParse);
        }

        if (groupingCharLength == groupingStringLength) {
            if (strictParse) {
                groupingSet = fStaticSets->fStrictDefaultGroupingSeparators;
            } else {
                groupingSet = fStaticSets->fDefaultGroupingSeparators;
            }
        }

        // We need to test groupingChar and decimalChar separately from groupingSet and decimalSet, if the sets are even initialized.
        // If sawDecimal is TRUE, only consider sawDecimalChar and NOT decimalSet
        // If a character matches decimalSet, don't consider it to be a member of the groupingSet.

        // We have to track digitCount ourselves, because digits.fCount will
        // pin when the maximum allowable digits is reached.
        int32_t digitCount = 0;
        int32_t integerDigitCount = 0;

        for (; position < textLength; )
        {
            UChar32 ch = text.char32At(position);

            /* We recognize all digit ranges, not only the Latin digit range
             * '0'..'9'.  We do so by using the Character.digit() method,
             * which converts a valid Unicode digit to the range 0..9.
             *
             * The character 'ch' may be a digit.  If so, place its value
             * from 0 to 9 in 'digit'.  First try using the locale digit,
             * which may or MAY NOT be a standard Unicode digit range.  If
             * this fails, try using the standard Unicode digit ranges by
             * calling Character.digit().  If this also fails, digit will
             * have a value outside the range 0..9.
             */
            digit = ch - zero;
            if (digit < 0 || digit > 9)
            {
                digit = u_charDigitValue(ch);
            }

            // As a last resort, look through the localized digits if the zero digit
            // is not a "standard" Unicode digit.
            if ( (digit < 0 || digit > 9) && u_charDigitValue(zero) != 0) {
                digit = 0;
                if ( fImpl->getConstSymbol((DecimalFormatSymbols::ENumberFormatSymbol)(DecimalFormatSymbols::kZeroDigitSymbol)).char32At(0) == ch ) {
                    break;
                }
                for (digit = 1 ; digit < 10 ; digit++ ) {
                    if ( fImpl->getConstSymbol((DecimalFormatSymbols::ENumberFormatSymbol)(DecimalFormatSymbols::kOneDigitSymbol+digit-1)).char32At(0) == ch ) {
                        break;
                    }
                }
            }

            if (digit >= 0 && digit <= 9)
            {
                if (strictParse && backup != -1) {
                    // comma followed by digit, so group before comma is a
                    // secondary group.  If there was a group separator
                    // before that, the group must == the secondary group
                    // length, else it can be <= the the secondary group
                    // length.
                    if ((lastGroup != -1 && backup - lastGroup - 1 != gs2) ||
                        (lastGroup == -1 && position - digitStart - 1 > gs2)) {
                        strictFail = TRUE;
                        break;
                    }

                    lastGroup = backup;
                }

                // Cancel out backup setting (see grouping handler below)
                backup = -1;
                sawDigit = TRUE;

                // Note: this will append leading zeros
                parsedNum.append((char)(digit + '0'), err);

                // count any digit that's not a leading zero
                if (digit > 0 || digitCount > 0 || sawDecimal) {
                    digitCount += 1;

                    // count any integer digit that's not a leading zero
                    if (! sawDecimal) {
                        integerDigitCount += 1;
                    }
                }

                position += U16_LENGTH(ch);
            }
            else if (groupingStringLength > 0 &&
                matchGrouping(groupingChar, sawGrouping, sawGroupingChar, groupingSet,
                            decimalChar, decimalSet,
                            ch) && groupingUsed)
            {
                if (sawDecimal) {
                    break;
                }

                if (strictParse) {
                    if ((!sawDigit || backup != -1)) {
                        // leading group, or two group separators in a row
                        strictFail = TRUE;
                        break;
                    }
                }

                // Ignore grouping characters, if we are using them, but require
                // that they be followed by a digit.  Otherwise we backup and
                // reprocess them.
                backup = position;
                position += groupingStringLength;
                sawGrouping=TRUE;
                // Once we see a grouping character, we only accept that grouping character from then on.
                sawGroupingChar=ch;
            }
            else if (matchDecimal(decimalChar,sawDecimal,sawDecimalChar, decimalSet, ch))
            {
                if (strictParse) {
                    if (backup != -1 ||
                        (lastGroup != -1 && position - lastGroup != fImpl->fEffGrouping.fGrouping + 1)) {
                        strictFail = TRUE;
                        break;
                    }
                }

                // If we're only parsing integers, or if we ALREADY saw the
                // decimal, then don't parse this one.
                if (isParseIntegerOnly() || sawDecimal) {
                    break;
                }

                parsedNum.append('.', err);
                position += decimalStringLength;
                sawDecimal = TRUE;
                // Once we see a decimal character, we only accept that decimal character from then on.
                sawDecimalChar=ch;
                // decimalSet is considered to consist of (ch,ch)
            }
            else {

                if(!fBoolFlags.contains(UNUM_PARSE_NO_EXPONENT) || // don't parse if this is set unless..
                   isScientificNotation()) { // .. it's an exponent format - ignore setting and parse anyways
                    const UnicodeString *tmp;
                    tmp = &fImpl->getConstSymbol(DecimalFormatSymbols::kExponentialSymbol);
                    // TODO: CASE
                    if (!text.caseCompare(position, tmp->length(), *tmp, U_FOLD_CASE_DEFAULT))    // error code is set below if !sawDigit
                    {
                        // Parse sign, if present
                        int32_t pos = position + tmp->length();
                        char exponentSign = '+';

                        if (pos < textLength)
                        {
                            tmp = &fImpl->getConstSymbol(DecimalFormatSymbols::kPlusSignSymbol);
                            if (!text.compare(pos, tmp->length(), *tmp))
                            {
                                pos += tmp->length();
                            }
                            else {
                                tmp = &fImpl->getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
                                if (!text.compare(pos, tmp->length(), *tmp))
                                {
                                    exponentSign = '-';
                                    pos += tmp->length();
                                }
                            }
                        }

                        UBool sawExponentDigit = FALSE;
                        while (pos < textLength) {
                            ch = text[(int32_t)pos];
                            digit = ch - zero;

                            if (digit < 0 || digit > 9) {
                                digit = u_charDigitValue(ch);
                            }
                            if (0 <= digit && digit <= 9) {
                                if (!sawExponentDigit) {
                                    parsedNum.append('E', err);
                                    parsedNum.append(exponentSign, err);
                                    sawExponentDigit = TRUE;
                                }
                                ++pos;
                                parsedNum.append((char)(digit + '0'), err);
                            } else {
                                break;
                            }
                        }

                        if (sawExponentDigit) {
                            position = pos; // Advance past the exponent
                        }

                        break; // Whether we fail or succeed, we exit this loop
                    } else {
                        break;
                    }
                } else { // not parsing exponent
                    break;
              }
            }
        }

        // if we didn't see a decimal and it is required, check to see if the pattern had one
        if(!sawDecimal && isDecimalPatternMatchRequired())
        {
            if(formatPattern.indexOf(kPatternDecimalSeparator) != -1)
            {
                parsePosition.setIndex(oldStart);
                parsePosition.setErrorIndex(position);
                debug("decimal point match required fail!");
                return FALSE;
            }
        }

        if (backup != -1)
        {
            position = backup;
        }

        if (strictParse && !sawDecimal) {
            if (lastGroup != -1 && position - lastGroup != fImpl->fEffGrouping.fGrouping + 1) {
                strictFail = TRUE;
            }
        }

        if (strictFail) {
            // only set with strictParse and a grouping separator error

            parsePosition.setIndex(oldStart);
            parsePosition.setErrorIndex(position);
            debug("strictFail!");
            return FALSE;
        }

        // If there was no decimal point we have an integer

        // If none of the text string was recognized.  For example, parse
        // "x" with pattern "#0.00" (return index and error index both 0)
        // parse "$" with pattern "$#0.00". (return index 0 and error index
        // 1).
        if (!sawDigit && digitCount == 0) {
#ifdef FMT_DEBUG
            debug("none of text rec");
            printf("position=%d\n",position);
#endif
            parsePosition.setIndex(oldStart);
            parsePosition.setErrorIndex(oldStart);
            return FALSE;
        }
    }

    // Match padding before suffix
    if (formatWidth > 0 && fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforeSuffix) {
        position = skipPadding(text, position);
    }

    int32_t posSuffixMatch = -1, negSuffixMatch = -1;

    // Match positive and negative suffixes; prefer longest match.
    if (posMatch >= 0 || (!strictParse && negMatch < 0)) {
        posSuffixMatch = compareAffix(text, position, FALSE, FALSE, posSuffix, complexCurrencyParsing, type, currency);
    }
    if (negMatch >= 0) {
        negSuffixMatch = compareAffix(text, position, TRUE, FALSE, negSuffix, complexCurrencyParsing, type, currency);
    }
    if (posSuffixMatch >= 0 && negSuffixMatch >= 0) {
        if (posSuffixMatch > negSuffixMatch) {
            negSuffixMatch = -1;
        } else if (negSuffixMatch > posSuffixMatch) {
            posSuffixMatch = -1;
        }
    }

    // Fail if neither or both
    if (strictParse && ((posSuffixMatch >= 0) == (negSuffixMatch >= 0))) {
        parsePosition.setErrorIndex(position);
        debug("neither or both");
        return FALSE;
    }

    position += (posSuffixMatch >= 0 ? posSuffixMatch : (negSuffixMatch >= 0 ? negSuffixMatch : 0));

    // Match padding before suffix
    if (formatWidth > 0 && fImpl->fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterSuffix) {
        position = skipPadding(text, position);
    }

    parsePosition.setIndex(position);

    parsedNum.data()[0] = (posSuffixMatch >= 0 || (!strictParse && negMatch < 0 && negSuffixMatch < 0)) ? '+' : '-';
#ifdef FMT_DEBUG
printf("PP -> %d, SLOW = [%s]!    pp=%d, os=%d, err=%s\n", position, parsedNum.data(), parsePosition.getIndex(),oldStart,u_errorName(err));
#endif
  } /* end SLOW parse */
  if(parsePosition.getIndex() == oldStart)
    {
#ifdef FMT_DEBUG
      printf(" PP didnt move, err\n");
#endif
        parsePosition.setErrorIndex(position);
        return FALSE;
    }
#if UCONFIG_HAVE_PARSEALLINPUT
  else if (fParseAllInput==UNUM_YES&&parsePosition.getIndex()!=textLength)
    {
#ifdef FMT_DEBUG
      printf(" PP didnt consume all (UNUM_YES), err\n");
#endif
        parsePosition.setErrorIndex(position);
        return FALSE;
    }
#endif
    // uint32_t bits = (fastParseOk?kFastpathOk:0) |
    //   (fastParseHadDecimal?0:kNoDecimal);
    //printf("FPOK=%d, FPHD=%d, bits=%08X\n", fastParseOk, fastParseHadDecimal, bits);
    digits.set(parsedNum.toStringPiece(),
               err,
               0//bits
               );

    if (U_FAILURE(err)) {
#ifdef FMT_DEBUG
      printf(" err setting %s\n", u_errorName(err));
#endif
        parsePosition.setErrorIndex(position);
        return FALSE;
    }

    // check if we missed a required decimal point
    if(fastParseOk && isDecimalPatternMatchRequired())
    {
        if(formatPattern.indexOf(kPatternDecimalSeparator) != -1)
        {
            parsePosition.setIndex(oldStart);
            parsePosition.setErrorIndex(position);
            debug("decimal point match required fail!");
            return FALSE;
        }
    }


    return TRUE;
}

/**
 * Starting at position, advance past a run of pad characters, if any.
 * Return the index of the first character after position that is not a pad
 * character.  Result is >= position.
 */
int32_t DecimalFormat::skipPadding(const UnicodeString& text, int32_t position) const {
    int32_t padLen = U16_LENGTH(fImpl->fAffixes.fPadChar);
    while (position < text.length() &&
           text.char32At(position) == fImpl->fAffixes.fPadChar) {
        position += padLen;
    }
    return position;
}

/**
 * Return the length matched by the given affix, or -1 if none.
 * Runs of white space in the affix, match runs of white space in
 * the input.  Pattern white space and input white space are
 * determined differently; see code.
 * @param text input text
 * @param pos offset into input at which to begin matching
 * @param isNegative
 * @param isPrefix
 * @param affixPat affix pattern used for currency affix comparison.
 * @param complexCurrencyParsing whether it is currency parsing or not
 * @param type the currency type to parse against, LONG_NAME only or not.
 * @param currency return value for parsed currency, for generic
 * currency parsing mode, or null for normal parsing. In generic
 * currency parsing mode, any currency is parsed, not just the
 * currency that this formatter is set to.
 * @return length of input that matches, or -1 if match failure
 */
int32_t DecimalFormat::compareAffix(const UnicodeString& text,
                                    int32_t pos,
                                    UBool isNegative,
                                    UBool isPrefix,
                                    const UnicodeString* affixPat,
                                    UBool complexCurrencyParsing,
                                    int8_t type,
                                    UChar* currency) const
{
    const UnicodeString *patternToCompare;
    if (currency != NULL ||
        (fImpl->fMonetary && complexCurrencyParsing)) {

        if (affixPat != NULL) {
            return compareComplexAffix(*affixPat, text, pos, type, currency);
        }
    }

    if (isNegative) {
        if (isPrefix) {
            patternToCompare = &fImpl->fAffixes.fNegativePrefix.getOtherVariant().toString();
        }
        else {
            patternToCompare = &fImpl->fAffixes.fNegativeSuffix.getOtherVariant().toString();
        }
    }
    else {
        if (isPrefix) {
            patternToCompare = &fImpl->fAffixes.fPositivePrefix.getOtherVariant().toString();
        }
        else {
            patternToCompare = &fImpl->fAffixes.fPositiveSuffix.getOtherVariant().toString();
        }
    }
    return compareSimpleAffix(*patternToCompare, text, pos, isLenient());
}

UBool DecimalFormat::equalWithSignCompatibility(UChar32 lhs, UChar32 rhs) const {
    if (lhs == rhs) {
        return TRUE;
    }
    U_ASSERT(fStaticSets != NULL); // should already be loaded
    const UnicodeSet *minusSigns = fStaticSets->fMinusSigns;
    const UnicodeSet *plusSigns = fStaticSets->fPlusSigns;
    return (minusSigns->contains(lhs) && minusSigns->contains(rhs)) ||
        (plusSigns->contains(lhs) && plusSigns->contains(rhs));
}

// check for LRM 0x200E, RLM 0x200F, ALM 0x061C
#define IS_BIDI_MARK(c) (c==0x200E || c==0x200F || c==0x061C)

#define TRIM_BUFLEN 32
UnicodeString& DecimalFormat::trimMarksFromAffix(const UnicodeString& affix, UnicodeString& trimmedAffix) {
    UChar trimBuf[TRIM_BUFLEN];
    int32_t affixLen = affix.length();
    int32_t affixPos, trimLen = 0;

    for (affixPos = 0; affixPos < affixLen; affixPos++) {
        UChar c = affix.charAt(affixPos);
        if (!IS_BIDI_MARK(c)) {
            if (trimLen < TRIM_BUFLEN) {
                trimBuf[trimLen++] = c;
            } else {
                trimLen = 0;
                break;
            }
        }
    }
    return (trimLen > 0)? trimmedAffix.setTo(trimBuf, trimLen): trimmedAffix.setTo(affix);
}

/**
 * Return the length matched by the given affix, or -1 if none.
 * Runs of white space in the affix, match runs of white space in
 * the input.  Pattern white space and input white space are
 * determined differently; see code.
 * @param affix pattern string, taken as a literal
 * @param input input text
 * @param pos offset into input at which to begin matching
 * @return length of input that matches, or -1 if match failure
 */
int32_t DecimalFormat::compareSimpleAffix(const UnicodeString& affix,
                                          const UnicodeString& input,
                                          int32_t pos,
                                          UBool lenient) const {
    int32_t start = pos;
    UnicodeString trimmedAffix;
    // For more efficiency we should keep lazily-created trimmed affixes around in
    // instance variables instead of trimming each time they are used (the next step)
    trimMarksFromAffix(affix, trimmedAffix);
    UChar32 affixChar = trimmedAffix.char32At(0);
    int32_t affixLength = trimmedAffix.length();
    int32_t inputLength = input.length();
    int32_t affixCharLength = U16_LENGTH(affixChar);
    UnicodeSet *affixSet;
    UErrorCode status = U_ZERO_ERROR;

    U_ASSERT(fStaticSets != NULL); // should already be loaded

    if (U_FAILURE(status)) {
        return -1;
    }
    if (!lenient) {
        affixSet = fStaticSets->fStrictDashEquivalents;

        // If the trimmedAffix is exactly one character long and that character
        // is in the dash set and the very next input character is also
        // in the dash set, return a match.
        if (affixCharLength == affixLength && affixSet->contains(affixChar))  {
            UChar32 ic = input.char32At(pos);
            if (affixSet->contains(ic)) {
                pos += U16_LENGTH(ic);
                pos = skipBidiMarks(input, pos); // skip any trailing bidi marks
                return pos - start;
            }
        }

        for (int32_t i = 0; i < affixLength; ) {
            UChar32 c = trimmedAffix.char32At(i);
            int32_t len = U16_LENGTH(c);
            if (PatternProps::isWhiteSpace(c)) {
                // We may have a pattern like: \u200F \u0020
                //        and input text like: \u200F \u0020
                // Note that U+200F and U+0020 are Pattern_White_Space but only
                // U+0020 is UWhiteSpace.  So we have to first do a direct
                // match of the run of Pattern_White_Space in the pattern,
                // then match any extra characters.
                UBool literalMatch = FALSE;
                while (pos < inputLength) {
                    UChar32 ic = input.char32At(pos);
                    if (ic == c) {
                        literalMatch = TRUE;
                        i += len;
                        pos += len;
                        if (i == affixLength) {
                            break;
                        }
                        c = trimmedAffix.char32At(i);
                        len = U16_LENGTH(c);
                        if (!PatternProps::isWhiteSpace(c)) {
                            break;
                        }
                    } else if (IS_BIDI_MARK(ic)) {
                        pos ++; // just skip over this input text
                    } else {
                        break;
                    }
                }

                // Advance over run in pattern
                i = skipPatternWhiteSpace(trimmedAffix, i);

                // Advance over run in input text
                // Must see at least one white space char in input,
                // unless we've already matched some characters literally.
                int32_t s = pos;
                pos = skipUWhiteSpace(input, pos);
                if (pos == s && !literalMatch) {
                    return -1;
                }

                // If we skip UWhiteSpace in the input text, we need to skip it in the pattern.
                // Otherwise, the previous lines may have skipped over text (such as U+00A0) that
                // is also in the trimmedAffix.
                i = skipUWhiteSpace(trimmedAffix, i);
            } else {
                UBool match = FALSE;
                while (pos < inputLength) {
                    UChar32 ic = input.char32At(pos);
                    if (!match && ic == c) {
                        i += len;
                        pos += len;
                        match = TRUE;
                    } else if (IS_BIDI_MARK(ic)) {
                        pos++; // just skip over this input text
                    } else {
                        break;
                    }
                }
                if (!match) {
                    return -1;
                }
            }
        }
    } else {
        UBool match = FALSE;

        affixSet = fStaticSets->fDashEquivalents;

        if (affixCharLength == affixLength && affixSet->contains(affixChar))  {
            pos = skipUWhiteSpaceAndMarks(input, pos);
            UChar32 ic = input.char32At(pos);

            if (affixSet->contains(ic)) {
                pos += U16_LENGTH(ic);
                pos = skipBidiMarks(input, pos);
                return pos - start;
            }
        }

        for (int32_t i = 0; i < affixLength; )
        {
            //i = skipRuleWhiteSpace(trimmedAffix, i);
            i = skipUWhiteSpace(trimmedAffix, i);
            pos = skipUWhiteSpaceAndMarks(input, pos);

            if (i >= affixLength || pos >= inputLength) {
                break;
            }

            UChar32 c = trimmedAffix.char32At(i);
            UChar32 ic = input.char32At(pos);

            if (!equalWithSignCompatibility(ic, c)) {
                return -1;
            }

            match = TRUE;
            i += U16_LENGTH(c);
            pos += U16_LENGTH(ic);
            pos = skipBidiMarks(input, pos);
        }

        if (affixLength > 0 && ! match) {
            return -1;
        }
    }
    return pos - start;
}

/**
 * Skip over a run of zero or more Pattern_White_Space characters at
 * pos in text.
 */
int32_t DecimalFormat::skipPatternWhiteSpace(const UnicodeString& text, int32_t pos) {
    const UChar* s = text.getBuffer();
    return (int32_t)(PatternProps::skipWhiteSpace(s + pos, text.length() - pos) - s);
}

/**
 * Skip over a run of zero or more isUWhiteSpace() characters at pos
 * in text.
 */
int32_t DecimalFormat::skipUWhiteSpace(const UnicodeString& text, int32_t pos) {
    while (pos < text.length()) {
        UChar32 c = text.char32At(pos);
        if (!u_isUWhiteSpace(c)) {
            break;
        }
        pos += U16_LENGTH(c);
    }
    return pos;
}

/**
 * Skip over a run of zero or more isUWhiteSpace() characters or bidi marks at pos
 * in text.
 */
int32_t DecimalFormat::skipUWhiteSpaceAndMarks(const UnicodeString& text, int32_t pos) {
    while (pos < text.length()) {
        UChar32 c = text.char32At(pos);
        if (!u_isUWhiteSpace(c) && !IS_BIDI_MARK(c)) { // u_isUWhiteSpace doesn't include LRM,RLM,ALM
            break;
        }
        pos += U16_LENGTH(c);
    }
    return pos;
}

/**
 * Skip over a run of zero or more bidi marks at pos in text.
 */
int32_t DecimalFormat::skipBidiMarks(const UnicodeString& text, int32_t pos) {
    while (pos < text.length()) {
        UChar c = text.charAt(pos);
        if (!IS_BIDI_MARK(c)) {
            break;
        }
        pos++;
    }
    return pos;
}

/**
 * Return the length matched by the given affix, or -1 if none.
 * @param affixPat pattern string
 * @param input input text
 * @param pos offset into input at which to begin matching
 * @param type the currency type to parse against, LONG_NAME only or not.
 * @param currency return value for parsed currency, for generic
 * currency parsing mode, or null for normal parsing. In generic
 * currency parsing mode, any currency is parsed, not just the
 * currency that this formatter is set to.
 * @return length of input that matches, or -1 if match failure
 */
int32_t DecimalFormat::compareComplexAffix(const UnicodeString& affixPat,
                                           const UnicodeString& text,
                                           int32_t pos,
                                           int8_t type,
                                           UChar* currency) const
{
    int32_t start = pos;
    U_ASSERT(currency != NULL || fImpl->fMonetary);

    for (int32_t i=0;
         i<affixPat.length() && pos >= 0; ) {
        UChar32 c = affixPat.char32At(i);
        i += U16_LENGTH(c);

        if (c == kQuote) {
            U_ASSERT(i <= affixPat.length());
            c = affixPat.char32At(i);
            i += U16_LENGTH(c);

            const UnicodeString* affix = NULL;

            switch (c) {
            case kCurrencySign: {
                // since the currency names in choice format is saved
                // the same way as other currency names,
                // do not need to do currency choice parsing here.
                // the general currency parsing parse against all names,
                // including names in choice format.
                UBool intl = i<affixPat.length() &&
                    affixPat.char32At(i) == kCurrencySign;
                if (intl) {
                    ++i;
                }
                UBool plural = i<affixPat.length() &&
                    affixPat.char32At(i) == kCurrencySign;
                if (plural) {
                    ++i;
                    intl = FALSE;
                }
                // Parse generic currency -- anything for which we
                // have a display name, or any 3-letter ISO code.
                // Try to parse display name for our locale; first
                // determine our locale.
                const char* loc = fCurrencyPluralInfo->getLocale().getName();
                ParsePosition ppos(pos);
                UChar curr[4];
                UErrorCode ec = U_ZERO_ERROR;
                // Delegate parse of display name => ISO code to Currency
                uprv_parseCurrency(loc, text, ppos, type, curr, ec);

                // If parse succeeds, populate currency[0]
                if (U_SUCCESS(ec) && ppos.getIndex() != pos) {
                    if (currency) {
                        u_strcpy(currency, curr);
                    } else {
                        // The formatter is currency-style but the client has not requested
                        // the value of the parsed currency. In this case, if that value does
                        // not match the formatter's current value, then the parse fails.
                        UChar effectiveCurr[4];
                        getEffectiveCurrency(effectiveCurr, ec);
                        if ( U_FAILURE(ec) || u_strncmp(curr,effectiveCurr,4) != 0 ) {
                            pos = -1;
                            continue;
                        }
                    }
                    pos = ppos.getIndex();
                } else if (!isLenient()){
                    pos = -1;
                }
                continue;
            }
            case kPatternPercent:
                affix = &fImpl->getConstSymbol(DecimalFormatSymbols::kPercentSymbol);
                break;
            case kPatternPerMill:
                affix = &fImpl->getConstSymbol(DecimalFormatSymbols::kPerMillSymbol);
                break;
            case kPatternPlus:
                affix = &fImpl->getConstSymbol(DecimalFormatSymbols::kPlusSignSymbol);
                break;
            case kPatternMinus:
                affix = &fImpl->getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
                break;
            default:
                // fall through to affix!=0 test, which will fail
                break;
            }

            if (affix != NULL) {
                pos = match(text, pos, *affix);
                continue;
            }
        }

        pos = match(text, pos, c);
        if (PatternProps::isWhiteSpace(c)) {
            i = skipPatternWhiteSpace(affixPat, i);
        }
    }
    return pos - start;
}

/**
 * Match a single character at text[pos] and return the index of the
 * next character upon success.  Return -1 on failure.  If
 * ch is a Pattern_White_Space then match a run of white space in text.
 */
int32_t DecimalFormat::match(const UnicodeString& text, int32_t pos, UChar32 ch) {
    if (PatternProps::isWhiteSpace(ch)) {
        // Advance over run of white space in input text
        // Must see at least one white space char in input
        int32_t s = pos;
        pos = skipPatternWhiteSpace(text, pos);
        if (pos == s) {
            return -1;
        }
        return pos;
    }
    return (pos >= 0 && text.char32At(pos) == ch) ?
        (pos + U16_LENGTH(ch)) : -1;
}

/**
 * Match a string at text[pos] and return the index of the next
 * character upon success.  Return -1 on failure.  Match a run of
 * white space in str with a run of white space in text.
 */
int32_t DecimalFormat::match(const UnicodeString& text, int32_t pos, const UnicodeString& str) {
    for (int32_t i=0; i<str.length() && pos >= 0; ) {
        UChar32 ch = str.char32At(i);
        i += U16_LENGTH(ch);
        if (PatternProps::isWhiteSpace(ch)) {
            i = skipPatternWhiteSpace(str, i);
        }
        pos = match(text, pos, ch);
    }
    return pos;
}

UBool DecimalFormat::matchSymbol(const UnicodeString &text, int32_t position, int32_t length, const UnicodeString &symbol,
                         UnicodeSet *sset, UChar32 schar)
{
    if (sset != NULL) {
        return sset->contains(schar);
    }

    return text.compare(position, length, symbol) == 0;
}

UBool DecimalFormat::matchDecimal(UChar32 symbolChar,
                            UBool sawDecimal,  UChar32 sawDecimalChar,
                             const UnicodeSet *sset, UChar32 schar) {
   if(sawDecimal) {
       return schar==sawDecimalChar;
   } else if(schar==symbolChar) {
       return TRUE;
   } else if(sset!=NULL) {
        return sset->contains(schar);
   } else {
       return FALSE;
   }
}

UBool DecimalFormat::matchGrouping(UChar32 groupingChar,
                            UBool sawGrouping, UChar32 sawGroupingChar,
                             const UnicodeSet *sset,
                             UChar32 /*decimalChar*/, const UnicodeSet *decimalSet,
                             UChar32 schar) {
    if(sawGrouping) {
        return schar==sawGroupingChar;  // previously found
    } else if(schar==groupingChar) {
        return TRUE; // char from symbols
    } else if(sset!=NULL) {
        return sset->contains(schar) &&  // in groupingSet but...
           ((decimalSet==NULL) || !decimalSet->contains(schar)); // Exclude decimalSet from groupingSet
    } else {
        return FALSE;
    }
}



//------------------------------------------------------------------------------
// Gets the pointer to the localized decimal format symbols

const DecimalFormatSymbols*
DecimalFormat::getDecimalFormatSymbols() const
{
    return &fImpl->getDecimalFormatSymbols();
}

//------------------------------------------------------------------------------
// De-owning the current localized symbols and adopt the new symbols.

void
DecimalFormat::adoptDecimalFormatSymbols(DecimalFormatSymbols* symbolsToAdopt)
{
    if (symbolsToAdopt == NULL) {
        return; // do not allow caller to set fSymbols to NULL
    }
    fImpl->adoptDecimalFormatSymbols(symbolsToAdopt);
}
//------------------------------------------------------------------------------
// Setting the symbols is equlivalent to adopting a newly created localized
// symbols.

void
DecimalFormat::setDecimalFormatSymbols(const DecimalFormatSymbols& symbols)
{
    adoptDecimalFormatSymbols(new DecimalFormatSymbols(symbols));
}


const CurrencyPluralInfo*
DecimalFormat::getCurrencyPluralInfo(void) const
{
    return fCurrencyPluralInfo;
}


void
DecimalFormat::adoptCurrencyPluralInfo(CurrencyPluralInfo* toAdopt)
{
    if (toAdopt != NULL) {
        delete fCurrencyPluralInfo;
        fCurrencyPluralInfo = toAdopt;
        // re-set currency affix patterns and currency affixes.
        if (fImpl->fMonetary) {
            UErrorCode status = U_ZERO_ERROR;
            if (fAffixPatternsForCurrency) {
                deleteHashForAffixPattern();
            }
            setupCurrencyAffixPatterns(status);
        }
    }
}

void
DecimalFormat::setCurrencyPluralInfo(const CurrencyPluralInfo& info)
{
    adoptCurrencyPluralInfo(info.clone());
}


//------------------------------------------------------------------------------
// Gets the positive prefix of the number pattern.

UnicodeString&
DecimalFormat::getPositivePrefix(UnicodeString& result) const
{
    return fImpl->getPositivePrefix(result);
}

//------------------------------------------------------------------------------
// Sets the positive prefix of the number pattern.

void
DecimalFormat::setPositivePrefix(const UnicodeString& newValue)
{
    fImpl->setPositivePrefix(newValue);
}

//------------------------------------------------------------------------------
// Gets the negative prefix  of the number pattern.

UnicodeString&
DecimalFormat::getNegativePrefix(UnicodeString& result) const
{
    return fImpl->getNegativePrefix(result);
}

//------------------------------------------------------------------------------
// Gets the negative prefix  of the number pattern.

void
DecimalFormat::setNegativePrefix(const UnicodeString& newValue)
{
    fImpl->setNegativePrefix(newValue);
}

//------------------------------------------------------------------------------
// Gets the positive suffix of the number pattern.

UnicodeString&
DecimalFormat::getPositiveSuffix(UnicodeString& result) const
{
    return fImpl->getPositiveSuffix(result);
}

//------------------------------------------------------------------------------
// Sets the positive suffix of the number pattern.

void
DecimalFormat::setPositiveSuffix(const UnicodeString& newValue)
{
    fImpl->setPositiveSuffix(newValue);
}

//------------------------------------------------------------------------------
// Gets the negative suffix of the number pattern.

UnicodeString&
DecimalFormat::getNegativeSuffix(UnicodeString& result) const
{
    return fImpl->getNegativeSuffix(result);
}

//------------------------------------------------------------------------------
// Sets the negative suffix of the number pattern.

void
DecimalFormat::setNegativeSuffix(const UnicodeString& newValue)
{
    fImpl->setNegativeSuffix(newValue);
}

//------------------------------------------------------------------------------
// Gets the multiplier of the number pattern.
//   Multipliers are stored as decimal numbers (DigitLists) because that
//      is the most convenient for muliplying or dividing the numbers to be formatted.
//   A NULL multiplier implies one, and the scaling operations are skipped.

int32_t
DecimalFormat::getMultiplier() const
{
    return fImpl->getMultiplier();
}

//------------------------------------------------------------------------------
// Sets the multiplier of the number pattern.
void
DecimalFormat::setMultiplier(int32_t newValue)
{
    fImpl->setMultiplier(newValue);
}

/**
 * Get the rounding increment.
 * @return A positive rounding increment, or 0.0 if rounding
 * is not in effect.
 * @see #setRoundingIncrement
 * @see #getRoundingMode
 * @see #setRoundingMode
 */
double DecimalFormat::getRoundingIncrement() const {
    return fImpl->getRoundingIncrement();
}

/**
 * Set the rounding increment.  This method also controls whether
 * rounding is enabled.
 * @param newValue A positive rounding increment, or 0.0 to disable rounding.
 * Negative increments are equivalent to 0.0.
 * @see #getRoundingIncrement
 * @see #getRoundingMode
 * @see #setRoundingMode
 */
void DecimalFormat::setRoundingIncrement(double newValue) {
    fImpl->setRoundingIncrement(newValue);
}

/**
 * Get the rounding mode.
 * @return A rounding mode
 * @see #setRoundingIncrement
 * @see #getRoundingIncrement
 * @see #setRoundingMode
 */
DecimalFormat::ERoundingMode DecimalFormat::getRoundingMode() const {
    return fImpl->getRoundingMode();
}

/**
 * Set the rounding mode.  This has no effect unless the rounding
 * increment is greater than zero.
 * @param roundingMode A rounding mode
 * @see #setRoundingIncrement
 * @see #getRoundingIncrement
 * @see #getRoundingMode
 */
void DecimalFormat::setRoundingMode(ERoundingMode roundingMode) {
    fImpl->setRoundingMode(roundingMode);
}

/**
 * Get the width to which the output of <code>format()</code> is padded.
 * @return the format width, or zero if no padding is in effect
 * @see #setFormatWidth
 * @see #getPadCharacter
 * @see #setPadCharacter
 * @see #getPadPosition
 * @see #setPadPosition
 */
int32_t DecimalFormat::getFormatWidth() const {
    return fImpl->getFormatWidth();
}

/**
 * Set the width to which the output of <code>format()</code> is padded.
 * This method also controls whether padding is enabled.
 * @param width the width to which to pad the result of
 * <code>format()</code>, or zero to disable padding.  A negative
 * width is equivalent to 0.
 * @see #getFormatWidth
 * @see #getPadCharacter
 * @see #setPadCharacter
 * @see #getPadPosition
 * @see #setPadPosition
 */
void DecimalFormat::setFormatWidth(int32_t width) {
    int32_t formatWidth = (width > 0) ? width : 0;
    fImpl->setFormatWidth(formatWidth);
}

UnicodeString DecimalFormat::getPadCharacterString() const {
    return UnicodeString(fImpl->getPadCharacter());
}

void DecimalFormat::setPadCharacter(const UnicodeString &padChar) {
    UChar pad;
    if (padChar.length() > 0) {
        pad = padChar.char32At(0);
    }
    else {
        pad = kDefaultPad;
    }
    fImpl->setPadCharacter(pad);
}

static DecimalFormat::EPadPosition fromPadPosition(DigitAffixesAndPadding::EPadPosition padPos) {
    switch (padPos) {
    case DigitAffixesAndPadding::kPadBeforePrefix:
        return DecimalFormat::kPadBeforePrefix;
    case DigitAffixesAndPadding::kPadAfterPrefix:
        return DecimalFormat::kPadAfterPrefix;
    case DigitAffixesAndPadding::kPadBeforeSuffix:
        return DecimalFormat::kPadBeforeSuffix;
    case DigitAffixesAndPadding::kPadAfterSuffix:
        return DecimalFormat::kPadAfterSuffix;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return DecimalFormat::kPadBeforePrefix;
}

/**
 * Get the position at which padding will take place.  This is the location
 * at which padding will be inserted if the result of <code>format()</code>
 * is shorter than the format width.
 * @return the pad position, one of <code>kPadBeforePrefix</code>,
 * <code>kPadAfterPrefix</code>, <code>kPadBeforeSuffix</code>, or
 * <code>kPadAfterSuffix</code>.
 * @see #setFormatWidth
 * @see #getFormatWidth
 * @see #setPadCharacter
 * @see #getPadCharacter
 * @see #setPadPosition
 * @see #kPadBeforePrefix
 * @see #kPadAfterPrefix
 * @see #kPadBeforeSuffix
 * @see #kPadAfterSuffix
 */
DecimalFormat::EPadPosition DecimalFormat::getPadPosition() const {
    return fromPadPosition(fImpl->getPadPosition());
}

static DigitAffixesAndPadding::EPadPosition toPadPosition(DecimalFormat::EPadPosition padPos) {
    switch (padPos) {
    case DecimalFormat::kPadBeforePrefix:
        return DigitAffixesAndPadding::kPadBeforePrefix;
    case DecimalFormat::kPadAfterPrefix:
        return DigitAffixesAndPadding::kPadAfterPrefix;
    case DecimalFormat::kPadBeforeSuffix:
        return DigitAffixesAndPadding::kPadBeforeSuffix;
    case DecimalFormat::kPadAfterSuffix:
        return DigitAffixesAndPadding::kPadAfterSuffix;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return DigitAffixesAndPadding::kPadBeforePrefix;
}

/**
 * <strong><font face=helvetica color=red>NEW</font></strong>
 * Set the position at which padding will take place.  This is the location
 * at which padding will be inserted if the result of <code>format()</code>
 * is shorter than the format width.  This has no effect unless padding is
 * enabled.
 * @param padPos the pad position, one of <code>kPadBeforePrefix</code>,
 * <code>kPadAfterPrefix</code>, <code>kPadBeforeSuffix</code>, or
 * <code>kPadAfterSuffix</code>.
 * @see #setFormatWidth
 * @see #getFormatWidth
 * @see #setPadCharacter
 * @see #getPadCharacter
 * @see #getPadPosition
 * @see #kPadBeforePrefix
 * @see #kPadAfterPrefix
 * @see #kPadBeforeSuffix
 * @see #kPadAfterSuffix
 */
void DecimalFormat::setPadPosition(EPadPosition padPos) {
    fImpl->setPadPosition(toPadPosition(padPos));
}

/**
 * Return whether or not scientific notation is used.
 * @return TRUE if this object formats and parses scientific notation
 * @see #setScientificNotation
 * @see #getMinimumExponentDigits
 * @see #setMinimumExponentDigits
 * @see #isExponentSignAlwaysShown
 * @see #setExponentSignAlwaysShown
 */
UBool DecimalFormat::isScientificNotation() const {
    return fImpl->isScientificNotation();
}

/**
 * Set whether or not scientific notation is used.
 * @param useScientific TRUE if this object formats and parses scientific
 * notation
 * @see #isScientificNotation
 * @see #getMinimumExponentDigits
 * @see #setMinimumExponentDigits
 * @see #isExponentSignAlwaysShown
 * @see #setExponentSignAlwaysShown
 */
void DecimalFormat::setScientificNotation(UBool useScientific) {
    fImpl->setScientificNotation(useScientific);
}

/**
 * Return the minimum exponent digits that will be shown.
 * @return the minimum exponent digits that will be shown
 * @see #setScientificNotation
 * @see #isScientificNotation
 * @see #setMinimumExponentDigits
 * @see #isExponentSignAlwaysShown
 * @see #setExponentSignAlwaysShown
 */
int8_t DecimalFormat::getMinimumExponentDigits() const {
    return fImpl->getMinimumExponentDigits();
}

/**
 * Set the minimum exponent digits that will be shown.  This has no
 * effect unless scientific notation is in use.
 * @param minExpDig a value >= 1 indicating the fewest exponent digits
 * that will be shown.  Values less than 1 will be treated as 1.
 * @see #setScientificNotation
 * @see #isScientificNotation
 * @see #getMinimumExponentDigits
 * @see #isExponentSignAlwaysShown
 * @see #setExponentSignAlwaysShown
 */
void DecimalFormat::setMinimumExponentDigits(int8_t minExpDig) {
    int32_t minExponentDigits = (int8_t)((minExpDig > 0) ? minExpDig : 1);
    fImpl->setMinimumExponentDigits(minExponentDigits);
}

/**
 * Return whether the exponent sign is always shown.
 * @return TRUE if the exponent is always prefixed with either the
 * localized minus sign or the localized plus sign, false if only negative
 * exponents are prefixed with the localized minus sign.
 * @see #setScientificNotation
 * @see #isScientificNotation
 * @see #setMinimumExponentDigits
 * @see #getMinimumExponentDigits
 * @see #setExponentSignAlwaysShown
 */
UBool DecimalFormat::isExponentSignAlwaysShown() const {
    return fImpl->isExponentSignAlwaysShown();
}

/**
 * Set whether the exponent sign is always shown.  This has no effect
 * unless scientific notation is in use.
 * @param expSignAlways TRUE if the exponent is always prefixed with either
 * the localized minus sign or the localized plus sign, false if only
 * negative exponents are prefixed with the localized minus sign.
 * @see #setScientificNotation
 * @see #isScientificNotation
 * @see #setMinimumExponentDigits
 * @see #getMinimumExponentDigits
 * @see #isExponentSignAlwaysShown
 */
void DecimalFormat::setExponentSignAlwaysShown(UBool expSignAlways) {
    fImpl->setExponentSignAlwaysShown(expSignAlways);
}

//------------------------------------------------------------------------------
// Gets the grouping size of the number pattern.  For example, thousand or 10
// thousand groupings.

int32_t
DecimalFormat::getGroupingSize() const
{
    return fImpl->getGroupingSize();
}

//------------------------------------------------------------------------------
// Gets the grouping size of the number pattern.

void
DecimalFormat::setGroupingSize(int32_t newValue)
{
    fImpl->setGroupingSize(newValue);
}

//------------------------------------------------------------------------------

int32_t
DecimalFormat::getSecondaryGroupingSize() const
{
    return fImpl->getSecondaryGroupingSize();
}

//------------------------------------------------------------------------------

void
DecimalFormat::setSecondaryGroupingSize(int32_t newValue)
{
    fImpl->setSecondaryGroupingSize(newValue);
}

//------------------------------------------------------------------------------

int32_t
DecimalFormat::getMinimumGroupingDigits() const
{
    return fImpl->getMinimumGroupingDigits();
}

//------------------------------------------------------------------------------

void
DecimalFormat::setMinimumGroupingDigits(int32_t newValue)
{
    fImpl->setMinimumGroupingDigits(newValue);
}

//------------------------------------------------------------------------------
// Checks if to show the decimal separator.

UBool
DecimalFormat::isDecimalSeparatorAlwaysShown() const
{
    return fImpl->isDecimalSeparatorAlwaysShown();
}

//------------------------------------------------------------------------------
// Sets to always show the decimal separator.

void
DecimalFormat::setDecimalSeparatorAlwaysShown(UBool newValue)
{
    fImpl->setDecimalSeparatorAlwaysShown(newValue);
}

//------------------------------------------------------------------------------
// Checks if decimal point pattern match is required
UBool
DecimalFormat::isDecimalPatternMatchRequired(void) const
{
    return fBoolFlags.contains(UNUM_PARSE_DECIMAL_MARK_REQUIRED);
}

//------------------------------------------------------------------------------
// Checks if decimal point pattern match is required

void
DecimalFormat::setDecimalPatternMatchRequired(UBool newValue)
{
    fBoolFlags.set(UNUM_PARSE_DECIMAL_MARK_REQUIRED, newValue);
}


//------------------------------------------------------------------------------
// Emits the pattern of this DecimalFormat instance.

UnicodeString&
DecimalFormat::toPattern(UnicodeString& result) const
{
    return fImpl->toPattern(result);
}

//------------------------------------------------------------------------------
// Emits the localized pattern this DecimalFormat instance.

UnicodeString&
DecimalFormat::toLocalizedPattern(UnicodeString& result) const
{
    // toLocalizedPattern is deprecated, so we just make it the same as
    // toPattern.
    return fImpl->toPattern(result);
}

//------------------------------------------------------------------------------

void
DecimalFormat::applyPattern(const UnicodeString& pattern, UErrorCode& status)
{
    if (pattern.indexOf(kCurrencySign) != -1) {
        handleCurrencySignInPattern(status);
    }
    fImpl->applyPattern(pattern, status);
}

//------------------------------------------------------------------------------

void
DecimalFormat::applyPattern(const UnicodeString& pattern,
                            UParseError& parseError,
                            UErrorCode& status)
{
    if (pattern.indexOf(kCurrencySign) != -1) {
        handleCurrencySignInPattern(status);
    }
    fImpl->applyPattern(pattern, parseError, status);
}
//------------------------------------------------------------------------------

void
DecimalFormat::applyLocalizedPattern(const UnicodeString& pattern, UErrorCode& status)
{
    if (pattern.indexOf(kCurrencySign) != -1) {
        handleCurrencySignInPattern(status);
    }
    fImpl->applyLocalizedPattern(pattern, status);
}

//------------------------------------------------------------------------------

void
DecimalFormat::applyLocalizedPattern(const UnicodeString& pattern,
                                     UParseError& parseError,
                                     UErrorCode& status)
{
    if (pattern.indexOf(kCurrencySign) != -1) {
        handleCurrencySignInPattern(status);
    }
    fImpl->applyLocalizedPattern(pattern, parseError, status);
}

//------------------------------------------------------------------------------

/**
 * Sets the maximum number of digits allowed in the integer portion of a
 * number.
 * @see NumberFormat#setMaximumIntegerDigits
 */
void DecimalFormat::setMaximumIntegerDigits(int32_t newValue) {
    newValue = _min(newValue, gDefaultMaxIntegerDigits);
    NumberFormat::setMaximumIntegerDigits(newValue);
    fImpl->updatePrecision();
}

/**
 * Sets the minimum number of digits allowed in the integer portion of a
 * number. This override limits the integer digit count to 309.
 * @see NumberFormat#setMinimumIntegerDigits
 */
void DecimalFormat::setMinimumIntegerDigits(int32_t newValue) {
    newValue = _min(newValue, kDoubleIntegerDigits);
    NumberFormat::setMinimumIntegerDigits(newValue);
    fImpl->updatePrecision();
}

/**
 * Sets the maximum number of digits allowed in the fraction portion of a
 * number. This override limits the fraction digit count to 340.
 * @see NumberFormat#setMaximumFractionDigits
 */
void DecimalFormat::setMaximumFractionDigits(int32_t newValue) {
    newValue = _min(newValue, kDoubleFractionDigits);
    NumberFormat::setMaximumFractionDigits(newValue);
    fImpl->updatePrecision();
}

/**
 * Sets the minimum number of digits allowed in the fraction portion of a
 * number. This override limits the fraction digit count to 340.
 * @see NumberFormat#setMinimumFractionDigits
 */
void DecimalFormat::setMinimumFractionDigits(int32_t newValue) {
    newValue = _min(newValue, kDoubleFractionDigits);
    NumberFormat::setMinimumFractionDigits(newValue);
    fImpl->updatePrecision();
}

int32_t DecimalFormat::getMinimumSignificantDigits() const {
    return fImpl->getMinimumSignificantDigits();
}

int32_t DecimalFormat::getMaximumSignificantDigits() const {
    return fImpl->getMaximumSignificantDigits();
}

void DecimalFormat::setMinimumSignificantDigits(int32_t min) {
    if (min < 1) {
        min = 1;
    }
    // pin max sig dig to >= min
    int32_t max = _max(fImpl->fMaxSigDigits, min);
    fImpl->setMinMaxSignificantDigits(min, max);
}

void DecimalFormat::setMaximumSignificantDigits(int32_t max) {
    if (max < 1) {
        max = 1;
    }
    // pin min sig dig to 1..max
    U_ASSERT(fImpl->fMinSigDigits >= 1);
    int32_t min = _min(fImpl->fMinSigDigits, max);
    fImpl->setMinMaxSignificantDigits(min, max);
}

UBool DecimalFormat::areSignificantDigitsUsed() const {
    return fImpl->areSignificantDigitsUsed();
}

void DecimalFormat::setSignificantDigitsUsed(UBool useSignificantDigits) {
    fImpl->setSignificantDigitsUsed(useSignificantDigits);
}

void DecimalFormat::setCurrency(const UChar* theCurrency, UErrorCode& ec) {
    // set the currency before compute affixes to get the right currency names
    NumberFormat::setCurrency(theCurrency, ec);
    fImpl->updateCurrency(ec);
}

void DecimalFormat::setCurrencyUsage(UCurrencyUsage newContext, UErrorCode* ec){
    fImpl->setCurrencyUsage(newContext, *ec);
}

UCurrencyUsage DecimalFormat::getCurrencyUsage() const {
    return fImpl->getCurrencyUsage();
}

// Deprecated variant with no UErrorCode parameter
void DecimalFormat::setCurrency(const UChar* theCurrency) {
    UErrorCode ec = U_ZERO_ERROR;
    setCurrency(theCurrency, ec);
}

void DecimalFormat::getEffectiveCurrency(UChar* result, UErrorCode& ec) const {
    if (fImpl->fSymbols == NULL) {
        ec = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    ec = U_ZERO_ERROR;
    const UChar* c = getCurrency();
    if (*c == 0) {
        const UnicodeString &intl =
            fImpl->getConstSymbol(DecimalFormatSymbols::kIntlCurrencySymbol);
        c = intl.getBuffer(); // ok for intl to go out of scope
    }
    u_strncpy(result, c, 3);
    result[3] = 0;
}

Hashtable*
DecimalFormat::initHashForAffixPattern(UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    Hashtable* hTable;
    if ( (hTable = new Hashtable(TRUE, status)) == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if ( U_FAILURE(status) ) {
        delete hTable;
        return NULL;
    }
    hTable->setValueComparator(decimfmtAffixPatternValueComparator);
    return hTable;
}

void
DecimalFormat::deleteHashForAffixPattern()
{
    if ( fAffixPatternsForCurrency == NULL ) {
        return;
    }
    int32_t pos = UHASH_FIRST;
    const UHashElement* element = NULL;
    while ( (element = fAffixPatternsForCurrency->nextElement(pos)) != NULL ) {
        const UHashTok valueTok = element->value;
        const AffixPatternsForCurrency* value = (AffixPatternsForCurrency*)valueTok.pointer;
        delete value;
    }
    delete fAffixPatternsForCurrency;
    fAffixPatternsForCurrency = NULL;
}


void
DecimalFormat::copyHashForAffixPattern(const Hashtable* source,
                                       Hashtable* target,
                                       UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    int32_t pos = UHASH_FIRST;
    const UHashElement* element = NULL;
    if ( source ) {
        while ( (element = source->nextElement(pos)) != NULL ) {
            const UHashTok keyTok = element->key;
            const UnicodeString* key = (UnicodeString*)keyTok.pointer;
            const UHashTok valueTok = element->value;
            const AffixPatternsForCurrency* value = (AffixPatternsForCurrency*)valueTok.pointer;
            AffixPatternsForCurrency* copy = new AffixPatternsForCurrency(
                value->negPrefixPatternForCurrency,
                value->negSuffixPatternForCurrency,
                value->posPrefixPatternForCurrency,
                value->posSuffixPatternForCurrency,
                value->patternType);
            target->put(UnicodeString(*key), copy, status);
            if ( U_FAILURE(status) ) {
                return;
            }
        }
    }
}

void
DecimalFormat::setGroupingUsed(UBool newValue) {
  NumberFormat::setGroupingUsed(newValue);
  fImpl->updateGrouping();
}

void
DecimalFormat::setParseIntegerOnly(UBool newValue) {
  NumberFormat::setParseIntegerOnly(newValue);
}

void
DecimalFormat::setContext(UDisplayContext value, UErrorCode& status) {
  NumberFormat::setContext(value, status);
}

DecimalFormat& DecimalFormat::setAttribute( UNumberFormatAttribute attr,
                                            int32_t newValue,
                                            UErrorCode &status) {
  if(U_FAILURE(status)) return *this;

  switch(attr) {
  case UNUM_LENIENT_PARSE:
    setLenient(newValue!=0);
    break;

    case UNUM_PARSE_INT_ONLY:
      setParseIntegerOnly(newValue!=0);
      break;

    case UNUM_GROUPING_USED:
      setGroupingUsed(newValue!=0);
      break;

    case UNUM_DECIMAL_ALWAYS_SHOWN:
      setDecimalSeparatorAlwaysShown(newValue!=0);
        break;

    case UNUM_MAX_INTEGER_DIGITS:
      setMaximumIntegerDigits(newValue);
        break;

    case UNUM_MIN_INTEGER_DIGITS:
      setMinimumIntegerDigits(newValue);
        break;

    case UNUM_INTEGER_DIGITS:
      setMinimumIntegerDigits(newValue);
      setMaximumIntegerDigits(newValue);
        break;

    case UNUM_MAX_FRACTION_DIGITS:
      setMaximumFractionDigits(newValue);
        break;

    case UNUM_MIN_FRACTION_DIGITS:
      setMinimumFractionDigits(newValue);
        break;

    case UNUM_FRACTION_DIGITS:
      setMinimumFractionDigits(newValue);
      setMaximumFractionDigits(newValue);
      break;

    case UNUM_SIGNIFICANT_DIGITS_USED:
      setSignificantDigitsUsed(newValue!=0);
        break;

    case UNUM_MAX_SIGNIFICANT_DIGITS:
      setMaximumSignificantDigits(newValue);
        break;

    case UNUM_MIN_SIGNIFICANT_DIGITS:
      setMinimumSignificantDigits(newValue);
        break;

    case UNUM_MULTIPLIER:
      setMultiplier(newValue);
       break;

    case UNUM_GROUPING_SIZE:
      setGroupingSize(newValue);
        break;

    case UNUM_ROUNDING_MODE:
      setRoundingMode((DecimalFormat::ERoundingMode)newValue);
        break;

    case UNUM_FORMAT_WIDTH:
      setFormatWidth(newValue);
        break;

    case UNUM_PADDING_POSITION:
        /** The position at which padding will take place. */
      setPadPosition((DecimalFormat::EPadPosition)newValue);
        break;

    case UNUM_SECONDARY_GROUPING_SIZE:
      setSecondaryGroupingSize(newValue);
        break;

#if UCONFIG_HAVE_PARSEALLINPUT
    case UNUM_PARSE_ALL_INPUT:
      setParseAllInput((UNumberFormatAttributeValue)newValue);
        break;
#endif

    /* These are stored in fBoolFlags */
    case UNUM_PARSE_NO_EXPONENT:
    case UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS:
    case UNUM_PARSE_DECIMAL_MARK_REQUIRED:
      if(!fBoolFlags.isValidValue(newValue)) {
          status = U_ILLEGAL_ARGUMENT_ERROR;
      } else {
          if (attr == UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS) {
              fImpl->setFailIfMoreThanMaxDigits((UBool) newValue);
          }
          fBoolFlags.set(attr, newValue);
      }
      break;

    case UNUM_SCALE:
        fImpl->setScale(newValue);
        break;

    case UNUM_CURRENCY_USAGE:
        setCurrencyUsage((UCurrencyUsage)newValue, &status);
        break;

    case UNUM_MINIMUM_GROUPING_DIGITS:
        setMinimumGroupingDigits(newValue);
        break;

    default:
      status = U_UNSUPPORTED_ERROR;
      break;
  }
  return *this;
}

int32_t DecimalFormat::getAttribute( UNumberFormatAttribute attr,
                                     UErrorCode &status ) const {
  if(U_FAILURE(status)) return -1;
  switch(attr) {
    case UNUM_LENIENT_PARSE:
        return isLenient();

    case UNUM_PARSE_INT_ONLY:
        return isParseIntegerOnly();

    case UNUM_GROUPING_USED:
        return isGroupingUsed();

    case UNUM_DECIMAL_ALWAYS_SHOWN:
        return isDecimalSeparatorAlwaysShown();

    case UNUM_MAX_INTEGER_DIGITS:
        return getMaximumIntegerDigits();

    case UNUM_MIN_INTEGER_DIGITS:
        return getMinimumIntegerDigits();

    case UNUM_INTEGER_DIGITS:
        // TBD: what should this return?
        return getMinimumIntegerDigits();

    case UNUM_MAX_FRACTION_DIGITS:
        return getMaximumFractionDigits();

    case UNUM_MIN_FRACTION_DIGITS:
        return getMinimumFractionDigits();

    case UNUM_FRACTION_DIGITS:
        // TBD: what should this return?
        return getMinimumFractionDigits();

    case UNUM_SIGNIFICANT_DIGITS_USED:
        return areSignificantDigitsUsed();

    case UNUM_MAX_SIGNIFICANT_DIGITS:
        return getMaximumSignificantDigits();

    case UNUM_MIN_SIGNIFICANT_DIGITS:
        return getMinimumSignificantDigits();

    case UNUM_MULTIPLIER:
        return getMultiplier();

    case UNUM_GROUPING_SIZE:
        return getGroupingSize();

    case UNUM_ROUNDING_MODE:
        return getRoundingMode();

    case UNUM_FORMAT_WIDTH:
        return getFormatWidth();

    case UNUM_PADDING_POSITION:
        return getPadPosition();

    case UNUM_SECONDARY_GROUPING_SIZE:
        return getSecondaryGroupingSize();

    /* These are stored in fBoolFlags */
    case UNUM_PARSE_NO_EXPONENT:
    case UNUM_FORMAT_FAIL_IF_MORE_THAN_MAX_DIGITS:
    case UNUM_PARSE_DECIMAL_MARK_REQUIRED:
      return fBoolFlags.get(attr);

    case UNUM_SCALE:
        return fImpl->fScale;

    case UNUM_CURRENCY_USAGE:
        return fImpl->getCurrencyUsage();

    case UNUM_MINIMUM_GROUPING_DIGITS:
        return getMinimumGroupingDigits();

    default:
        status = U_UNSUPPORTED_ERROR;
        break;
  }

  return -1; /* undefined */
}

#if UCONFIG_HAVE_PARSEALLINPUT
void DecimalFormat::setParseAllInput(UNumberFormatAttributeValue value) {
  fParseAllInput = value;
}
#endif

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
