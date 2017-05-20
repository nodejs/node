// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
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
static const char gLatnPath[] = "NumberElements/latn";

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
  // A flag for whether or not this CDFLocaleStyleData was loaded from the
  // Latin numbering system as a fallback from the locale numbering system.
  // This value is meaningless if the object is bogus or empty.
  UBool fromFallback;
  inline CDFLocaleStyleData() : unitsByVariant(NULL), fromFallback(FALSE) {
    uprv_memset(divisors, 0, sizeof(divisors));
  }
  ~CDFLocaleStyleData();
  // Init initializes this object.
  void Init(UErrorCode& status);
  inline UBool isBogus() const {
    return unitsByVariant == NULL;
  }
  void setToBogus();
  UBool isEmpty() {
    return unitsByVariant == NULL || unitsByVariant->count == 0;
  }
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
static void load(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status);
static int32_t populatePrefixSuffix(const char* variant, int32_t log10Value, const UnicodeString& formatStr, UHashtable* result, UBool overwrite, UErrorCode& status);
static double calculateDivisor(double power10, int32_t numZeros);
static UBool onlySpaces(UnicodeString u);
static void fixQuotes(UnicodeString& s);
static void checkForOtherVariants(CDFLocaleStyleData* result, UErrorCode& status);
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
    StringPiece /* number */,
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

  load(inLocale, result, status);

  if (U_FAILURE(status)) {
    delete result;
    return NULL;
  }
  return result;
}

namespace {

struct CmptDecDataSink : public ResourceSink {

  CDFLocaleData& dataBundle; // Where to save values when they are read
  UBool isLatin; // Whether or not we are traversing the Latin tree
  UBool isFallback; // Whether or not we are traversing the Latin tree as fallback

  enum EPatternsTableKey { PATTERNS_SHORT, PATTERNS_LONG };
  enum EFormatsTableKey { DECIMAL_FORMAT, CURRENCY_FORMAT };

  /*
   * NumberElements{              <-- top (numbering system table)
   *  latn{                       <-- patternsTable (one per numbering system)
   *    patternsLong{             <-- formatsTable (one per pattern)
   *      decimalFormat{          <-- powersOfTenTable (one per format)
   *        1000{                 <-- pluralVariantsTable (one per power of ten)
   *          one{"0 thousand"}   <-- plural variant and template
   */

  CmptDecDataSink(CDFLocaleData& _dataBundle)
    : dataBundle(_dataBundle), isLatin(FALSE), isFallback(FALSE) {}
  virtual ~CmptDecDataSink();

  virtual void put(const char *key, ResourceValue &value, UBool isRoot, UErrorCode &errorCode) {
    // SPECIAL CASE: Don't consume root in the non-Latin numbering system
    if (isRoot && !isLatin) { return; }

    ResourceTable patternsTable = value.getTable(errorCode);
    if (U_FAILURE(errorCode)) { return; }
    for (int i1 = 0; patternsTable.getKeyAndValue(i1, key, value); ++i1) {

      // Check for patternsShort or patternsLong
      EPatternsTableKey patternsTableKey;
      if (uprv_strcmp(key, gPatternsShort) == 0) {
        patternsTableKey = PATTERNS_SHORT;
      } else if (uprv_strcmp(key, gPatternsLong) == 0) {
        patternsTableKey = PATTERNS_LONG;
      } else {
        continue;
      }

      // Traverse into the formats table
      ResourceTable formatsTable = value.getTable(errorCode);
      if (U_FAILURE(errorCode)) { return; }
      for (int i2 = 0; formatsTable.getKeyAndValue(i2, key, value); ++i2) {

        // Check for decimalFormat or currencyFormat
        EFormatsTableKey formatsTableKey;
        if (uprv_strcmp(key, gDecimalFormatTag) == 0) {
          formatsTableKey = DECIMAL_FORMAT;
        // TODO: Enable this statement when currency support is added
        // } else if (uprv_strcmp(key, gCurrencyFormat) == 0) {
        //   formatsTableKey = CURRENCY_FORMAT;
        } else {
          continue;
        }

        // Set the current style and destination based on the two keys
        UNumberCompactStyle style;
        CDFLocaleStyleData* destination = NULL;
        if (patternsTableKey == PATTERNS_LONG
            && formatsTableKey == DECIMAL_FORMAT) {
          style = UNUM_LONG;
          destination = &dataBundle.longData;
        } else if (patternsTableKey == PATTERNS_SHORT
            && formatsTableKey == DECIMAL_FORMAT) {
          style = UNUM_SHORT;
          destination = &dataBundle.shortData;
        // TODO: Enable the following statements when currency support is added
        // } else if (patternsTableKey == PATTERNS_SHORT
        //     && formatsTableKey == CURRENCY_FORMAT) {
        //   style = UNUM_SHORT_CURRENCY; // or whatever the enum gets named
        //   destination = &dataBundle.shortCurrencyData;
        // } else {
        //   // Silently ignore this case
        //   continue;
        }

        // SPECIAL CASE: RULES FOR WHETHER OR NOT TO CONSUME THIS TABLE:
        //   1) Don't consume longData if shortData was consumed from the non-Latin
        //      locale numbering system
        //   2) Don't consume longData for the first time if this is the root bundle and
        //      shortData is already populated from a more specific locale. Note that if
        //      both longData and shortData are both only in root, longData will be
        //      consumed since it is alphabetically before shortData in the bundle.
        if (isFallback
                && style == UNUM_LONG
                && !dataBundle.shortData.isEmpty()
                && !dataBundle.shortData.fromFallback) {
            continue;
        }
        if (isRoot
                && style == UNUM_LONG
                && dataBundle.longData.isEmpty()
                && !dataBundle.shortData.isEmpty()) {
            continue;
        }

        // Set the "fromFallback" flag on the data object
        destination->fromFallback = isFallback;

        // Traverse into the powers of ten table
        ResourceTable powersOfTenTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int i3 = 0; powersOfTenTable.getKeyAndValue(i3, key, value); ++i3) {

          // The key will always be some even power of 10. e.g 10000.
          char* endPtr = NULL;
          double power10 = uprv_strtod(key, &endPtr);
          if (*endPtr != 0) {
            errorCode = U_INTERNAL_PROGRAM_ERROR;
            return;
          }
          int32_t log10Value = computeLog10(power10, FALSE);

          // Silently ignore divisors that are too big.
          if (log10Value >= MAX_DIGITS) continue;

          // Iterate over the plural variants ("one", "other", etc)
          ResourceTable pluralVariantsTable = value.getTable(errorCode);
          if (U_FAILURE(errorCode)) { return; }
          for (int i4 = 0; pluralVariantsTable.getKeyAndValue(i4, key, value); ++i4) {
            const char* pluralVariant = key;
            const UnicodeString formatStr = value.getUnicodeString(errorCode);

            // Copy the data into the in-memory data bundle (do not overwrite
            // existing values)
            int32_t numZeros = populatePrefixSuffix(
                pluralVariant, log10Value, formatStr,
                destination->unitsByVariant, FALSE, errorCode);

            // If populatePrefixSuffix returns -1, it means that this key has been
            // encountered already.
            if (numZeros < 0) {
              continue;
            }

            // Set the divisor, which is based on the number of zeros in the template
            // string.  If the divisor from here is different from the one previously
            // stored, it means that the number of zeros in different plural variants
            // differs; throw an exception.
            // TODO: How should I check for floating-point errors here?
            //       Is there a good reason why "divisor" is double and not long like Java?
            double divisor = calculateDivisor(power10, numZeros);
            if (destination->divisors[log10Value] != 0.0
                && destination->divisors[log10Value] != divisor) {
              errorCode = U_INTERNAL_PROGRAM_ERROR;
              return;
            }
            destination->divisors[log10Value] = divisor;
          }
        }
      }
    }
  }
};

// Virtual destructors must be defined out of line.
CmptDecDataSink::~CmptDecDataSink() {}

} // namespace

static void load(const Locale& inLocale, CDFLocaleData* result, UErrorCode& status) {
  LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(inLocale, status));
  if (U_FAILURE(status)) {
    return;
  }
  const char* nsName = ns->getName();

  LocalUResourceBundlePointer resource(ures_open(NULL, inLocale.getName(), &status));
  if (U_FAILURE(status)) {
    return;
  }
  CmptDecDataSink sink(*result);
  sink.isFallback = FALSE;

  // First load the number elements data if nsName is not Latin.
  if (uprv_strcmp(nsName, gLatnTag) != 0) {
    sink.isLatin = FALSE;
    CharString path;
    path.append(gNumberElementsTag, status)
        .append('/', status)
        .append(nsName, status);
    ures_getAllItemsWithFallback(resource.getAlias(), path.data(), sink, status);
    if (status == U_MISSING_RESOURCE_ERROR) {
      // Silently ignore and use Latin
      status = U_ZERO_ERROR;
    } else if  (U_FAILURE(status)) {
      return;
    }
    sink.isFallback = TRUE;
  }

  // Now load Latin.
  sink.isLatin = TRUE;
  ures_getAllItemsWithFallback(resource.getAlias(), gLatnPath, sink, status);
  if (U_FAILURE(status)) return;

  // If longData is empty, default it to be equal to shortData
  if (result->longData.isEmpty()) {
    result->longData.setToBogus();
  }

  // Check for "other" variants in each of the three data classes, and resolve missing elements.

  if (!result->longData.isBogus()) {
    checkForOtherVariants(&result->longData, status);
    if (U_FAILURE(status)) return;
    fillInMissing(&result->longData);
  }

  checkForOtherVariants(&result->shortData, status);
  if (U_FAILURE(status)) return;
  fillInMissing(&result->shortData);

  // TODO: Enable this statement when currency support is added
  // checkForOtherVariants(&result->shortCurrencyData, status);
  // if (U_FAILURE(status)) return;
  // fillInMissing(&result->shortCurrencyData);
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
    const char* variant, int32_t log10Value, const UnicodeString& formatStr, UHashtable* result, UBool overwrite, UErrorCode& status) {
  if (U_FAILURE(status)) {
    return 0;
  }

  // ICU 59 HACK: Ignore negative part of format string, mimicking ICU 58 behavior.
  // TODO(sffc): Make sure this is fixed during the overhaul port in ICU 60.
  int32_t semiPos = formatStr.indexOf(';', 0);
  if (semiPos == -1) {
    semiPos = formatStr.length();
  }
  UnicodeString positivePart = formatStr.tempSubString(0, semiPos);

  int32_t firstIdx = positivePart.indexOf(kZero, UPRV_LENGTHOF(kZero), 0);
  // We must have 0's in format string.
  if (firstIdx == -1) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return 0;
  }
  int32_t lastIdx = positivePart.lastIndexOf(kZero, UPRV_LENGTHOF(kZero), firstIdx);
  CDFUnit* unit = createCDFUnit(variant, log10Value, result, status);
  if (U_FAILURE(status)) {
    return 0;
  }

  // Return -1 if we are not overwriting an existing value
  if (unit->isSet() && !overwrite) {
    return -1;
  }
  unit->markAsSet();

  // Everything up to first 0 is the prefix
  unit->prefix = positivePart.tempSubString(0, firstIdx);
  fixQuotes(unit->prefix);
  // Everything beyond the last 0 is the suffix
  unit->suffix = positivePart.tempSubString(lastIdx + 1);
  fixQuotes(unit->suffix);

  // If there is effectively no prefix or suffix, ignore the actual number of
  // 0's and act as if the number of 0's matches the size of the number.
  if (onlySpaces(unit->prefix) && onlySpaces(unit->suffix)) {
    return log10Value + 1;
  }

  // Calculate number of zeros before decimal point
  int32_t idx = firstIdx + 1;
  while (idx <= lastIdx && positivePart.charAt(idx) == u_0) {
    ++idx;
  }
  return (idx - firstIdx);
}

// Calculate a divisor based on the magnitude and number of zeros in the
// template string.
static double calculateDivisor(double power10, int32_t numZeros) {
  double divisor = power10;
  for (int32_t i = 1; i < numZeros; ++i) {
    divisor /= 10.0;
  }
  return divisor;
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

// Checks to make sure that an "other" variant is present in all
// powers of 10.
static void checkForOtherVariants(CDFLocaleStyleData* result,
    UErrorCode& status) {
  if (result == NULL || result->unitsByVariant == NULL) {
    return;
  }

  const CDFUnit* otherByBase =
      (const CDFUnit*) uhash_get(result->unitsByVariant, gOther);
  if (otherByBase == NULL) {
    status = U_INTERNAL_PROGRAM_ERROR;
    return;
  }

  // Check all other plural variants, and make sure that if
  // any of them are populated, then other is also populated
  int32_t pos = UHASH_FIRST;
  const UHashElement* element;
  while ((element = uhash_nextElement(result->unitsByVariant, &pos)) != NULL) {
    CDFUnit* variantsByBase = (CDFUnit*) element->value.pointer;
    if (variantsByBase == otherByBase) continue;
    for (int32_t log10Value = 0; log10Value < MAX_DIGITS; ++log10Value) {
      if (variantsByBase[log10Value].isSet()
          && !otherByBase[log10Value].isSet()) {
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
      }
    }
  }
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
