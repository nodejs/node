/*
*******************************************************************************
* Copyright (C) 1997-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File COMPACTDECIMALFORMAT.CPP
*
********************************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "charstr.h"
#include "cstring.h"
#include "digitlst.h"
#include "mutex.h"
#include "unicode/compactdecimalformat.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/ures.h"
#include "ucln_in.h"
#include "uhash.h"
#include "umutex.h"
#include "unicode/ures.h"
#include "uresimp.h"

// Maps locale name to CDFLocaleData struct.
static UHashtable* gCompactDecimalData = NULL;
static UMutex gCompactDecimalMetaLock = U_MUTEX_INITIALIZER;

U_NAMESPACE_BEGIN

static const int32_t MAX_DIGITS = 15;
static const char gOther[] = "other";
static const char gLatnTag[] = "latn";
static const char gNumberElementsTag[] = "NumberElements";
static const char gDecimalFormatTag[] = "decimalFormat";
static const char gPatternsShort[] = "patternsShort";
static const char gPatternsLong[] = "patternsLong";
static const char gRoot[] = "root";

static const UChar u_0 = 0x30;
static const UChar u_apos = 0x27;

static const UChar kZero[] = {u_0};

// Used to unescape single quotes.
enum QuoteState {
  OUTSIDE,
  INSIDE_EMPTY,
  INSIDE_FULL
};

enum FallbackFlags {
  ANY = 0,
  MUST = 1,
  NOT_ROOT = 2
  // Next one will be 4 then 6 etc.
};


// CDFUnit represents a prefix-suffix pair for a particular variant
// and log10 value.
struct CDFUnit : public UMemory {
  UnicodeString prefix;
  UnicodeString suffix;
  inline CDFUnit() : prefix(), suffix() {
    prefix.setToBogus();
  }
  inline ~CDFUnit() {}
  inline UBool isSet() const {
    return !prefix.isBogus();
  }
  inline void markAsSet() {
    prefix.remove();
  }
};

// CDFLocaleStyleData contains formatting data for a particular locale
// and style.
class CDFLocaleStyleData : public UMemory {
 public:
  // What to divide by for each log10 value when formatting. These values
  // will be powers of 10. For English, would be:
  // 1, 1, 1, 1000, 1000, 1000, 1000000, 1000000, 1000000, 1000000000 ...
  double divisors[MAX_DIGITS];
  // Maps plural variants to CDFUnit[MAX_DIGITS] arrays.
  // To format a number x,
  // first compute log10(x). Compute displayNum = (x / divisors[log10(x)]).
  // Compute the plural variant for displayNum
  // (e.g zero, one, two, few, many, other).
  // Compute cdfUnits = unitsByVariant[pluralVariant].
  // Prefix and suffix to use at cdfUnits[log10(x)]
  UHashtable* unitsByVariant;
  inline CDFLocaleStyleData() : unitsByVariant(NULL) {}
  ~CDFLocaleStyleData();
  // Init initializes this object.
  void Init(UErrorCode& status);
  inline UBool isBogus() const {
    return unitsByVariant == NULL;
  }
  void setToBogus();
 private:
  CDFLocaleStyleData(const CDFLocaleStyleData&);
  CDFLocaleStyleData& operator=(const CDFLocaleStyleData&);
};

// CDFLocaleData contains formatting data for a particular locale.
struct CDFLocaleData : public UMemory {
  CDFLocaleStyleData shortData;
  CDFLocaleStyleData longData;
  inline CDFLocaleData() : shortData(), longData() { }
  inline ~CDFLocaleData() { }
  // Init initializes this object.
  void Init(UErrorCode& status);
};

U_NAMESPACE_END

U_CDECL_BEGIN

static UBool U_CALLCONV cdf_cleanup(void) {
  if (gCompactDecimalData != NULL) {
    uhash_close(gCompactDecimalData);
    gCompactDecimalData = NULL;
  }
  return TRUE;
}

static void U_CALLCONV deleteCDFUnits(void* ptr) {
  delete [] (icu::CDFUnit*) ptr;
}

static void U_CALLCONV deleteCDFLocaleData(void* ptr) {
  delete (icu::CDFLocaleData*) ptr;
}

U_CDECL_END

U_NAMESPACE_BEGIN

static UBool divisors_equal(const double* lhs, const double* rhs);
static const CDFLocaleStyleData* getCDFLocaleStyleData(const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status);

static const CDFLocaleStyleData* extractDataByStyleEnum(const CDFLocaleData& data, UNumberCompactStyle style, UErrorCode& status);
static CDFLocaleData* loadCDFLocaleData(const Locale& inLocale, UErrorCode& status);
static void initCDFLocaleData(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status);
static UResourceBundle* tryGetDecimalFallback(const UResourceBundle* numberSystemResource, const char* style, UResourceBundle** fillIn, FallbackFlags flags, UErrorCode& status);
static UResourceBundle* tryGetByKeyWithFallback(const UResourceBundle* rb, const char* path, UResourceBundle** fillIn, FallbackFlags flags, UErrorCode& status);
static UBool isRoot(const UResourceBundle* rb, UErrorCode& status);
static void initCDFLocaleStyleData(const UResourceBundle* decimalFormatBundle, CDFLocaleStyleData* result, UErrorCode& status);
static void populatePower10(const UResourceBundle* power10Bundle, CDFLocaleStyleData* result, UErrorCode& status);
static int32_t populatePrefixSuffix(const char* variant, int32_t log10Value, const UnicodeString& formatStr, UHashtable* result, UErrorCode& status);
static UBool onlySpaces(UnicodeString u);
static void fixQuotes(UnicodeString& s);
static void fillInMissing(CDFLocaleStyleData* result);
static int32_t computeLog10(double x, UBool inRange);
static CDFUnit* createCDFUnit(const char* variant, int32_t log10Value, UHashtable* table, UErrorCode& status);
static const CDFUnit* getCDFUnitFallback(const UHashtable* table, const UnicodeString& variant, int32_t log10Value);

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CompactDecimalFormat)

CompactDecimalFormat::CompactDecimalFormat(
    const DecimalFormat& decimalFormat,
    const UHashtable* unitsByVariant,
    const double* divisors,
    PluralRules* pluralRules)
  : DecimalFormat(decimalFormat), _unitsByVariant(unitsByVariant), _divisors(divisors), _pluralRules(pluralRules) {
}

CompactDecimalFormat::CompactDecimalFormat(const CompactDecimalFormat& source)
    : DecimalFormat(source), _unitsByVariant(source._unitsByVariant), _divisors(source._divisors), _pluralRules(source._pluralRules->clone()) {
}

CompactDecimalFormat* U_EXPORT2
CompactDecimalFormat::createInstance(
    const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status) {
  LocalPointer<DecimalFormat> decfmt((DecimalFormat*) NumberFormat::makeInstance(inLocale, UNUM_DECIMAL, TRUE, status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  LocalPointer<PluralRules> pluralRules(PluralRules::forLocale(inLocale, status));
  if (U_FAILURE(status)) {
    return NULL;
  }
  const CDFLocaleStyleData* data = getCDFLocaleStyleData(inLocale, style, status);
  if (U_FAILURE(status)) {
    return NULL;
  }
  CompactDecimalFormat* result =
      new CompactDecimalFormat(*decfmt, data->unitsByVariant, data->divisors, pluralRules.getAlias());
  if (result == NULL) {
    status = U_MEMORY_ALLOCATION_ERROR;
    return NULL;
  }
  pluralRules.orphan();
  result->setMaximumSignificantDigits(3);
  result->setSignificantDigitsUsed(TRUE);
  result->setGroupingUsed(FALSE);
  return result;
}

CompactDecimalFormat&
CompactDecimalFormat::operator=(const CompactDecimalFormat& rhs) {
  if (this != &rhs) {
    DecimalFormat::operator=(rhs);
    _unitsByVariant = rhs._unitsByVariant;
    _divisors = rhs._divisors;
    delete _pluralRules;
    _pluralRules = rhs._pluralRules->clone();
  }
  return *this;
}

CompactDecimalFormat::~CompactDecimalFormat() {
  delete _pluralRules;
}


Format*
CompactDecimalFormat::clone(void) const {
  return new CompactDecimalFormat(*this);
}

UBool
CompactDecimalFormat::operator==(const Format& that) const {
  if (this == &that) {
    return TRUE;
  }
  return (DecimalFormat::operator==(that) && eqHelper((const CompactDecimalFormat&) that));
}

UBool
CompactDecimalFormat::eqHelper(const CompactDecimalFormat& that) const {
  return uhash_equals(_unitsByVariant, that._unitsByVariant) && divisors_equal(_divisors, that._divisors) && (*_pluralRules == *that._pluralRules);
}

UnicodeString&
CompactDecimalFormat::format(
    double number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  UErrorCode status = U_ZERO_ERROR;
  return format(number, appendTo, pos, status);
}

UnicodeString&
CompactDecimalFormat::format(
    double number,
    UnicodeString& appendTo,
    FieldPosition& pos,
    UErrorCode &status) const {
  if (U_FAILURE(status)) {
    return appendTo;
  }
  DigitList orig, rounded;
  orig.set(number);
  UBool isNegative;
  _round(orig, rounded, isNegative, status);
  if (U_FAILURE(status)) {
    return appendTo;
  }
  double roundedDouble = rounded.getDouble();
  if (isNegative) {
    roundedDouble = -roundedDouble;
  }
  int32_t baseIdx = computeLog10(roundedDouble, TRUE);
  double numberToFormat = roundedDouble / _divisors[baseIdx];
  UnicodeString variant = _pluralRules->select(numberToFormat);
  if (isNegative) {
    numberToFormat = -numberToFormat;
  }
  const CDFUnit* unit = getCDFUnitFallback(_unitsByVariant, variant, baseIdx);
  appendTo += unit->prefix;
  DecimalFormat::format(numberToFormat, appendTo, pos);
  appendTo += unit->suffix;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    double /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    int32_t number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  return format((double) number, appendTo, pos);
}

UnicodeString&
CompactDecimalFormat::format(
    int32_t number,
    UnicodeString& appendTo,
    FieldPosition& pos,
    UErrorCode &status) const {
  return format((double) number, appendTo, pos, status);
}

UnicodeString&
CompactDecimalFormat::format(
    int32_t /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    int64_t number,
    UnicodeString& appendTo,
    FieldPosition& pos) const {
  return format((double) number, appendTo, pos);
}

UnicodeString&
CompactDecimalFormat::format(
    int64_t number,
    UnicodeString& appendTo,
    FieldPosition& pos,
    UErrorCode &status) const {
  return format((double) number, appendTo, pos, status);
}

UnicodeString&
CompactDecimalFormat::format(
    int64_t /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const StringPiece& /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(
    const DigitList& /* number */,
    UnicodeString& appendTo,
    FieldPositionIterator* /* posIter */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

UnicodeString&
CompactDecimalFormat::format(const DigitList& /* number */,
                             UnicodeString& appendTo,
                             FieldPosition& /* pos */,
                             UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
  return appendTo;
}

void
CompactDecimalFormat::parse(
    const UnicodeString& /* text */,
    Formattable& /* result */,
    ParsePosition& /* parsePosition */) const {
}

void
CompactDecimalFormat::parse(
    const UnicodeString& /* text */,
    Formattable& /* result */,
    UErrorCode& status) const {
  status = U_UNSUPPORTED_ERROR;
}

CurrencyAmount*
CompactDecimalFormat::parseCurrency(
    const UnicodeString& /* text */,
    ParsePosition& /* pos */) const {
  return NULL;
}

void CDFLocaleStyleData::Init(UErrorCode& status) {
  if (unitsByVariant != NULL) {
    return;
  }
  unitsByVariant = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
  if (U_FAILURE(status)) {
    return;
  }
  uhash_setKeyDeleter(unitsByVariant, uprv_free);
  uhash_setValueDeleter(unitsByVariant, deleteCDFUnits);
}

CDFLocaleStyleData::~CDFLocaleStyleData() {
  setToBogus();
}

void CDFLocaleStyleData::setToBogus() {
  if (unitsByVariant != NULL) {
    uhash_close(unitsByVariant);
    unitsByVariant = NULL;
  }
}

void CDFLocaleData::Init(UErrorCode& status) {
  shortData.Init(status);
  if (U_FAILURE(status)) {
    return;
  }
  longData.Init(status);
}

// Helper method for operator=
static UBool divisors_equal(const double* lhs, const double* rhs) {
  for (int32_t i = 0; i < MAX_DIGITS; ++i) {
    if (lhs[i] != rhs[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

// getCDFLocaleStyleData returns pointer to formatting data for given locale and
// style within the global cache. On cache miss, getCDFLocaleStyleData loads
// the data from CLDR into the global cache before returning the pointer. If a
// UNUM_LONG data is requested for a locale, and that locale does not have
// UNUM_LONG data, getCDFLocaleStyleData will fall back to UNUM_SHORT data for
// that locale.
static const CDFLocaleStyleData* getCDFLocaleStyleData(const Locale& inLocale, UNumberCompactStyle style, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  CDFLocaleData* result = NULL;
  const char* key = inLocale.getName();
  {
    Mutex lock(&gCompactDecimalMetaLock);
    if (gCompactDecimalData == NULL) {
      gCompactDecimalData = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &status);
      if (U_FAILURE(status)) {
        return NULL;
      }
      uhash_setKeyDeleter(gCompactDecimalData, uprv_free);
      uhash_setValueDeleter(gCompactDecimalData, deleteCDFLocaleData);
      ucln_i18n_registerCleanup(UCLN_I18N_CDFINFO, cdf_cleanup);
    } else {
      result = (CDFLocaleData*) uhash_get(gCompactDecimalData, key);
    }
  }
  if (result != NULL) {
    return extractDataByStyleEnum(*result, style, status);
  }

  result = loadCDFLocaleData(inLocale, status);
  if (U_FAILURE(status)) {
    return NULL;
  }

  {
    Mutex lock(&gCompactDecimalMetaLock);
    CDFLocaleData* temp = (CDFLocaleData*) uhash_get(gCompactDecimalData, key);
    if (temp != NULL) {
      delete result;
      result = temp;
    } else {
      uhash_put(gCompactDecimalData, uprv_strdup(key), (void*) result, &status);
      if (U_FAILURE(status)) {
        return NULL;
      }
    }
  }
  return extractDataByStyleEnum(*result, style, status);
}

static const CDFLocaleStyleData* extractDataByStyleEnum(const CDFLocaleData& data, UNumberCompactStyle style, UErrorCode& status) {
  switch (style) {
    case UNUM_SHORT:
      return &data.shortData;
    case UNUM_LONG:
      if (!data.longData.isBogus()) {
        return &data.longData;
      }
      return &data.shortData;
    default:
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return NULL;
  }
}

// loadCDFLocaleData loads formatting data from CLDR for a given locale. The
// caller owns the returned pointer.
static CDFLocaleData* loadCDFLocaleData(const Locale& inLocale, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  CDFLocaleData* result = new CDFLocaleData;
  if (result == NULL) {
    status = U_MEMORY_ALLOCATION_ERROR;
    return NULL;
  }
  result->Init(status);
  if (U_FAILURE(status)) {
    delete result;
    return NULL;
  }

  initCDFLocaleData(inLocale, result, status);
  if (U_FAILURE(status)) {
    delete result;
    return NULL;
  }
  return result;
}

// initCDFLocaleData initializes result with data from CLDR.
// inLocale is the locale, the CLDR data is stored in result.
// We load the UNUM_SHORT  and UNUM_LONG data looking first in local numbering
// system and not including root locale in fallback. Next we try in the latn
// numbering system where we fallback all the way to root. If we don't find
// UNUM_SHORT data in these three places, we report an error. If we find
// UNUM_SHORT data before finding UNUM_LONG data we make UNUM_LONG data fall
// back to UNUM_SHORT data.
static void initCDFLocaleData(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status) {
  LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(inLocale, status));
  if (U_FAILURE(status)) {
    return;
  }
  const char* numberingSystemName = ns->getName();
  UResourceBundle* rb = ures_open(NULL, inLocale.getName(), &status);
  rb = ures_getByKeyWithFallback(rb, gNumberElementsTag, rb, &status);
  if (U_FAILURE(status)) {
    ures_close(rb);
    return;
  }
  UResourceBundle* shortDataFillIn = NULL;
  UResourceBundle* longDataFillIn = NULL;
  UResourceBundle* shortData = NULL;
  UResourceBundle* longData = NULL;

  if (uprv_strcmp(numberingSystemName, gLatnTag) != 0) {
    LocalUResourceBundlePointer localResource(
        tryGetByKeyWithFallback(rb, numberingSystemName, NULL, NOT_ROOT, status));
    shortData = tryGetDecimalFallback(
        localResource.getAlias(), gPatternsShort, &shortDataFillIn, NOT_ROOT, status);
    longData = tryGetDecimalFallback(
        localResource.getAlias(), gPatternsLong, &longDataFillIn, NOT_ROOT, status);
  }
  if (U_FAILURE(status)) {
    ures_close(shortDataFillIn);
    ures_close(longDataFillIn);
    ures_close(rb);
    return;
  }

  // If we haven't found UNUM_SHORT look in latn numbering system. We must
  // succeed at finding UNUM_SHORT here.
  if (shortData == NULL) {
    LocalUResourceBundlePointer latnResource(tryGetByKeyWithFallback(rb, gLatnTag, NULL, MUST, status));
    shortData = tryGetDecimalFallback(latnResource.getAlias(), gPatternsShort, &shortDataFillIn, MUST, status);
    if (longData == NULL) {
      longData = tryGetDecimalFallback(latnResource.getAlias(), gPatternsLong, &longDataFillIn, ANY, status);
      if (longData != NULL && isRoot(longData, status) && !isRoot(shortData, status)) {
        longData = NULL;
      }
    }
  }
  initCDFLocaleStyleData(shortData, &result->shortData, status);
  ures_close(shortDataFillIn);
  if (U_FAILURE(status)) {
    ures_close(longDataFillIn);
    ures_close(rb);
  }

  if (longData == NULL) {
    result->longData.setToBogus();
  } else {
    initCDFLocaleStyleData(longData, &result->longData, status);
  }
  ures_close(longDataFillIn);
  ures_close(rb);
}

/**
 * tryGetDecimalFallback attempts to fetch the "decimalFormat" resource bundle
 * with a particular style. style is either "patternsShort" or "patternsLong."
 * FillIn, flags, and status work in the same way as in tryGetByKeyWithFallback.
 */
static UResourceBundle* tryGetDecimalFallback(const UResourceBundle* numberSystemResource, const char* style, UResourceBundle** fillIn, FallbackFlags flags, UErrorCode& status) {
  UResourceBundle* first = tryGetByKeyWithFallback(numberSystemResource, style, fillIn, flags, status);
  UResourceBundle* second = tryGetByKeyWithFallback(first, gDecimalFormatTag, fillIn, flags, status);
  if (fillIn == NULL) {
    ures_close(first);
  }
  return second;
}

// tryGetByKeyWithFallback returns a sub-resource bundle that matches given
// criteria or NULL if none found. rb is the resource bundle that we are
// searching. If rb == NULL then this function behaves as if no sub-resource
// is found; path is the key of the sub-resource,
// (i.e "foo" but not "foo/bar"); If fillIn is NULL, caller must always call
// ures_close() on returned resource. See below for example when fillIn is
// not NULL. flags is ANY or NOT_ROOT. Optionally, these values
// can be ored with MUST. MUST by itself is the same as ANY | MUST.
// The locale of the returned sub-resource will either match the
// flags or the returned sub-resouce will be NULL. If MUST is included in
// flags, and not suitable sub-resource is found then in addition to returning
// NULL, this function also sets status to U_MISSING_RESOURCE_ERROR. If MUST
// is not included in flags, then this function just returns NULL if no
// such sub-resource is found and will never set status to
// U_MISSING_RESOURCE_ERROR.
//
// Example: This code first searches for "foo/bar" sub-resource without falling
// back to ROOT. Then searches for "baz" sub-resource as last resort.
//
// UResourcebundle* fillIn = NULL;
// UResourceBundle* data = tryGetByKeyWithFallback(rb, "foo", &fillIn, NON_ROOT, status);
// data = tryGetByKeyWithFallback(data, "bar", &fillIn, NON_ROOT, status);
// if (!data) {
//   data = tryGetbyKeyWithFallback(rb, "baz", &fillIn, MUST,  status);
// }
// if (U_FAILURE(status)) {
//   ures_close(fillIn);
//   return;
// }
// doStuffWithNonNullSubresource(data);
//
// /* Wrong! don't do the following as it can leak memory if fillIn gets set
// to NULL. */
// fillIn = tryGetByKeyWithFallback(rb, "wrong", &fillIn, ANY, status);
//
// ures_close(fillIn);
//
static UResourceBundle* tryGetByKeyWithFallback(const UResourceBundle* rb, const char* path, UResourceBundle** fillIn, FallbackFlags flags, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  UBool must = (flags & MUST);
  if (rb == NULL) {
    if (must) {
      status = U_MISSING_RESOURCE_ERROR;
    }
    return NULL;
  }
  UResourceBundle* result = NULL;
  UResourceBundle* ownedByUs = NULL;
  if (fillIn == NULL) {
    ownedByUs = ures_getByKeyWithFallback(rb, path, NULL, &status);
    result = ownedByUs;
  } else {
    *fillIn = ures_getByKeyWithFallback(rb, path, *fillIn, &status);
    result = *fillIn;
  }
  if (U_FAILURE(status)) {
    ures_close(ownedByUs);
    if (status == U_MISSING_RESOURCE_ERROR && !must) {
      status = U_ZERO_ERROR;
    }
    return NULL;
  }
  flags = (FallbackFlags) (flags & ~MUST);
  switch (flags) {
    case NOT_ROOT:
      {
        UBool bRoot = isRoot(result, status);
        if (bRoot || U_FAILURE(status)) {
          ures_close(ownedByUs);
          if (must && (status == U_ZERO_ERROR)) {
            status = U_MISSING_RESOURCE_ERROR;
          }
          return NULL;
        }
        return result;
      }
    case ANY:
      return result;
    default:
      ures_close(ownedByUs);
      status = U_ILLEGAL_ARGUMENT_ERROR;
      return NULL;
  }
}

static UBool isRoot(const UResourceBundle* rb, UErrorCode& status) {
  const char* actualLocale = ures_getLocaleByType(
      rb, ULOC_ACTUAL_LOCALE, &status);
  if (U_FAILURE(status)) {
    return FALSE;
  }
  return uprv_strcmp(actualLocale, gRoot) == 0;
}


// initCDFLocaleStyleData loads formatting data for a particular style.
// decimalFormatBundle is the "decimalFormat" resource bundle in CLDR.
// Loaded data stored in result.
static void initCDFLocaleStyleData(const UResourceBundle* decimalFormatBundle, CDFLocaleStyleData* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return;
  }
  // Iterate through all the powers of 10.
  int32_t size = ures_getSize(decimalFormatBundle);
  UResourceBundle* power10 = NULL;
  for (int32_t i = 0; i < size; ++i) {
    power10 = ures_getByIndex(decimalFormatBundle, i, power10, &status);
    if (U_FAILURE(status)) {
      ures_close(power10);
      return;
    }
    populatePower10(power10, result, status);
    if (U_FAILURE(status)) {
      ures_close(power10);
      return;
    }
  }
  ures_close(power10);
  fillInMissing(result);
}

// populatePower10 grabs data for a particular power of 10 from CLDR.
// The loaded data is stored in result.
static void populatePower10(const UResourceBundle* power10Bundle, CDFLocaleStyleData* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return;
  }
  char* endPtr = NULL;
  double power10 = uprv_strtod(ures_getKey(power10Bundle), &endPtr);
  if (*endPtr != 0) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return;
  }
  int32_t log10Value = computeLog10(power10, FALSE);
  // Silently ignore divisors that are too big.
  if (log10Value == MAX_DIGITS) {
    return;
  }
  int32_t size = ures_getSize(power10Bundle);
  int32_t numZeros = 0;
  UBool otherVariantDefined = FALSE;
  UResourceBundle* variantBundle = NULL;
  // Iterate over all the plural variants for the power of 10
  for (int32_t i = 0; i < size; ++i) {
    variantBundle = ures_getByIndex(power10Bundle, i, variantBundle, &status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    const char* variant = ures_getKey(variantBundle);
    int32_t resLen;
    const UChar* formatStrP = ures_getString(variantBundle, &resLen, &status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    UnicodeString formatStr(false, formatStrP, resLen);
    if (uprv_strcmp(variant, gOther) == 0) {
      otherVariantDefined = TRUE;
    }
    int32_t nz = populatePrefixSuffix(
        variant, log10Value, formatStr, result->unitsByVariant, status);
    if (U_FAILURE(status)) {
      ures_close(variantBundle);
      return;
    }
    if (nz != numZeros) {
      // We expect all format strings to have the same number of 0's
      // left of the decimal point.
      if (numZeros != 0) {
        status = U_INTERNAL_PROGRAM_ERROR;
        ures_close(variantBundle);
        return;
      }
      numZeros = nz;
    }
  }
  ures_close(variantBundle);
  // We expect to find an OTHER variant for each power of 10.
  if (!otherVariantDefined) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return;
  }
  double divisor = power10;
  for (int32_t i = 1; i < numZeros; ++i) {
    divisor /= 10.0;
  }
  result->divisors[log10Value] = divisor;
}

// populatePrefixSuffix Adds a specific prefix-suffix pair to result for a
// given variant and log10 value.
// variant is 'zero', 'one', 'two', 'few', 'many', or 'other'.
// formatStr is the format string from which the prefix and suffix are
// extracted. It is usually of form 'Pefix 000 suffix'.
// populatePrefixSuffix returns the number of 0's found in formatStr
// before the decimal point.
// In the special case that formatStr contains only spaces for prefix
// and suffix, populatePrefixSuffix returns log10Value + 1.
static int32_t populatePrefixSuffix(
    const char* variant, int32_t log10Value, const UnicodeString& formatStr, UHashtable* result, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return 0;
  }
  int32_t firstIdx = formatStr.indexOf(kZero, UPRV_LENGTHOF(kZero), 0);
  // We must have 0's in format string.
  if (firstIdx == -1) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return 0;
  }
  int32_t lastIdx = formatStr.lastIndexOf(kZero, UPRV_LENGTHOF(kZero), firstIdx);
  CDFUnit* unit = createCDFUnit(variant, log10Value, result, status);
  if (U_FAILURE(status)) {
    return 0;
  }
  // Everything up to first 0 is the prefix
  unit->prefix = formatStr.tempSubString(0, firstIdx);
  fixQuotes(unit->prefix);
  // Everything beyond the last 0 is the suffix
  unit->suffix = formatStr.tempSubString(lastIdx + 1);
  fixQuotes(unit->suffix);

  // If there is effectively no prefix or suffix, ignore the actual number of
  // 0's and act as if the number of 0's matches the size of the number.
  if (onlySpaces(unit->prefix) && onlySpaces(unit->suffix)) {
    return log10Value + 1;
  }

  // Calculate number of zeros before decimal point
  int32_t idx = firstIdx + 1;
  while (idx <= lastIdx && formatStr.charAt(idx) == u_0) {
    ++idx;
  }
  return (idx - firstIdx);
}

static UBool onlySpaces(UnicodeString u) {
  return u.trim().length() == 0;
}

// fixQuotes unescapes single quotes. Don''t -> Don't. Letter 'j' -> Letter j.
// Modifies s in place.
static void fixQuotes(UnicodeString& s) {
  QuoteState state = OUTSIDE;
  int32_t len = s.length();
  int32_t dest = 0;
  for (int32_t i = 0; i < len; ++i) {
    UChar ch = s.charAt(i);
    if (ch == u_apos) {
      if (state == INSIDE_EMPTY) {
        s.setCharAt(dest, ch);
        ++dest;
      }
    } else {
      s.setCharAt(dest, ch);
      ++dest;
    }

    // Update state
    switch (state) {
      case OUTSIDE:
        state = ch == u_apos ? INSIDE_EMPTY : OUTSIDE;
        break;
      case INSIDE_EMPTY:
      case INSIDE_FULL:
        state = ch == u_apos ? OUTSIDE : INSIDE_FULL;
        break;
      default:
        break;
    }
  }
  s.truncate(dest);
}

// fillInMissing ensures that the data in result is complete.
// result data is complete if for each variant in result, there exists
// a prefix-suffix pair for each log10 value and there also exists
// a divisor for each log10 value.
//
// First this function figures out for which log10 values, the other
// variant already had data. These are the same log10 values defined
// in CLDR.
//
// For each log10 value not defined in CLDR, it uses the divisor for
// the last defined log10 value or 1.
//
// Then for each variant, it does the following. For each log10
// value not defined in CLDR, copy the prefix-suffix pair from the
// previous log10 value. If log10 value is defined in CLDR but is
// missing from given variant, copy the prefix-suffix pair for that
// log10 value from the 'other' variant.
static void fillInMissing(CDFLocaleStyleData* result) {
  const CDFUnit* otherUnits =
      (const CDFUnit*) uhash_get(result->unitsByVariant, gOther);
  UBool definedInCLDR[MAX_DIGITS];
  double lastDivisor = 1.0;
  for (int32_t i = 0; i < MAX_DIGITS; ++i) {
    if (!otherUnits[i].isSet()) {
      result->divisors[i] = lastDivisor;
      definedInCLDR[i] = FALSE;
    } else {
      lastDivisor = result->divisors[i];
      definedInCLDR[i] = TRUE;
    }
  }
  // Iterate over each variant.
  int32_t pos = UHASH_FIRST;
  const UHashElement* element = uhash_nextElement(result->unitsByVariant, &pos);
  for (;element != NULL; element = uhash_nextElement(result->unitsByVariant, &pos)) {
    CDFUnit* units = (CDFUnit*) element->value.pointer;
    for (int32_t i = 0; i < MAX_DIGITS; ++i) {
      if (definedInCLDR[i]) {
        if (!units[i].isSet()) {
          units[i] = otherUnits[i];
        }
      } else {
        if (i == 0) {
          units[0].markAsSet();
        } else {
          units[i] = units[i - 1];
        }
      }
    }
  }
}

// computeLog10 computes floor(log10(x)). If inRange is TRUE, the biggest
// value computeLog10 will return MAX_DIGITS -1 even for
// numbers > 10^MAX_DIGITS. If inRange is FALSE, computeLog10 will return
// up to MAX_DIGITS.
static int32_t computeLog10(double x, UBool inRange) {
  int32_t result = 0;
  int32_t max = inRange ? MAX_DIGITS - 1 : MAX_DIGITS;
  while (x >= 10.0) {
    x /= 10.0;
    ++result;
    if (result == max) {
      break;
    }
  }
  return result;
}

// createCDFUnit returns a pointer to the prefix-suffix pair for a given
// variant and log10 value within table. If no such prefix-suffix pair is
// stored in table, one is created within table before returning pointer.
static CDFUnit* createCDFUnit(const char* variant, int32_t log10Value, UHashtable* table, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return NULL;
  }
  CDFUnit *cdfUnit = (CDFUnit*) uhash_get(table, variant);
  if (cdfUnit == NULL) {
    cdfUnit = new CDFUnit[MAX_DIGITS];
    if (cdfUnit == NULL) {
      status = U_MEMORY_ALLOCATION_ERROR;
      return NULL;
    }
    uhash_put(table, uprv_strdup(variant), cdfUnit, &status);
    if (U_FAILURE(status)) {
      return NULL;
    }
  }
  CDFUnit* result = &cdfUnit[log10Value];
  result->markAsSet();
  return result;
}

// getCDFUnitFallback returns a pointer to the prefix-suffix pair for a given
// variant and log10 value within table. If the given variant doesn't exist, it
// falls back to the OTHER variant. Therefore, this method will always return
// some non-NULL value.
static const CDFUnit* getCDFUnitFallback(const UHashtable* table, const UnicodeString& variant, int32_t log10Value) {
  CharString cvariant;
  UErrorCode status = U_ZERO_ERROR;
  const CDFUnit *cdfUnit = NULL;
  cvariant.appendInvariantChars(variant, status);
  if (!U_FAILURE(status)) {
    cdfUnit = (const CDFUnit*) uhash_get(table, cvariant.data());
  }
  if (cdfUnit == NULL) {
    cdfUnit = (const CDFUnit*) uhash_get(table, gOther);
  }
  return &cdfUnit[log10Value];
}

U_NAMESPACE_END
#endif
