// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTPTNGEN.CPP
*
*******************************************************************************
*/

#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

#include "unicode/datefmt.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/simpleformatter.h"
#include "unicode/smpdtfmt.h"
#include "unicode/udat.h"
#include "unicode/udatpg.h"
#include "unicode/uniset.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "unicode/rep.h"
#include "cpputils.h"
#include "mutex.h"
#include "umutex.h"
#include "cmemory.h"
#include "cstring.h"
#include "locbased.h"
#include "hash.h"
#include "uhash.h"
#include "uresimp.h"
#include "dtptngen_impl.h"
#include "ucln_in.h"
#include "charstr.h"
#include "uassert.h"

#if U_CHARSET_FAMILY==U_EBCDIC_FAMILY
/**
 * If we are on EBCDIC, use an iterator which will
 * traverse the bundles in ASCII order.
 */
#define U_USE_ASCII_BUNDLE_ITERATOR
#define U_SORT_ASCII_BUNDLE_ITERATOR
#endif

#if defined(U_USE_ASCII_BUNDLE_ITERATOR)

#include "unicode/ustring.h"
#include "uarrsort.h"

struct UResAEntry {
    UChar *key;
    UResourceBundle *item;
};

struct UResourceBundleAIterator {
    UResourceBundle  *bund;
    UResAEntry *entries;
    int32_t num;
    int32_t cursor;
};

/* Must be C linkage to pass function pointer to the sort function */

U_CDECL_BEGIN

static int32_t U_CALLCONV
ures_a_codepointSort(const void *context, const void *left, const void *right) {
    //CompareContext *cmp=(CompareContext *)context;
    return u_strcmp(((const UResAEntry *)left)->key,
                    ((const UResAEntry *)right)->key);
}

U_CDECL_END

static void ures_a_open(UResourceBundleAIterator *aiter, UResourceBundle *bund, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return;
    }
    aiter->bund = bund;
    aiter->num = ures_getSize(aiter->bund);
    aiter->cursor = 0;
#if !defined(U_SORT_ASCII_BUNDLE_ITERATOR)
    aiter->entries = NULL;
#else
    aiter->entries = (UResAEntry*)uprv_malloc(sizeof(UResAEntry)*aiter->num);
    for(int i=0;i<aiter->num;i++) {
        aiter->entries[i].item = ures_getByIndex(aiter->bund, i, NULL, status);
        const char *akey = ures_getKey(aiter->entries[i].item);
        int32_t len = uprv_strlen(akey)+1;
        aiter->entries[i].key = (UChar*)uprv_malloc(len*sizeof(UChar));
        u_charsToUChars(akey, aiter->entries[i].key, len);
    }
    uprv_sortArray(aiter->entries, aiter->num, sizeof(UResAEntry), ures_a_codepointSort, NULL, TRUE, status);
#endif
}

static void ures_a_close(UResourceBundleAIterator *aiter) {
#if defined(U_SORT_ASCII_BUNDLE_ITERATOR)
    for(int i=0;i<aiter->num;i++) {
        uprv_free(aiter->entries[i].key);
        ures_close(aiter->entries[i].item);
    }
#endif
}

static const UChar *ures_a_getNextString(UResourceBundleAIterator *aiter, int32_t *len, const char **key, UErrorCode *err) {
#if !defined(U_SORT_ASCII_BUNDLE_ITERATOR)
    return ures_getNextString(aiter->bund, len, key, err);
#else
    if(U_FAILURE(*err)) return NULL;
    UResourceBundle *item = aiter->entries[aiter->cursor].item;
    const UChar* ret = ures_getString(item, len, err);
    *key = ures_getKey(item);
    aiter->cursor++;
    return ret;
#endif
}


#endif


U_NAMESPACE_BEGIN

// *****************************************************************************
// class DateTimePatternGenerator
// *****************************************************************************
static const UChar Canonical_Items[] = {
    // GyQMwWEDFdaHmsSv
    CAP_G, LOW_Y, CAP_Q, CAP_M, LOW_W, CAP_W, CAP_E,
    CAP_D, CAP_F, LOW_D, LOW_A, // The UDATPG_x_FIELD constants and these fields have a different order than in ICU4J
    CAP_H, LOW_M, LOW_S, CAP_S, LOW_V, 0
};

static const dtTypeElem dtTypes[] = {
    // patternChar, field, type, minLen, weight
    {CAP_G, UDATPG_ERA_FIELD, DT_SHORT, 1, 3,},
    {CAP_G, UDATPG_ERA_FIELD, DT_LONG,  4, 0},
    {CAP_G, UDATPG_ERA_FIELD, DT_NARROW, 5, 0},

    {LOW_Y, UDATPG_YEAR_FIELD, DT_NUMERIC, 1, 20},
    {CAP_Y, UDATPG_YEAR_FIELD, DT_NUMERIC + DT_DELTA, 1, 20},
    {LOW_U, UDATPG_YEAR_FIELD, DT_NUMERIC + 2*DT_DELTA, 1, 20},
    {LOW_R, UDATPG_YEAR_FIELD, DT_NUMERIC + 3*DT_DELTA, 1, 20},
    {CAP_U, UDATPG_YEAR_FIELD, DT_SHORT, 1, 3},
    {CAP_U, UDATPG_YEAR_FIELD, DT_LONG, 4, 0},
    {CAP_U, UDATPG_YEAR_FIELD, DT_NARROW, 5, 0},

    {CAP_Q, UDATPG_QUARTER_FIELD, DT_NUMERIC, 1, 2},
    {CAP_Q, UDATPG_QUARTER_FIELD, DT_SHORT, 3, 0},
    {CAP_Q, UDATPG_QUARTER_FIELD, DT_LONG, 4, 0},
    {CAP_Q, UDATPG_QUARTER_FIELD, DT_NARROW, 5, 0},
    {LOW_Q, UDATPG_QUARTER_FIELD, DT_NUMERIC + DT_DELTA, 1, 2},
    {LOW_Q, UDATPG_QUARTER_FIELD, DT_SHORT - DT_DELTA, 3, 0},
    {LOW_Q, UDATPG_QUARTER_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {LOW_Q, UDATPG_QUARTER_FIELD, DT_NARROW - DT_DELTA, 5, 0},

    {CAP_M, UDATPG_MONTH_FIELD, DT_NUMERIC, 1, 2},
    {CAP_M, UDATPG_MONTH_FIELD, DT_SHORT, 3, 0},
    {CAP_M, UDATPG_MONTH_FIELD, DT_LONG, 4, 0},
    {CAP_M, UDATPG_MONTH_FIELD, DT_NARROW, 5, 0},
    {CAP_L, UDATPG_MONTH_FIELD, DT_NUMERIC + DT_DELTA, 1, 2},
    {CAP_L, UDATPG_MONTH_FIELD, DT_SHORT - DT_DELTA, 3, 0},
    {CAP_L, UDATPG_MONTH_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {CAP_L, UDATPG_MONTH_FIELD, DT_NARROW - DT_DELTA, 5, 0},
    {LOW_L, UDATPG_MONTH_FIELD, DT_NUMERIC + DT_DELTA, 1, 1},

    {LOW_W, UDATPG_WEEK_OF_YEAR_FIELD, DT_NUMERIC, 1, 2},

    {CAP_W, UDATPG_WEEK_OF_MONTH_FIELD, DT_NUMERIC, 1, 0},

    {CAP_E, UDATPG_WEEKDAY_FIELD, DT_SHORT, 1, 3},
    {CAP_E, UDATPG_WEEKDAY_FIELD, DT_LONG, 4, 0},
    {CAP_E, UDATPG_WEEKDAY_FIELD, DT_NARROW, 5, 0},
    {CAP_E, UDATPG_WEEKDAY_FIELD, DT_SHORTER, 6, 0},
    {LOW_C, UDATPG_WEEKDAY_FIELD, DT_NUMERIC + 2*DT_DELTA, 1, 2},
    {LOW_C, UDATPG_WEEKDAY_FIELD, DT_SHORT - 2*DT_DELTA, 3, 0},
    {LOW_C, UDATPG_WEEKDAY_FIELD, DT_LONG - 2*DT_DELTA, 4, 0},
    {LOW_C, UDATPG_WEEKDAY_FIELD, DT_NARROW - 2*DT_DELTA, 5, 0},
    {LOW_C, UDATPG_WEEKDAY_FIELD, DT_SHORTER - 2*DT_DELTA, 6, 0},
    {LOW_E, UDATPG_WEEKDAY_FIELD, DT_NUMERIC + DT_DELTA, 1, 2}, // LOW_E is currently not used in CLDR data, should not be canonical
    {LOW_E, UDATPG_WEEKDAY_FIELD, DT_SHORT - DT_DELTA, 3, 0},
    {LOW_E, UDATPG_WEEKDAY_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {LOW_E, UDATPG_WEEKDAY_FIELD, DT_NARROW - DT_DELTA, 5, 0},
    {LOW_E, UDATPG_WEEKDAY_FIELD, DT_SHORTER - DT_DELTA, 6, 0},

    {LOW_D, UDATPG_DAY_FIELD, DT_NUMERIC, 1, 2},
    {LOW_G, UDATPG_DAY_FIELD, DT_NUMERIC + DT_DELTA, 1, 20}, // really internal use, so we don't care

    {CAP_D, UDATPG_DAY_OF_YEAR_FIELD, DT_NUMERIC, 1, 3},

    {CAP_F, UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD, DT_NUMERIC, 1, 0},

    {LOW_A, UDATPG_DAYPERIOD_FIELD, DT_SHORT, 1, 3},
    {LOW_A, UDATPG_DAYPERIOD_FIELD, DT_LONG, 4, 0},
    {LOW_A, UDATPG_DAYPERIOD_FIELD, DT_NARROW, 5, 0},
    {LOW_B, UDATPG_DAYPERIOD_FIELD, DT_SHORT - DT_DELTA, 1, 3},
    {LOW_B, UDATPG_DAYPERIOD_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {LOW_B, UDATPG_DAYPERIOD_FIELD, DT_NARROW - DT_DELTA, 5, 0},
    // b needs to be closer to a than to B, so we make this 3*DT_DELTA
    {CAP_B, UDATPG_DAYPERIOD_FIELD, DT_SHORT - 3*DT_DELTA, 1, 3},
    {CAP_B, UDATPG_DAYPERIOD_FIELD, DT_LONG - 3*DT_DELTA, 4, 0},
    {CAP_B, UDATPG_DAYPERIOD_FIELD, DT_NARROW - 3*DT_DELTA, 5, 0},

    {CAP_H, UDATPG_HOUR_FIELD, DT_NUMERIC + 10*DT_DELTA, 1, 2}, // 24 hour
    {LOW_K, UDATPG_HOUR_FIELD, DT_NUMERIC + 11*DT_DELTA, 1, 2}, // 24 hour
    {LOW_H, UDATPG_HOUR_FIELD, DT_NUMERIC, 1, 2}, // 12 hour
    {CAP_K, UDATPG_HOUR_FIELD, DT_NUMERIC + DT_DELTA, 1, 2}, // 12 hour
    // The C code has had versions of the following 3, keep & update. Should not need these, but...
    // Without these, certain tests using e.g. staticGetSkeleton fail because j/J in patterns
    // get skipped instead of mapped to the right hour chars, for example in
    //   DateFormatTest::TestPatternFromSkeleton
    //   IntlTestDateTimePatternGeneratorAPI:: testStaticGetSkeleton
    //   DateIntervalFormatTest::testTicket11985
    // Need to investigate better handling of jJC replacement e.g. in staticGetSkeleton.
    {CAP_J, UDATPG_HOUR_FIELD, DT_NUMERIC + 5*DT_DELTA, 1, 2}, // 12/24 hour no AM/PM
    {LOW_J, UDATPG_HOUR_FIELD, DT_NUMERIC + 6*DT_DELTA, 1, 6}, // 12/24 hour
    {CAP_C, UDATPG_HOUR_FIELD, DT_NUMERIC + 7*DT_DELTA, 1, 6}, // 12/24 hour with preferred dayPeriods for 12

    {LOW_M, UDATPG_MINUTE_FIELD, DT_NUMERIC, 1, 2},

    {LOW_S, UDATPG_SECOND_FIELD, DT_NUMERIC, 1, 2},
    {CAP_A, UDATPG_SECOND_FIELD, DT_NUMERIC + DT_DELTA, 1, 1000},

    {CAP_S, UDATPG_FRACTIONAL_SECOND_FIELD, DT_NUMERIC, 1, 1000},

    {LOW_V, UDATPG_ZONE_FIELD, DT_SHORT - 2*DT_DELTA, 1, 0},
    {LOW_V, UDATPG_ZONE_FIELD, DT_LONG - 2*DT_DELTA, 4, 0},
    {LOW_Z, UDATPG_ZONE_FIELD, DT_SHORT, 1, 3},
    {LOW_Z, UDATPG_ZONE_FIELD, DT_LONG, 4, 0},
    {CAP_Z, UDATPG_ZONE_FIELD, DT_NARROW - DT_DELTA, 1, 3},
    {CAP_Z, UDATPG_ZONE_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {CAP_Z, UDATPG_ZONE_FIELD, DT_SHORT - DT_DELTA, 5, 0},
    {CAP_O, UDATPG_ZONE_FIELD, DT_SHORT - DT_DELTA, 1, 0},
    {CAP_O, UDATPG_ZONE_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {CAP_V, UDATPG_ZONE_FIELD, DT_SHORT - DT_DELTA, 1, 0},
    {CAP_V, UDATPG_ZONE_FIELD, DT_LONG - DT_DELTA, 2, 0},
    {CAP_V, UDATPG_ZONE_FIELD, DT_LONG-1 - DT_DELTA, 3, 0},
    {CAP_V, UDATPG_ZONE_FIELD, DT_LONG-2 - DT_DELTA, 4, 0},
    {CAP_X, UDATPG_ZONE_FIELD, DT_NARROW - DT_DELTA, 1, 0},
    {CAP_X, UDATPG_ZONE_FIELD, DT_SHORT - DT_DELTA, 2, 0},
    {CAP_X, UDATPG_ZONE_FIELD, DT_LONG - DT_DELTA, 4, 0},
    {LOW_X, UDATPG_ZONE_FIELD, DT_NARROW - DT_DELTA, 1, 0},
    {LOW_X, UDATPG_ZONE_FIELD, DT_SHORT - DT_DELTA, 2, 0},
    {LOW_X, UDATPG_ZONE_FIELD, DT_LONG - DT_DELTA, 4, 0},

    {0, UDATPG_FIELD_COUNT, 0, 0, 0} , // last row of dtTypes[]
 };

static const char* const CLDR_FIELD_APPEND[] = {
    "Era", "Year", "Quarter", "Month", "Week", "*", "Day-Of-Week",
    "*", "*", "Day", "*", // The UDATPG_x_FIELD constants and these fields have a different order than in ICU4J
    "Hour", "Minute", "Second", "*", "Timezone"
};

static const char* const CLDR_FIELD_NAME[] = {
    "era", "year", "quarter", "month", "week", "weekOfMonth", "weekday",
    "dayOfYear", "weekdayOfMonth", "day", "dayperiod", // The UDATPG_x_FIELD constants and these fields have a different order than in ICU4J
    "hour", "minute", "second", "*", "zone"
};

// For appendItems
static const UChar UDATPG_ItemFormat[]= {0x7B, 0x30, 0x7D, 0x20, 0x251C, 0x7B, 0x32, 0x7D, 0x3A,
    0x20, 0x7B, 0x31, 0x7D, 0x2524, 0};  // {0} \u251C{2}: {1}\u2524

//static const UChar repeatedPatterns[6]={CAP_G, CAP_E, LOW_Z, LOW_V, CAP_Q, 0}; // "GEzvQ"

static const char DT_DateTimePatternsTag[]="DateTimePatterns";
static const char DT_DateTimeCalendarTag[]="calendar";
static const char DT_DateTimeGregorianTag[]="gregorian";
static const char DT_DateTimeAppendItemsTag[]="appendItems";
static const char DT_DateTimeFieldsTag[]="fields";
static const char DT_DateTimeAvailableFormatsTag[]="availableFormats";
//static const UnicodeString repeatedPattern=UnicodeString(repeatedPatterns);

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateTimePatternGenerator)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DTSkeletonEnumeration)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DTRedundantEnumeration)

DateTimePatternGenerator*  U_EXPORT2
DateTimePatternGenerator::createInstance(UErrorCode& status) {
    return createInstance(Locale::getDefault(), status);
}

DateTimePatternGenerator* U_EXPORT2
DateTimePatternGenerator::createInstance(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    LocalPointer<DateTimePatternGenerator> result(
            new DateTimePatternGenerator(locale, status), status);
    return U_SUCCESS(status) ? result.orphan() : NULL;
}

DateTimePatternGenerator*  U_EXPORT2
DateTimePatternGenerator::createEmptyInstance(UErrorCode& status) {
    DateTimePatternGenerator *result = new DateTimePatternGenerator(status);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        delete result;
        result = NULL;
    }
    return result;
}

DateTimePatternGenerator::DateTimePatternGenerator(UErrorCode &status) :
    skipMatcher(NULL),
    fAvailableFormatKeyHash(NULL)
{
    fp = new FormatParser();
    dtMatcher = new DateTimeMatcher();
    distanceInfo = new DistanceInfo();
    patternMap = new PatternMap();
    if (fp == NULL || dtMatcher == NULL || distanceInfo == NULL || patternMap == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
}

DateTimePatternGenerator::DateTimePatternGenerator(const Locale& locale, UErrorCode &status) :
    skipMatcher(NULL),
    fAvailableFormatKeyHash(NULL)
{
    fp = new FormatParser();
    dtMatcher = new DateTimeMatcher();
    distanceInfo = new DistanceInfo();
    patternMap = new PatternMap();
    if (fp == NULL || dtMatcher == NULL || distanceInfo == NULL || patternMap == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    else {
        initData(locale, status);
    }
}

DateTimePatternGenerator::DateTimePatternGenerator(const DateTimePatternGenerator& other) :
    UObject(),
    skipMatcher(NULL),
    fAvailableFormatKeyHash(NULL)
{
    fp = new FormatParser();
    dtMatcher = new DateTimeMatcher();
    distanceInfo = new DistanceInfo();
    patternMap = new PatternMap();
    *this=other;
}

DateTimePatternGenerator&
DateTimePatternGenerator::operator=(const DateTimePatternGenerator& other) {
    // reflexive case
    if (&other == this) {
        return *this;
    }
    pLocale = other.pLocale;
    fDefaultHourFormatChar = other.fDefaultHourFormatChar;
    *fp = *(other.fp);
    dtMatcher->copyFrom(other.dtMatcher->skeleton);
    *distanceInfo = *(other.distanceInfo);
    dateTimeFormat = other.dateTimeFormat;
    decimal = other.decimal;
    // NUL-terminate for the C API.
    dateTimeFormat.getTerminatedBuffer();
    decimal.getTerminatedBuffer();
    delete skipMatcher;
    if ( other.skipMatcher == NULL ) {
        skipMatcher = NULL;
    }
    else {
        skipMatcher = new DateTimeMatcher(*other.skipMatcher);
    }
    for (int32_t i=0; i< UDATPG_FIELD_COUNT; ++i ) {
        appendItemFormats[i] = other.appendItemFormats[i];
        appendItemNames[i] = other.appendItemNames[i];
        // NUL-terminate for the C API.
        appendItemFormats[i].getTerminatedBuffer();
        appendItemNames[i].getTerminatedBuffer();
    }
    UErrorCode status = U_ZERO_ERROR;
    patternMap->copyFrom(*other.patternMap, status);
    copyHashtable(other.fAvailableFormatKeyHash, status);
    return *this;
}


UBool
DateTimePatternGenerator::operator==(const DateTimePatternGenerator& other) const {
    if (this == &other) {
        return TRUE;
    }
    if ((pLocale==other.pLocale) && (patternMap->equals(*other.patternMap)) &&
        (dateTimeFormat==other.dateTimeFormat) && (decimal==other.decimal)) {
        for ( int32_t i=0 ; i<UDATPG_FIELD_COUNT; ++i ) {
           if ((appendItemFormats[i] != other.appendItemFormats[i]) ||
               (appendItemNames[i] != other.appendItemNames[i]) ) {
               return FALSE;
           }
        }
        return TRUE;
    }
    else {
        return FALSE;
    }
}

UBool
DateTimePatternGenerator::operator!=(const DateTimePatternGenerator& other) const {
    return  !operator==(other);
}

DateTimePatternGenerator::~DateTimePatternGenerator() {
    if (fAvailableFormatKeyHash!=NULL) {
        delete fAvailableFormatKeyHash;
    }

    if (fp != NULL) delete fp;
    if (dtMatcher != NULL) delete dtMatcher;
    if (distanceInfo != NULL) delete distanceInfo;
    if (patternMap != NULL) delete patternMap;
    if (skipMatcher != NULL) delete skipMatcher;
}

namespace {

UInitOnce initOnce = U_INITONCE_INITIALIZER;
UHashtable *localeToAllowedHourFormatsMap = NULL;

// Value deleter for hashmap.
U_CFUNC void U_CALLCONV deleteAllowedHourFormats(void *ptr) {
    uprv_free(ptr);
}

// Close hashmap at cleanup.
U_CFUNC UBool U_CALLCONV allowedHourFormatsCleanup() {
    uhash_close(localeToAllowedHourFormatsMap);
    return TRUE;
}

enum AllowedHourFormat{
    ALLOWED_HOUR_FORMAT_UNKNOWN = -1,
    ALLOWED_HOUR_FORMAT_h,
    ALLOWED_HOUR_FORMAT_H,
    ALLOWED_HOUR_FORMAT_hb,
    ALLOWED_HOUR_FORMAT_Hb,
    ALLOWED_HOUR_FORMAT_hB,
    ALLOWED_HOUR_FORMAT_HB
};

}  // namespace

void
DateTimePatternGenerator::initData(const Locale& locale, UErrorCode &status) {
    //const char *baseLangName = locale.getBaseName(); // unused

    skipMatcher = NULL;
    fAvailableFormatKeyHash=NULL;
    addCanonicalItems(status);
    addICUPatterns(locale, status);
    addCLDRData(locale, status);
    setDateTimeFromCalendar(locale, status);
    setDecimalSymbols(locale, status);
    umtx_initOnce(initOnce, loadAllowedHourFormatsData, status);
    getAllowedHourFormats(locale, status);
} // DateTimePatternGenerator::initData

namespace {

struct AllowedHourFormatsSink : public ResourceSink {
    // Initialize sub-sinks.
    AllowedHourFormatsSink() {}
    virtual ~AllowedHourFormatsSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
                     UErrorCode &errorCode) {
        ResourceTable timeData = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; timeData.getKeyAndValue(i, key, value); ++i) {
            const char *regionOrLocale = key;
            ResourceTable formatList = value.getTable(errorCode);
            if (U_FAILURE(errorCode)) { return; }
            for (int32_t j = 0; formatList.getKeyAndValue(j, key, value); ++j) {
                if (uprv_strcmp(key, "allowed") == 0) {  // Ignore "preferred" list.
                    LocalMemory<int32_t> list;
                    int32_t length;
                    if (value.getType() == URES_STRING) {
                        if (list.allocateInsteadAndReset(2) == NULL) {
                            errorCode = U_MEMORY_ALLOCATION_ERROR;
                            return;
                        }
                        list[0] = getHourFormatFromUnicodeString(value.getUnicodeString(errorCode));
                        length = 1;
                    }
                    else {
                        ResourceArray allowedFormats = value.getArray(errorCode);
                        length = allowedFormats.getSize();
                        if (list.allocateInsteadAndReset(length + 1) == NULL) {
                            errorCode = U_MEMORY_ALLOCATION_ERROR;
                            return;
                        }
                        for (int32_t k = 0; k < length; ++k) {
                            allowedFormats.getValue(k, value);
                            list[k] = getHourFormatFromUnicodeString(value.getUnicodeString(errorCode));
                        }
                    }
                    list[length] = ALLOWED_HOUR_FORMAT_UNKNOWN;
                    uhash_put(localeToAllowedHourFormatsMap,
                              const_cast<char *>(regionOrLocale), list.orphan(), &errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                }
            }
        }
    }

    AllowedHourFormat getHourFormatFromUnicodeString(const UnicodeString &s) {
        if (s.length() == 1) {
            if (s[0] == LOW_H) { return ALLOWED_HOUR_FORMAT_h; }
            if (s[0] == CAP_H) { return ALLOWED_HOUR_FORMAT_H; }
        } else if (s.length() == 2) {
            if (s[0] == LOW_H && s[1] == LOW_B) { return ALLOWED_HOUR_FORMAT_hb; }
            if (s[0] == CAP_H && s[1] == LOW_B) { return ALLOWED_HOUR_FORMAT_Hb; }
            if (s[0] == LOW_H && s[1] == CAP_B) { return ALLOWED_HOUR_FORMAT_hB; }
            if (s[0] == CAP_H && s[1] == CAP_B) { return ALLOWED_HOUR_FORMAT_HB; }
        }

        return ALLOWED_HOUR_FORMAT_UNKNOWN;
    }
};

}  // namespace

AllowedHourFormatsSink::~AllowedHourFormatsSink() {}

U_CFUNC void U_CALLCONV DateTimePatternGenerator::loadAllowedHourFormatsData(UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    localeToAllowedHourFormatsMap = uhash_open(
        uhash_hashChars, uhash_compareChars, NULL, &status);
    uhash_setValueDeleter(localeToAllowedHourFormatsMap, deleteAllowedHourFormats);
    LocalUResourceBundlePointer rb(ures_openDirect(NULL, "supplementalData", &status));

    AllowedHourFormatsSink sink;
    // TODO: Currently in the enumeration each table allocates a new array.
    // Try to reduce the number of memory allocations. Consider storing a
    // UVector32 with the concatenation of all of the sub-arrays, put the start index
    // into the hashmap, store 6 single-value sub-arrays right at the beginning of the
    // vector (at index enum*2) for easy data sharing, copy sub-arrays into runtime
    // object. Remember to clean up the vector, too.
    ures_getAllItemsWithFallback(rb.getAlias(), "timeData", sink, status);

    ucln_i18n_registerCleanup(UCLN_I18N_ALLOWED_HOUR_FORMATS, allowedHourFormatsCleanup);
}

void DateTimePatternGenerator::getAllowedHourFormats(const Locale &locale, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    const char *localeID = locale.getName();
    char maxLocaleID[ULOC_FULLNAME_CAPACITY];
    int32_t length = uloc_addLikelySubtags(localeID, maxLocaleID, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status)) {
        return;
    } else if (length == ULOC_FULLNAME_CAPACITY) {  // no room for NUL
        status = U_BUFFER_OVERFLOW_ERROR;
        return;
    }
    Locale maxLocale = Locale(maxLocaleID);

    const char *country = maxLocale.getCountry();
    if (*country == '\0') { country = "001"; }
    const char *language = maxLocale.getLanguage();

    CharString langCountry;
    langCountry.append(language, uprv_strlen(language), status);
    langCountry.append('_', status);
    langCountry.append(country, uprv_strlen(country), status);

    int32_t *allowedFormats;
    allowedFormats = (int32_t *)uhash_get(localeToAllowedHourFormatsMap, langCountry.data());
    if (allowedFormats == NULL) {
        allowedFormats = (int32_t *)uhash_get(localeToAllowedHourFormatsMap, const_cast<char *>(country));
    }

    if (allowedFormats != NULL) {  // Lookup is successful
        for (int32_t i = 0; i < UPRV_LENGTHOF(fAllowedHourFormats); ++i) {
            fAllowedHourFormats[i] = allowedFormats[i];
            if (allowedFormats[i] == ALLOWED_HOUR_FORMAT_UNKNOWN) {
                break;
            }
        }
    } else {  // Lookup failed, twice
        fAllowedHourFormats[0] = ALLOWED_HOUR_FORMAT_H;
        fAllowedHourFormats[1] = ALLOWED_HOUR_FORMAT_UNKNOWN;
    }
}

UnicodeString
DateTimePatternGenerator::getSkeleton(const UnicodeString& pattern, UErrorCode&
/*status*/) {
    FormatParser fp;
    DateTimeMatcher matcher;
    PtnSkeleton localSkeleton;
    matcher.set(pattern, &fp, localSkeleton);
    return localSkeleton.getSkeleton();
}

UnicodeString
DateTimePatternGenerator::staticGetSkeleton(
        const UnicodeString& pattern, UErrorCode& /*status*/) {
    FormatParser fp;
    DateTimeMatcher matcher;
    PtnSkeleton localSkeleton;
    matcher.set(pattern, &fp, localSkeleton);
    return localSkeleton.getSkeleton();
}

UnicodeString
DateTimePatternGenerator::getBaseSkeleton(const UnicodeString& pattern, UErrorCode& /*status*/) {
    FormatParser fp;
    DateTimeMatcher matcher;
    PtnSkeleton localSkeleton;
    matcher.set(pattern, &fp, localSkeleton);
    return localSkeleton.getBaseSkeleton();
}

UnicodeString
DateTimePatternGenerator::staticGetBaseSkeleton(
        const UnicodeString& pattern, UErrorCode& /*status*/) {
    FormatParser fp;
    DateTimeMatcher matcher;
    PtnSkeleton localSkeleton;
    matcher.set(pattern, &fp, localSkeleton);
    return localSkeleton.getBaseSkeleton();
}

void
DateTimePatternGenerator::addICUPatterns(const Locale& locale, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    UnicodeString dfPattern;
    UnicodeString conflictingString;
    DateFormat* df;

    // Load with ICU patterns
    for (int32_t i=DateFormat::kFull; i<=DateFormat::kShort; i++) {
        DateFormat::EStyle style = (DateFormat::EStyle)i;
        df = DateFormat::createDateInstance(style, locale);
        SimpleDateFormat* sdf;
        if (df != NULL && (sdf = dynamic_cast<SimpleDateFormat*>(df)) != NULL) {
            sdf->toPattern(dfPattern);
            addPattern(dfPattern, FALSE, conflictingString, status);
        }
        // TODO Maybe we should return an error when the date format isn't simple.
        delete df;
        if (U_FAILURE(status)) { return; }

        df = DateFormat::createTimeInstance(style, locale);
        if (df != NULL && (sdf = dynamic_cast<SimpleDateFormat*>(df)) != NULL) {
            sdf->toPattern(dfPattern);
            addPattern(dfPattern, FALSE, conflictingString, status);

            // TODO: C++ and Java are inconsistent (see #12568).
            // C++ uses MEDIUM, but Java uses SHORT.
            if ( i==DateFormat::kShort && !dfPattern.isEmpty() ) {
                consumeShortTimePattern(dfPattern, status);
            }
        }
        // TODO Maybe we should return an error when the date format isn't simple.
        delete df;
        if (U_FAILURE(status)) { return; }
    }
}

void
DateTimePatternGenerator::hackTimes(const UnicodeString& hackPattern, UErrorCode& status)  {
    UnicodeString conflictingString;

    fp->set(hackPattern);
    UnicodeString mmss;
    UBool gotMm=FALSE;
    for (int32_t i=0; i<fp->itemNumber; ++i) {
        UnicodeString field = fp->items[i];
        if ( fp->isQuoteLiteral(field) ) {
            if ( gotMm ) {
               UnicodeString quoteLiteral;
               fp->getQuoteLiteral(quoteLiteral, &i);
               mmss += quoteLiteral;
            }
        }
        else {
            if (fp->isPatternSeparator(field) && gotMm) {
                mmss+=field;
            }
            else {
                UChar ch=field.charAt(0);
                if (ch==LOW_M) {
                    gotMm=TRUE;
                    mmss+=field;
                }
                else {
                    if (ch==LOW_S) {
                        if (!gotMm) {
                            break;
                        }
                        mmss+= field;
                        addPattern(mmss, FALSE, conflictingString, status);
                        break;
                    }
                    else {
                        if (gotMm || ch==LOW_Z || ch==CAP_Z || ch==LOW_V || ch==CAP_V) {
                            break;
                        }
                    }
                }
            }
        }
    }
}

#define ULOC_LOCALE_IDENTIFIER_CAPACITY (ULOC_FULLNAME_CAPACITY + 1 + ULOC_KEYWORD_AND_VALUES_CAPACITY)

static const UChar hourFormatChars[] = { CAP_H, LOW_H, CAP_K, LOW_K, 0 }; // HhKk, the hour format characters

void
DateTimePatternGenerator::getCalendarTypeToUse(const Locale& locale, CharString& destination, UErrorCode& err) {
    destination.clear().append(DT_DateTimeGregorianTag, -1, err); // initial default
    if ( U_SUCCESS(err) ) {
        char localeWithCalendarKey[ULOC_LOCALE_IDENTIFIER_CAPACITY];
        // obtain a locale that always has the calendar key value that should be used
        ures_getFunctionalEquivalent(
            localeWithCalendarKey,
            ULOC_LOCALE_IDENTIFIER_CAPACITY,
            NULL,
            "calendar",
            "calendar",
            locale.getName(),
            NULL,
            FALSE,
            &err);
        localeWithCalendarKey[ULOC_LOCALE_IDENTIFIER_CAPACITY-1] = 0; // ensure null termination
        // now get the calendar key value from that locale
        char calendarType[ULOC_KEYWORDS_CAPACITY];
        int32_t calendarTypeLen = uloc_getKeywordValue(
            localeWithCalendarKey,
            "calendar",
            calendarType,
            ULOC_KEYWORDS_CAPACITY,
            &err);
        if (U_SUCCESS(err) && calendarTypeLen < ULOC_KEYWORDS_CAPACITY) {
            destination.clear().append(calendarType, -1, err);
            if (U_FAILURE(err)) { return; }
        }
        err = U_ZERO_ERROR;
    }
}

void
DateTimePatternGenerator::consumeShortTimePattern(const UnicodeString& shortTimePattern,
        UErrorCode& status) {

    // set fDefaultHourFormatChar to the hour format character from this pattern
    int32_t tfIdx, tfLen = shortTimePattern.length();
    UBool ignoreChars = FALSE;
    for (tfIdx = 0; tfIdx < tfLen; tfIdx++) {
        UChar tfChar = shortTimePattern.charAt(tfIdx);
        if ( tfChar == SINGLE_QUOTE ) {
            ignoreChars = !ignoreChars; // toggle (handle quoted literals & '' for single quote)
        } else if ( !ignoreChars && u_strchr(hourFormatChars, tfChar) != NULL ) {
            fDefaultHourFormatChar = tfChar;
            break;
        }
    }

    // HACK for hh:ss
    hackTimes(shortTimePattern, status);
}

struct DateTimePatternGenerator::AppendItemFormatsSink : public ResourceSink {

    // Destination for data, modified via setters.
    DateTimePatternGenerator& dtpg;

    AppendItemFormatsSink(DateTimePatternGenerator& _dtpg) : dtpg(_dtpg) {}
    virtual ~AppendItemFormatsSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        ResourceTable itemsTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; itemsTable.getKeyAndValue(i, key, value); ++i) {
            UDateTimePatternField field = dtpg.getAppendFormatNumber(key);
            if (field == UDATPG_FIELD_COUNT) { continue; }
            const UnicodeString& valueStr = value.getUnicodeString(errorCode);
            if (dtpg.getAppendItemFormat(field).isEmpty() && !valueStr.isEmpty()) {
                dtpg.setAppendItemFormat(field, valueStr);
            }
        }
    }

    void fillInMissing() {
        UnicodeString defaultItemFormat(TRUE, UDATPG_ItemFormat, UPRV_LENGTHOF(UDATPG_ItemFormat)-1);  // Read-only alias.
        for (int32_t i = 0; i < UDATPG_FIELD_COUNT; i++) {
            UDateTimePatternField field = (UDateTimePatternField)i;
            if (dtpg.getAppendItemFormat(field).isEmpty()) {
                dtpg.setAppendItemFormat(field, defaultItemFormat);
            }
        }
    }
};

struct DateTimePatternGenerator::AppendItemNamesSink : public ResourceSink {

    // Destination for data, modified via setters.
    DateTimePatternGenerator& dtpg;

    AppendItemNamesSink(DateTimePatternGenerator& _dtpg) : dtpg(_dtpg) {}
    virtual ~AppendItemNamesSink();

    virtual void put(const char *key, ResourceValue &value, UBool /*noFallback*/,
            UErrorCode &errorCode) {
        ResourceTable itemsTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; itemsTable.getKeyAndValue(i, key, value); ++i) {
            UDateTimePatternField field = dtpg.getAppendNameNumber(key);
            if (field == UDATPG_FIELD_COUNT) { continue; }
            ResourceTable detailsTable = value.getTable(errorCode);
            if (U_FAILURE(errorCode)) { return; }
            for (int32_t j = 0; detailsTable.getKeyAndValue(j, key, value); ++j) {
                if (uprv_strcmp(key, "dn") != 0) { continue; }
                const UnicodeString& valueStr = value.getUnicodeString(errorCode);
                if (dtpg.getAppendItemName(field).isEmpty() && !valueStr.isEmpty()) {
                    dtpg.setAppendItemName(field, valueStr);
                }
                break;
            }
        }
    }

    void fillInMissing() {
        for (int32_t i = 0; i < UDATPG_FIELD_COUNT; i++) {
            UDateTimePatternField field = (UDateTimePatternField)i;
            UnicodeString& valueStr = dtpg.getMutableAppendItemName(field);
            if (valueStr.isEmpty()) {
                valueStr = CAP_F;
                U_ASSERT(i < 20);
                if (i < 10) {
                    // F0, F1, ..., F9
                    valueStr += (UChar)(i+0x30);
                } else {
                    // F10, F11, ...
                    valueStr += (UChar)0x31;
                    valueStr += (UChar)(i-10 + 0x30);
                }
                // NUL-terminate for the C API.
                valueStr.getTerminatedBuffer();
            }
        }
    }
};

struct DateTimePatternGenerator::AvailableFormatsSink : public ResourceSink {

    // Destination for data, modified via setters.
    DateTimePatternGenerator& dtpg;

    // Temporary variable, required for calling addPatternWithSkeleton.
    UnicodeString conflictingPattern;

    AvailableFormatsSink(DateTimePatternGenerator& _dtpg) : dtpg(_dtpg) {}
    virtual ~AvailableFormatsSink();

    virtual void put(const char *key, ResourceValue &value, UBool isRoot,
            UErrorCode &errorCode) {
        ResourceTable itemsTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }
        for (int32_t i = 0; itemsTable.getKeyAndValue(i, key, value); ++i) {
            const UnicodeString formatKey(key, -1, US_INV);
            if (!dtpg.isAvailableFormatSet(formatKey) ) {
                dtpg.setAvailableFormat(formatKey, errorCode);
                // Add pattern with its associated skeleton. Override any duplicate
                // derived from std patterns, but not a previous availableFormats entry:
                const UnicodeString& formatValue = value.getUnicodeString(errorCode);
                conflictingPattern.remove();
                dtpg.addPatternWithSkeleton(formatValue, &formatKey, !isRoot, conflictingPattern, errorCode);
            }
        }
    }
};

// Virtual destructors must be defined out of line.
DateTimePatternGenerator::AppendItemFormatsSink::~AppendItemFormatsSink() {}
DateTimePatternGenerator::AppendItemNamesSink::~AppendItemNamesSink() {}
DateTimePatternGenerator::AvailableFormatsSink::~AvailableFormatsSink() {}

void
DateTimePatternGenerator::addCLDRData(const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) { return; }
    UnicodeString rbPattern, value, field;
    CharString path;

    LocalUResourceBundlePointer rb(ures_open(NULL, locale.getName(), &errorCode));
    if (U_FAILURE(errorCode)) { return; }

    CharString calendarTypeToUse; // to be filled in with the type to use, if all goes well
    getCalendarTypeToUse(locale, calendarTypeToUse, errorCode);
    if (U_FAILURE(errorCode)) { return; }

    // Local err to ignore resource not found exceptions
    UErrorCode err = U_ZERO_ERROR;

    // Load append item formats.
    AppendItemFormatsSink appendItemFormatsSink(*this);
    path.clear()
        .append(DT_DateTimeCalendarTag, errorCode)
        .append('/', errorCode)
        .append(calendarTypeToUse, errorCode)
        .append('/', errorCode)
        .append(DT_DateTimeAppendItemsTag, errorCode); // i.e., calendar/xxx/appendItems
    if (U_FAILURE(errorCode)) { return; }
    ures_getAllItemsWithFallback(rb.getAlias(), path.data(), appendItemFormatsSink, err);
    appendItemFormatsSink.fillInMissing();

    // Load CLDR item names.
    err = U_ZERO_ERROR;
    AppendItemNamesSink appendItemNamesSink(*this);
    ures_getAllItemsWithFallback(rb.getAlias(), DT_DateTimeFieldsTag, appendItemNamesSink, err);
    appendItemNamesSink.fillInMissing();

    // Load the available formats from CLDR.
    err = U_ZERO_ERROR;
    initHashtable(errorCode);
    if (U_FAILURE(errorCode)) { return; }
    AvailableFormatsSink availableFormatsSink(*this);
    path.clear()
        .append(DT_DateTimeCalendarTag, errorCode)
        .append('/', errorCode)
        .append(calendarTypeToUse, errorCode)
        .append('/', errorCode)
        .append(DT_DateTimeAvailableFormatsTag, errorCode); // i.e., calendar/xxx/availableFormats
    if (U_FAILURE(errorCode)) { return; }
    ures_getAllItemsWithFallback(rb.getAlias(), path.data(), availableFormatsSink, err);
}

void
DateTimePatternGenerator::initHashtable(UErrorCode& err) {
    if (fAvailableFormatKeyHash!=NULL) {
        return;
    }
    if ((fAvailableFormatKeyHash = new Hashtable(FALSE, err))==NULL) {
        err=U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

void
DateTimePatternGenerator::setAppendItemFormat(UDateTimePatternField field, const UnicodeString& value) {
    appendItemFormats[field] = value;
    // NUL-terminate for the C API.
    appendItemFormats[field].getTerminatedBuffer();
}

const UnicodeString&
DateTimePatternGenerator::getAppendItemFormat(UDateTimePatternField field) const {
    return appendItemFormats[field];
}

void
DateTimePatternGenerator::setAppendItemName(UDateTimePatternField field, const UnicodeString& value) {
    appendItemNames[field] = value;
    // NUL-terminate for the C API.
    appendItemNames[field].getTerminatedBuffer();
}

const UnicodeString&
DateTimePatternGenerator::getAppendItemName(UDateTimePatternField field) const {
    return appendItemNames[field];
}

UnicodeString&
DateTimePatternGenerator::getMutableAppendItemName(UDateTimePatternField field) {
    return appendItemNames[field];
}

void
DateTimePatternGenerator::getAppendName(UDateTimePatternField field, UnicodeString& value) {
    value = SINGLE_QUOTE;
    value += appendItemNames[field];
    value += SINGLE_QUOTE;
}

UnicodeString
DateTimePatternGenerator::getBestPattern(const UnicodeString& patternForm, UErrorCode& status) {
    return getBestPattern(patternForm, UDATPG_MATCH_NO_OPTIONS, status);
}

UnicodeString
DateTimePatternGenerator::getBestPattern(const UnicodeString& patternForm, UDateTimePatternMatchOptions options, UErrorCode& status) {
    const UnicodeString *bestPattern=NULL;
    UnicodeString dtFormat;
    UnicodeString resultPattern;
    int32_t flags = kDTPGNoFlags;

    int32_t dateMask=(1<<UDATPG_DAYPERIOD_FIELD) - 1;
    int32_t timeMask=(1<<UDATPG_FIELD_COUNT) - 1 - dateMask;

    // Replace hour metacharacters 'j', 'C' and 'J', set flags as necessary
    UnicodeString patternFormMapped = mapSkeletonMetacharacters(patternForm, &flags, status);
    if (U_FAILURE(status)) {
        return UnicodeString();
    }

    resultPattern.remove();
    dtMatcher->set(patternFormMapped, fp);
    const PtnSkeleton* specifiedSkeleton=NULL;
    bestPattern=getBestRaw(*dtMatcher, -1, distanceInfo, &specifiedSkeleton);
    if ( distanceInfo->missingFieldMask==0 && distanceInfo->extraFieldMask==0 ) {
        resultPattern = adjustFieldTypes(*bestPattern, specifiedSkeleton, flags, options);

        return resultPattern;
    }
    int32_t neededFields = dtMatcher->getFieldMask();
    UnicodeString datePattern=getBestAppending(neededFields & dateMask, flags, options);
    UnicodeString timePattern=getBestAppending(neededFields & timeMask, flags, options);
    if (datePattern.length()==0) {
        if (timePattern.length()==0) {
            resultPattern.remove();
        }
        else {
            return timePattern;
        }
    }
    if (timePattern.length()==0) {
        return datePattern;
    }
    resultPattern.remove();
    status = U_ZERO_ERROR;
    dtFormat=getDateTimeFormat();
    SimpleFormatter(dtFormat, 2, 2, status).format(timePattern, datePattern, resultPattern, status);
    return resultPattern;
}

/*
 * Map a skeleton that may have metacharacters jJC to one without, by replacing
 * the metacharacters with locale-appropriate fields of of h/H/k/K and of a/b/B
 * (depends on fDefaultHourFormatChar and fAllowedHourFormats being set, which in
 * turn depends on initData having been run). This method also updates the flags
 * as necessary. Returns the updated skeleton.
 */
UnicodeString
DateTimePatternGenerator::mapSkeletonMetacharacters(const UnicodeString& patternForm, int32_t* flags, UErrorCode& status) {
    UnicodeString patternFormMapped;
    patternFormMapped.remove();
    UBool inQuoted = FALSE;
    int32_t patPos, patLen = patternForm.length();
    for (patPos = 0; patPos < patLen; patPos++) {
        UChar patChr = patternForm.charAt(patPos);
        if (patChr == SINGLE_QUOTE) {
            inQuoted = !inQuoted;
        } else if (!inQuoted) {
            // Handle special mappings for 'j' and 'C' in which fields lengths
            // 1,3,5 => hour field length 1
            // 2,4,6 => hour field length 2
            // 1,2 => abbreviated dayPeriod (field length 1..3)
            // 3,4 => long dayPeriod (field length 4)
            // 5,6 => narrow dayPeriod (field length 5)
            if (patChr == LOW_J || patChr == CAP_C) {
                int32_t extraLen = 0; // 1 less than total field length
                while (patPos+1 < patLen && patternForm.charAt(patPos+1)==patChr) {
                    extraLen++;
                    patPos++;
                }
                int32_t hourLen = 1 + (extraLen & 1);
                int32_t dayPeriodLen = (extraLen < 2)? 1: 3 + (extraLen >> 1);
                UChar hourChar = LOW_H;
                UChar dayPeriodChar = LOW_A;
                if (patChr == LOW_J) {
                    hourChar = fDefaultHourFormatChar;
                } else {
                    AllowedHourFormat preferred;
                    if (fAllowedHourFormats[0] != ALLOWED_HOUR_FORMAT_UNKNOWN) {
                        preferred = (AllowedHourFormat)fAllowedHourFormats[0];
                    } else {
                        status = U_INVALID_FORMAT_ERROR;
                        return UnicodeString();
                    }
                    if (preferred == ALLOWED_HOUR_FORMAT_H || preferred == ALLOWED_HOUR_FORMAT_HB || preferred == ALLOWED_HOUR_FORMAT_Hb) {
                        hourChar = CAP_H;
                    }
                    // in #13183 just add b/B to skeleton, no longer need to set special flags
                    if (preferred == ALLOWED_HOUR_FORMAT_HB || preferred == ALLOWED_HOUR_FORMAT_hB) {
                        dayPeriodChar = CAP_B;
                    } else if (preferred == ALLOWED_HOUR_FORMAT_Hb || preferred == ALLOWED_HOUR_FORMAT_hb) {
                        dayPeriodChar = LOW_B;
                    }
                }
                if (hourChar==CAP_H || hourChar==LOW_K) {
                    dayPeriodLen = 0;
                }
                while (dayPeriodLen-- > 0) {
                    patternFormMapped.append(dayPeriodChar);
                }
                while (hourLen-- > 0) {
                    patternFormMapped.append(hourChar);
                }
            } else if (patChr == CAP_J) {
                // Get pattern for skeleton with H, then replace H or k
                // with fDefaultHourFormatChar (if different)
                patternFormMapped.append(CAP_H);
                *flags |= kDTPGSkeletonUsesCapJ;
            } else {
                patternFormMapped.append(patChr);
            }
        }
    }
    return patternFormMapped;
}

UnicodeString
DateTimePatternGenerator::replaceFieldTypes(const UnicodeString& pattern,
                                            const UnicodeString& skeleton,
                                            UErrorCode& status) {
    return replaceFieldTypes(pattern, skeleton, UDATPG_MATCH_NO_OPTIONS, status);
}

UnicodeString
DateTimePatternGenerator::replaceFieldTypes(const UnicodeString& pattern,
                                            const UnicodeString& skeleton,
                                            UDateTimePatternMatchOptions options,
                                            UErrorCode& /*status*/) {
    dtMatcher->set(skeleton, fp);
    UnicodeString result = adjustFieldTypes(pattern, NULL, kDTPGNoFlags, options);
    return result;
}

void
DateTimePatternGenerator::setDecimal(const UnicodeString& newDecimal) {
    this->decimal = newDecimal;
    // NUL-terminate for the C API.
    this->decimal.getTerminatedBuffer();
}

const UnicodeString&
DateTimePatternGenerator::getDecimal() const {
    return decimal;
}

void
DateTimePatternGenerator::addCanonicalItems(UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    UnicodeString  conflictingPattern;

    for (int32_t i=0; i<UDATPG_FIELD_COUNT; i++) {
        if (Canonical_Items[i] > 0) {
            addPattern(UnicodeString(Canonical_Items[i]), FALSE, conflictingPattern, status);
        }
        if (U_FAILURE(status)) { return; }
    }
}

void
DateTimePatternGenerator::setDateTimeFormat(const UnicodeString& dtFormat) {
    dateTimeFormat = dtFormat;
    // NUL-terminate for the C API.
    dateTimeFormat.getTerminatedBuffer();
}

const UnicodeString&
DateTimePatternGenerator::getDateTimeFormat() const {
    return dateTimeFormat;
}

void
DateTimePatternGenerator::setDateTimeFromCalendar(const Locale& locale, UErrorCode& status) {
    const UChar *resStr;
    int32_t resStrLen = 0;

    Calendar* fCalendar = Calendar::createInstance(locale, status);
    if (U_FAILURE(status)) { return; }

    LocalUResourceBundlePointer calData(ures_open(NULL, locale.getBaseName(), &status));
    ures_getByKey(calData.getAlias(), DT_DateTimeCalendarTag, calData.getAlias(), &status);

    LocalUResourceBundlePointer dateTimePatterns;
    if (fCalendar != NULL && fCalendar->getType() != NULL && *fCalendar->getType() != '\0'
            && uprv_strcmp(fCalendar->getType(), DT_DateTimeGregorianTag) != 0) {
        dateTimePatterns.adoptInstead(ures_getByKeyWithFallback(calData.getAlias(), fCalendar->getType(),
                                                                NULL, &status));
        ures_getByKeyWithFallback(dateTimePatterns.getAlias(), DT_DateTimePatternsTag,
                                  dateTimePatterns.getAlias(), &status);
    }

    if (dateTimePatterns.isNull() || status == U_MISSING_RESOURCE_ERROR) {
        status = U_ZERO_ERROR;
        dateTimePatterns.adoptInstead(ures_getByKeyWithFallback(calData.getAlias(), DT_DateTimeGregorianTag,
                                                                dateTimePatterns.orphan(), &status));
        ures_getByKeyWithFallback(dateTimePatterns.getAlias(), DT_DateTimePatternsTag,
                                  dateTimePatterns.getAlias(), &status);
    }
    if (U_FAILURE(status)) { return; }

    if (ures_getSize(dateTimePatterns.getAlias()) <= DateFormat::kDateTime)
    {
        status = U_INVALID_FORMAT_ERROR;
        return;
    }
    resStr = ures_getStringByIndex(dateTimePatterns.getAlias(), (int32_t)DateFormat::kDateTime, &resStrLen, &status);
    setDateTimeFormat(UnicodeString(TRUE, resStr, resStrLen));

    delete fCalendar;
}

void
DateTimePatternGenerator::setDecimalSymbols(const Locale& locale, UErrorCode& status) {
    DecimalFormatSymbols dfs = DecimalFormatSymbols(locale, status);
    if(U_SUCCESS(status)) {
        decimal = dfs.getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
        // NUL-terminate for the C API.
        decimal.getTerminatedBuffer();
    }
}

UDateTimePatternConflict
DateTimePatternGenerator::addPattern(
    const UnicodeString& pattern,
    UBool override,
    UnicodeString &conflictingPattern,
    UErrorCode& status)
{
    return addPatternWithSkeleton(pattern, NULL, override, conflictingPattern, status);
}

// For DateTimePatternGenerator::addPatternWithSkeleton -
// If skeletonToUse is specified, then an availableFormats entry is being added. In this case:
// 1. We pass that skeleton to matcher.set instead of having it derive a skeleton from the pattern.
// 2. If the new entry's skeleton or basePattern does match an existing entry but that entry also had a skeleton specified
// (i.e. it was also from availableFormats), then the new entry does not override it regardless of the value of the override
// parameter. This prevents later availableFormats entries from a parent locale overriding earlier ones from the actual
// specified locale. However, availableFormats entries *should* override entries with matching skeleton whose skeleton was
// derived (i.e. entries derived from the standard date/time patters for the specified locale).
// 3. When adding the pattern (patternMap->add), we set a new boolean to indicate that the added entry had a
// specified skeleton (which sets a new field in the PtnElem in the PatternMap).
UDateTimePatternConflict
DateTimePatternGenerator::addPatternWithSkeleton(
    const UnicodeString& pattern,
    const UnicodeString* skeletonToUse,
    UBool override,
    UnicodeString& conflictingPattern,
    UErrorCode& status)
{

    UnicodeString basePattern;
    PtnSkeleton   skeleton;
    UDateTimePatternConflict conflictingStatus = UDATPG_NO_CONFLICT;

    DateTimeMatcher matcher;
    if ( skeletonToUse == NULL ) {
        matcher.set(pattern, fp, skeleton);
        matcher.getBasePattern(basePattern);
    } else {
        matcher.set(*skeletonToUse, fp, skeleton); // no longer trims skeleton fields to max len 3, per #7930
        matcher.getBasePattern(basePattern); // or perhaps instead: basePattern = *skeletonToUse;
    }
    // We only care about base conflicts - and replacing the pattern associated with a base - if:
    // 1. the conflicting previous base pattern did *not* have an explicit skeleton; in that case the previous
    // base + pattern combination was derived from either (a) a canonical item, (b) a standard format, or
    // (c) a pattern specified programmatically with a previous call to addPattern (which would only happen
    // if we are getting here from a subsequent call to addPattern).
    // 2. a skeleton is specified for the current pattern, but override=false; in that case we are checking
    // availableFormats items from root, which should not override any previous entry with the same base.
    UBool entryHadSpecifiedSkeleton;
    const UnicodeString *duplicatePattern = patternMap->getPatternFromBasePattern(basePattern, entryHadSpecifiedSkeleton);
    if (duplicatePattern != NULL && (!entryHadSpecifiedSkeleton || (skeletonToUse != NULL && !override))) {
        conflictingStatus = UDATPG_BASE_CONFLICT;
        conflictingPattern = *duplicatePattern;
        if (!override) {
            return conflictingStatus;
        }
    }
    // The only time we get here with override=true and skeletonToUse!=null is when adding availableFormats
    // items from CLDR data. In that case, we don't want an item from a parent locale to replace an item with
    // same skeleton from the specified locale, so skip the current item if skeletonWasSpecified is true for
    // the previously-specified conflicting item.
    const PtnSkeleton* entrySpecifiedSkeleton = NULL;
    duplicatePattern = patternMap->getPatternFromSkeleton(skeleton, &entrySpecifiedSkeleton);
    if (duplicatePattern != NULL ) {
        conflictingStatus = UDATPG_CONFLICT;
        conflictingPattern = *duplicatePattern;
        if (!override || (skeletonToUse != NULL && entrySpecifiedSkeleton != NULL)) {
            return conflictingStatus;
        }
    }
    patternMap->add(basePattern, skeleton, pattern, skeletonToUse != NULL, status);
    if(U_FAILURE(status)) {
        return conflictingStatus;
    }

    return UDATPG_NO_CONFLICT;
}


UDateTimePatternField
DateTimePatternGenerator::getAppendFormatNumber(const char* field) const {
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i ) {
        if (uprv_strcmp(CLDR_FIELD_APPEND[i], field)==0) {
            return (UDateTimePatternField)i;
        }
    }
    return UDATPG_FIELD_COUNT;
}

UDateTimePatternField
DateTimePatternGenerator::getAppendNameNumber(const char* field) const {
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i ) {
        if (uprv_strcmp(CLDR_FIELD_NAME[i],field)==0) {
            return (UDateTimePatternField)i;
        }
    }
    return UDATPG_FIELD_COUNT;
}

const UnicodeString*
DateTimePatternGenerator::getBestRaw(DateTimeMatcher& source,
                                     int32_t includeMask,
                                     DistanceInfo* missingFields,
                                     const PtnSkeleton** specifiedSkeletonPtr) {
    int32_t bestDistance = 0x7fffffff;
    DistanceInfo tempInfo;
    const UnicodeString *bestPattern=NULL;
    const PtnSkeleton* specifiedSkeleton=NULL;

    PatternMapIterator it;
    for (it.set(*patternMap); it.hasNext(); ) {
        DateTimeMatcher trial = it.next();
        if (trial.equals(skipMatcher)) {
            continue;
        }
        int32_t distance=source.getDistance(trial, includeMask, tempInfo);
        if (distance<bestDistance) {
            bestDistance=distance;
            bestPattern=patternMap->getPatternFromSkeleton(*trial.getSkeletonPtr(), &specifiedSkeleton);
            missingFields->setTo(tempInfo);
            if (distance==0) {
                break;
            }
        }
    }

    // If the best raw match had a specified skeleton and that skeleton was requested by the caller,
    // then return it too. This generally happens when the caller needs to pass that skeleton
    // through to adjustFieldTypes so the latter can do a better job.
    if (bestPattern && specifiedSkeletonPtr) {
        *specifiedSkeletonPtr = specifiedSkeleton;
    }
    return bestPattern;
}

UnicodeString
DateTimePatternGenerator::adjustFieldTypes(const UnicodeString& pattern,
                                           const PtnSkeleton* specifiedSkeleton,
                                           int32_t flags,
                                           UDateTimePatternMatchOptions options) {
    UnicodeString newPattern;
    fp->set(pattern);
    for (int32_t i=0; i < fp->itemNumber; i++) {
        UnicodeString field = fp->items[i];
        if ( fp->isQuoteLiteral(field) ) {

            UnicodeString quoteLiteral;
            fp->getQuoteLiteral(quoteLiteral, &i);
            newPattern += quoteLiteral;
        }
        else {
            if (fp->isPatternSeparator(field)) {
                newPattern+=field;
                continue;
            }
            int32_t canonicalIndex = fp->getCanonicalIndex(field);
            if (canonicalIndex < 0) {
                newPattern+=field;
                continue;  // don't adjust
            }
            const dtTypeElem *row = &dtTypes[canonicalIndex];
            int32_t typeValue = row->field;

            // handle day periods - with #13183, no longer need special handling here, integrated with normal types

            if ((flags & kDTPGFixFractionalSeconds) != 0 && typeValue == UDATPG_SECOND_FIELD) {
                field += decimal;
                dtMatcher->skeleton.original.appendFieldTo(UDATPG_FRACTIONAL_SECOND_FIELD, field);
            } else if (dtMatcher->skeleton.type[typeValue]!=0) {
                    // Here:
                    // - "reqField" is the field from the originally requested skeleton, with length
                    // "reqFieldLen".
                    // - "field" is the field from the found pattern.
                    //
                    // The adjusted field should consist of characters from the originally requested
                    // skeleton, except in the case of UDATPG_HOUR_FIELD or UDATPG_MONTH_FIELD or
                    // UDATPG_WEEKDAY_FIELD or UDATPG_YEAR_FIELD, in which case it should consist
                    // of characters from the  found pattern.
                    //
                    // The length of the adjusted field (adjFieldLen) should match that in the originally
                    // requested skeleton, except that in the following cases the length of the adjusted field
                    // should match that in the found pattern (i.e. the length of this pattern field should
                    // not be adjusted):
                    // 1. typeValue is UDATPG_HOUR_FIELD/MINUTE/SECOND and the corresponding bit in options is
                    //    not set (ticket #7180). Note, we may want to implement a similar change for other
                    //    numeric fields (MM, dd, etc.) so the default behavior is to get locale preference for
                    //    field length, but options bits can be used to override this.
                    // 2. There is a specified skeleton for the found pattern and one of the following is true:
                    //    a) The length of the field in the skeleton (skelFieldLen) is equal to reqFieldLen.
                    //    b) The pattern field is numeric and the skeleton field is not, or vice versa.

                    UChar reqFieldChar = dtMatcher->skeleton.original.getFieldChar(typeValue);
                    int32_t reqFieldLen = dtMatcher->skeleton.original.getFieldLength(typeValue);
                    if (reqFieldChar == CAP_E && reqFieldLen < 3)
                        reqFieldLen = 3; // 1-3 for E are equivalent to 3 for c,e
                    int32_t adjFieldLen = reqFieldLen;
                    if ( (typeValue==UDATPG_HOUR_FIELD && (options & UDATPG_MATCH_HOUR_FIELD_LENGTH)==0) ||
                         (typeValue==UDATPG_MINUTE_FIELD && (options & UDATPG_MATCH_MINUTE_FIELD_LENGTH)==0) ||
                         (typeValue==UDATPG_SECOND_FIELD && (options & UDATPG_MATCH_SECOND_FIELD_LENGTH)==0) ) {
                         adjFieldLen = field.length();
                    } else if (specifiedSkeleton) {
                        int32_t skelFieldLen = specifiedSkeleton->original.getFieldLength(typeValue);
                        UBool patFieldIsNumeric = (row->type > 0);
                        UBool skelFieldIsNumeric = (specifiedSkeleton->type[typeValue] > 0);
                        if (skelFieldLen == reqFieldLen || (patFieldIsNumeric && !skelFieldIsNumeric) || (skelFieldIsNumeric && !patFieldIsNumeric)) {
                            // don't adjust the field length in the found pattern
                            adjFieldLen = field.length();
                        }
                    }
                    UChar c = (typeValue!= UDATPG_HOUR_FIELD
                            && typeValue!= UDATPG_MONTH_FIELD
                            && typeValue!= UDATPG_WEEKDAY_FIELD
                            && (typeValue!= UDATPG_YEAR_FIELD || reqFieldChar==CAP_Y))
                            ? reqFieldChar
                            : field.charAt(0);
                    if (typeValue == UDATPG_HOUR_FIELD && (flags & kDTPGSkeletonUsesCapJ) != 0) {
                        c = fDefaultHourFormatChar;
                    }
                    field.remove();
                    for (int32_t i=adjFieldLen; i>0; --i) {
                        field+=c;
                    }
            }
            newPattern+=field;
        }
    }
    return newPattern;
}

UnicodeString
DateTimePatternGenerator::getBestAppending(int32_t missingFields, int32_t flags, UDateTimePatternMatchOptions options) {
    UnicodeString  resultPattern, tempPattern;
    UErrorCode err=U_ZERO_ERROR;
    int32_t lastMissingFieldMask=0;
    if (missingFields!=0) {
        resultPattern=UnicodeString();
        const PtnSkeleton* specifiedSkeleton=NULL;
        tempPattern = *getBestRaw(*dtMatcher, missingFields, distanceInfo, &specifiedSkeleton);
        resultPattern = adjustFieldTypes(tempPattern, specifiedSkeleton, flags, options);
        if ( distanceInfo->missingFieldMask==0 ) {
            return resultPattern;
        }
        while (distanceInfo->missingFieldMask!=0) { // precondition: EVERY single field must work!
            if ( lastMissingFieldMask == distanceInfo->missingFieldMask ) {
                break;  // cannot find the proper missing field
            }
            if (((distanceInfo->missingFieldMask & UDATPG_SECOND_AND_FRACTIONAL_MASK)==UDATPG_FRACTIONAL_MASK) &&
                ((missingFields & UDATPG_SECOND_AND_FRACTIONAL_MASK) == UDATPG_SECOND_AND_FRACTIONAL_MASK)) {
                resultPattern = adjustFieldTypes(resultPattern, specifiedSkeleton, flags | kDTPGFixFractionalSeconds, options);
                distanceInfo->missingFieldMask &= ~UDATPG_FRACTIONAL_MASK;
                continue;
            }
            int32_t startingMask = distanceInfo->missingFieldMask;
            tempPattern = *getBestRaw(*dtMatcher, distanceInfo->missingFieldMask, distanceInfo, &specifiedSkeleton);
            tempPattern = adjustFieldTypes(tempPattern, specifiedSkeleton, flags, options);
            int32_t foundMask=startingMask& ~distanceInfo->missingFieldMask;
            int32_t topField=getTopBitNumber(foundMask);
            UnicodeString appendName;
            getAppendName((UDateTimePatternField)topField, appendName);
            const UnicodeString *values[3] = {
                &resultPattern,
                &tempPattern,
                &appendName
            };
            SimpleFormatter(appendItemFormats[topField], 2, 3, err).
                    formatAndReplace(values, 3, resultPattern, NULL, 0, err);
            lastMissingFieldMask = distanceInfo->missingFieldMask;
        }
    }
    return resultPattern;
}

int32_t
DateTimePatternGenerator::getTopBitNumber(int32_t foundMask) {
    if ( foundMask==0 ) {
        return 0;
    }
    int32_t i=0;
    while (foundMask!=0) {
        foundMask >>=1;
        ++i;
    }
    if (i-1 >UDATPG_ZONE_FIELD) {
        return UDATPG_ZONE_FIELD;
    }
    else
        return i-1;
}

void
DateTimePatternGenerator::setAvailableFormat(const UnicodeString &key, UErrorCode& err)
{
    fAvailableFormatKeyHash->puti(key, 1, err);
}

UBool
DateTimePatternGenerator::isAvailableFormatSet(const UnicodeString &key) const {
    return (UBool)(fAvailableFormatKeyHash->geti(key) == 1);
}

void
DateTimePatternGenerator::copyHashtable(Hashtable *other, UErrorCode &status) {

    if (other == NULL) {
        return;
    }
    if (fAvailableFormatKeyHash != NULL) {
        delete fAvailableFormatKeyHash;
        fAvailableFormatKeyHash = NULL;
    }
    initHashtable(status);
    if(U_FAILURE(status)){
        return;
    }
    int32_t pos = UHASH_FIRST;
    const UHashElement* elem = NULL;
    // walk through the hash table and create a deep clone
    while((elem = other->nextElement(pos))!= NULL){
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        fAvailableFormatKeyHash->puti(*otherKey, 1, status);
        if(U_FAILURE(status)){
            return;
        }
    }
}

StringEnumeration*
DateTimePatternGenerator::getSkeletons(UErrorCode& status) const {
    StringEnumeration* skeletonEnumerator = new DTSkeletonEnumeration(*patternMap, DT_SKELETON, status);
    return skeletonEnumerator;
}

const UnicodeString&
DateTimePatternGenerator::getPatternForSkeleton(const UnicodeString& skeleton) const {
    PtnElem *curElem;

    if (skeleton.length() ==0) {
        return emptyString;
    }
    curElem = patternMap->getHeader(skeleton.charAt(0));
    while ( curElem != NULL ) {
        if ( curElem->skeleton->getSkeleton()==skeleton ) {
            return curElem->pattern;
        }
        curElem=curElem->next;
    }
    return emptyString;
}

StringEnumeration*
DateTimePatternGenerator::getBaseSkeletons(UErrorCode& status) const {
    StringEnumeration* baseSkeletonEnumerator = new DTSkeletonEnumeration(*patternMap, DT_BASESKELETON, status);
    return baseSkeletonEnumerator;
}

StringEnumeration*
DateTimePatternGenerator::getRedundants(UErrorCode& status) {
    StringEnumeration* output = new DTRedundantEnumeration();
    const UnicodeString *pattern;
    PatternMapIterator it;
    for (it.set(*patternMap); it.hasNext(); ) {
        DateTimeMatcher current = it.next();
        pattern = patternMap->getPatternFromSkeleton(*(it.getSkeleton()));
        if ( isCanonicalItem(*pattern) ) {
            continue;
        }
        if ( skipMatcher == NULL ) {
            skipMatcher = new DateTimeMatcher(current);
        }
        else {
            *skipMatcher = current;
        }
        UnicodeString trial = getBestPattern(current.getPattern(), status);
        if (trial == *pattern) {
            ((DTRedundantEnumeration *)output)->add(*pattern, status);
        }
        if (current.equals(skipMatcher)) {
            continue;
        }
    }
    return output;
}

UBool
DateTimePatternGenerator::isCanonicalItem(const UnicodeString& item) const {
    if ( item.length() != 1 ) {
        return FALSE;
    }
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i) {
        if (item.charAt(0)==Canonical_Items[i]) {
            return TRUE;
        }
    }
    return FALSE;
}


DateTimePatternGenerator*
DateTimePatternGenerator::clone() const {
    return new DateTimePatternGenerator(*this);
}

PatternMap::PatternMap() {
   for (int32_t i=0; i < MAX_PATTERN_ENTRIES; ++i ) {
      boot[i]=NULL;
   }
   isDupAllowed = TRUE;
}

void
PatternMap::copyFrom(const PatternMap& other, UErrorCode& status) {
    this->isDupAllowed = other.isDupAllowed;
    for (int32_t bootIndex=0; bootIndex<MAX_PATTERN_ENTRIES; ++bootIndex ) {
        PtnElem *curElem, *otherElem, *prevElem=NULL;
        otherElem = other.boot[bootIndex];
        while (otherElem!=NULL) {
            if ((curElem = new PtnElem(otherElem->basePattern, otherElem->pattern))==NULL) {
                // out of memory
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            if ( this->boot[bootIndex]== NULL ) {
                this->boot[bootIndex] = curElem;
            }
            if ((curElem->skeleton=new PtnSkeleton(*(otherElem->skeleton))) == NULL ) {
                // out of memory
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            curElem->skeletonWasSpecified = otherElem->skeletonWasSpecified;
            if (prevElem!=NULL) {
                prevElem->next=curElem;
            }
            curElem->next=NULL;
            prevElem = curElem;
            otherElem = otherElem->next;
        }

    }
}

PtnElem*
PatternMap::getHeader(UChar baseChar) {
    PtnElem* curElem;

    if ( (baseChar >= CAP_A) && (baseChar <= CAP_Z) ) {
         curElem = boot[baseChar-CAP_A];
    }
    else {
        if ( (baseChar >=LOW_A) && (baseChar <= LOW_Z) ) {
            curElem = boot[26+baseChar-LOW_A];
        }
        else {
            return NULL;
        }
    }
    return curElem;
}

PatternMap::~PatternMap() {
   for (int32_t i=0; i < MAX_PATTERN_ENTRIES; ++i ) {
       if (boot[i]!=NULL ) {
           delete boot[i];
           boot[i]=NULL;
       }
   }
}  // PatternMap destructor

void
PatternMap::add(const UnicodeString& basePattern,
                const PtnSkeleton& skeleton,
                const UnicodeString& value,// mapped pattern value
                UBool skeletonWasSpecified,
                UErrorCode &status) {
    UChar baseChar = basePattern.charAt(0);
    PtnElem *curElem, *baseElem;
    status = U_ZERO_ERROR;

    // the baseChar must be A-Z or a-z
    if ((baseChar >= CAP_A) && (baseChar <= CAP_Z)) {
        baseElem = boot[baseChar-CAP_A];
    }
    else {
        if ((baseChar >=LOW_A) && (baseChar <= LOW_Z)) {
            baseElem = boot[26+baseChar-LOW_A];
         }
         else {
             status = U_ILLEGAL_CHARACTER;
             return;
         }
    }

    if (baseElem == NULL) {
        if ((curElem = new PtnElem(basePattern, value)) == NULL ) {
            // out of memory
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if (baseChar >= LOW_A) {
            boot[26 + (baseChar-LOW_A)] = curElem;
        }
        else {
            boot[baseChar-CAP_A] = curElem;
        }
        curElem->skeleton = new PtnSkeleton(skeleton);
        curElem->skeletonWasSpecified = skeletonWasSpecified;
    }
    if ( baseElem != NULL ) {
        curElem = getDuplicateElem(basePattern, skeleton, baseElem);

        if (curElem == NULL) {
            // add new element to the list.
            curElem = baseElem;
            while( curElem -> next != NULL )
            {
                curElem = curElem->next;
            }
            if ((curElem->next = new PtnElem(basePattern, value)) == NULL ) {
                // out of memory
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            curElem=curElem->next;
            curElem->skeleton = new PtnSkeleton(skeleton);
            curElem->skeletonWasSpecified = skeletonWasSpecified;
        }
        else {
            // Pattern exists in the list already.
            if ( !isDupAllowed ) {
                return;
            }
            // Overwrite the value.
            curElem->pattern = value;
            // It was a bug that we were not doing the following previously,
            // though that bug hid other problems by making things partly work.
            curElem->skeletonWasSpecified = skeletonWasSpecified;
        }
    }
}  // PatternMap::add

// Find the pattern from the given basePattern string.
const UnicodeString *
PatternMap::getPatternFromBasePattern(UnicodeString& basePattern, UBool& skeletonWasSpecified) { // key to search for
   PtnElem *curElem;

   if ((curElem=getHeader(basePattern.charAt(0)))==NULL) {
       return NULL;  // no match
   }

   do  {
       if ( basePattern.compare(curElem->basePattern)==0 ) {
          skeletonWasSpecified = curElem->skeletonWasSpecified;
          return &(curElem->pattern);
       }
       curElem=curElem->next;
   }while (curElem != NULL);

   return NULL;
}  // PatternMap::getFromBasePattern


// Find the pattern from the given skeleton.
// At least when this is called from getBestRaw & addPattern (in which case specifiedSkeletonPtr is non-NULL),
// the comparison should be based on skeleton.original (which is unique and tied to the distance measurement in bestRaw)
// and not skeleton.baseOriginal (which is not unique); otherwise we may pick a different skeleton than the one with the
// optimum distance value in getBestRaw. When this is called from public getRedundants (specifiedSkeletonPtr is NULL),
// for now it will continue to compare based on baseOriginal so as not to change the behavior unnecessarily.
const UnicodeString *
PatternMap::getPatternFromSkeleton(PtnSkeleton& skeleton, const PtnSkeleton** specifiedSkeletonPtr) { // key to search for
   PtnElem *curElem;

   if (specifiedSkeletonPtr) {
       *specifiedSkeletonPtr = NULL;
   }

   // find boot entry
   UChar baseChar = skeleton.getFirstChar();
   if ((curElem=getHeader(baseChar))==NULL) {
       return NULL;  // no match
   }

   do  {
       UBool equal;
       if (specifiedSkeletonPtr != NULL) { // called from DateTimePatternGenerator::getBestRaw or addPattern, use original
           equal = curElem->skeleton->original == skeleton.original;
       } else { // called from DateTimePatternGenerator::getRedundants, use baseOriginal
           equal = curElem->skeleton->baseOriginal == skeleton.baseOriginal;
       }
       if (equal) {
           if (specifiedSkeletonPtr && curElem->skeletonWasSpecified) {
               *specifiedSkeletonPtr = curElem->skeleton;
           }
           return &(curElem->pattern);
       }
       curElem=curElem->next;
   }while (curElem != NULL);

   return NULL;
}

UBool
PatternMap::equals(const PatternMap& other) {
    if ( this==&other ) {
        return TRUE;
    }
    for (int32_t bootIndex=0; bootIndex<MAX_PATTERN_ENTRIES; ++bootIndex ) {
        if ( boot[bootIndex]==other.boot[bootIndex] ) {
            continue;
        }
        if ( (boot[bootIndex]==NULL)||(other.boot[bootIndex]==NULL) ) {
            return FALSE;
        }
        PtnElem *otherElem = other.boot[bootIndex];
        PtnElem *myElem = boot[bootIndex];
        while ((otherElem!=NULL) || (myElem!=NULL)) {
            if ( myElem == otherElem ) {
                break;
            }
            if ((otherElem==NULL) || (myElem==NULL)) {
                return FALSE;
            }
            if ( (myElem->basePattern != otherElem->basePattern) ||
                 (myElem->pattern != otherElem->pattern) ) {
                return FALSE;
            }
            if ((myElem->skeleton!=otherElem->skeleton)&&
                !myElem->skeleton->equals(*(otherElem->skeleton))) {
                return FALSE;
            }
            myElem = myElem->next;
            otherElem=otherElem->next;
        }
    }
    return TRUE;
}

// find any key existing in the mapping table already.
// return TRUE if there is an existing key, otherwise return FALSE.
PtnElem*
PatternMap::getDuplicateElem(
            const UnicodeString &basePattern,
            const PtnSkeleton &skeleton,
            PtnElem *baseElem)  {
   PtnElem *curElem;

   if ( baseElem == (PtnElem *)NULL )  {
         return (PtnElem*)NULL;
   }
   else {
         curElem = baseElem;
   }
   do {
     if ( basePattern.compare(curElem->basePattern)==0 ) {
        UBool isEqual=TRUE;
        for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i) {
            if (curElem->skeleton->type[i] != skeleton.type[i] ) {
                isEqual=FALSE;
                break;
            }
        }
        if (isEqual) {
            return curElem;
        }
     }
     curElem = curElem->next;
   } while( curElem != (PtnElem *)NULL );

   // end of the list
   return (PtnElem*)NULL;

}  // PatternMap::getDuplicateElem

DateTimeMatcher::DateTimeMatcher(void) {
}

DateTimeMatcher::~DateTimeMatcher() {}

DateTimeMatcher::DateTimeMatcher(const DateTimeMatcher& other) {
    copyFrom(other.skeleton);
}


void
DateTimeMatcher::set(const UnicodeString& pattern, FormatParser* fp) {
    PtnSkeleton localSkeleton;
    return set(pattern, fp, localSkeleton);
}

void
DateTimeMatcher::set(const UnicodeString& pattern, FormatParser* fp, PtnSkeleton& skeletonResult) {
    int32_t i;
    for (i=0; i<UDATPG_FIELD_COUNT; ++i) {
        skeletonResult.type[i] = NONE;
    }
    skeletonResult.original.clear();
    skeletonResult.baseOriginal.clear();
    skeletonResult.addedDefaultDayPeriod = FALSE;

    fp->set(pattern);
    for (i=0; i < fp->itemNumber; i++) {
        const UnicodeString& value = fp->items[i];
        // don't skip 'a' anymore, dayPeriod handled specially below

        if ( fp->isQuoteLiteral(value) ) {
            UnicodeString quoteLiteral;
            fp->getQuoteLiteral(quoteLiteral, &i);
            continue;
        }
        int32_t canonicalIndex = fp->getCanonicalIndex(value);
        if (canonicalIndex < 0 ) {
            continue;
        }
        const dtTypeElem *row = &dtTypes[canonicalIndex];
        int32_t field = row->field;
        skeletonResult.original.populate(field, value);
        UChar repeatChar = row->patternChar;
        int32_t repeatCount = row->minLen;
        skeletonResult.baseOriginal.populate(field, repeatChar, repeatCount);
        int16_t subField = row->type;
        if ( row->type > 0) {
            subField += value.length();
        }
        skeletonResult.type[field] = subField;
    }
    // #13183, handle special behavior for day period characters (a, b, B)
    if (!skeletonResult.original.isFieldEmpty(UDATPG_HOUR_FIELD)) {
        if (skeletonResult.original.getFieldChar(UDATPG_HOUR_FIELD)==LOW_H || skeletonResult.original.getFieldChar(UDATPG_HOUR_FIELD)==CAP_K) {
            // We have a skeleton with 12-hour-cycle format
            if (skeletonResult.original.isFieldEmpty(UDATPG_DAYPERIOD_FIELD)) {
                // But we do not have a day period in the skeleton; add the default DAYPERIOD (currently "a")
                for (i = 0; dtTypes[i].patternChar != 0; i++) {
                    if ( dtTypes[i].field == UDATPG_DAYPERIOD_FIELD ) {
                        // first entry for UDATPG_DAYPERIOD_FIELD
                        skeletonResult.original.populate(UDATPG_DAYPERIOD_FIELD, dtTypes[i].patternChar, dtTypes[i].minLen);
                        skeletonResult.baseOriginal.populate(UDATPG_DAYPERIOD_FIELD, dtTypes[i].patternChar, dtTypes[i].minLen);
                        skeletonResult.type[UDATPG_DAYPERIOD_FIELD] = dtTypes[i].type;
                        skeletonResult.addedDefaultDayPeriod = TRUE;
                        break;
                    }
                }
            }
        } else {
            // Skeleton has 24-hour-cycle hour format and has dayPeriod, delete dayPeriod (i.e. ignore it)
            skeletonResult.original.clearField(UDATPG_DAYPERIOD_FIELD);
            skeletonResult.baseOriginal.clearField(UDATPG_DAYPERIOD_FIELD);
            skeletonResult.type[UDATPG_DAYPERIOD_FIELD] = NONE;
        }
    }
    copyFrom(skeletonResult);
}

void
DateTimeMatcher::getBasePattern(UnicodeString &result ) {
    result.remove(); // Reset the result first.
    skeleton.baseOriginal.appendTo(result);
}

UnicodeString
DateTimeMatcher::getPattern() {
    UnicodeString result;
    return skeleton.original.appendTo(result);
}

int32_t
DateTimeMatcher::getDistance(const DateTimeMatcher& other, int32_t includeMask, DistanceInfo& distanceInfo) {
    int32_t result=0;
    distanceInfo.clear();
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i ) {
        int32_t myType = (includeMask&(1<<i))==0 ? 0 : skeleton.type[i];
        int32_t otherType = other.skeleton.type[i];
        if (myType==otherType) {
            continue;
        }
        if (myType==0) {// and other is not
            result += EXTRA_FIELD;
            distanceInfo.addExtra(i);
        }
        else {
            if (otherType==0) {
                result += MISSING_FIELD;
                distanceInfo.addMissing(i);
            }
            else {
                result += abs(myType - otherType);
            }
        }

    }
    return result;
}

void
DateTimeMatcher::copyFrom(const PtnSkeleton& newSkeleton) {
    skeleton.copyFrom(newSkeleton);
}

void
DateTimeMatcher::copyFrom() {
    // same as clear
    skeleton.clear();
}

UBool
DateTimeMatcher::equals(const DateTimeMatcher* other) const {
    if (other==NULL) { return FALSE; }
    return skeleton.original == other->skeleton.original;
}

int32_t
DateTimeMatcher::getFieldMask() {
    int32_t result=0;

    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i) {
        if (skeleton.type[i]!=0) {
            result |= (1<<i);
        }
    }
    return result;
}

PtnSkeleton*
DateTimeMatcher::getSkeletonPtr() {
    return &skeleton;
}

FormatParser::FormatParser () {
    status = START;
    itemNumber=0;
}


FormatParser::~FormatParser () {
}


// Find the next token with the starting position and length
// Note: the startPos may
FormatParser::TokenStatus
FormatParser::setTokens(const UnicodeString& pattern, int32_t startPos, int32_t *len) {
    int32_t  curLoc = startPos;
    if ( curLoc >= pattern.length()) {
        return DONE;
    }
    // check the current char is between A-Z or a-z
    do {
        UChar c=pattern.charAt(curLoc);
        if ( (c>=CAP_A && c<=CAP_Z) || (c>=LOW_A && c<=LOW_Z) ) {
           curLoc++;
        }
        else {
               startPos = curLoc;
               *len=1;
               return ADD_TOKEN;
        }

        if ( pattern.charAt(curLoc)!= pattern.charAt(startPos) ) {
            break;  // not the same token
        }
    } while(curLoc <= pattern.length());
    *len = curLoc-startPos;
    return ADD_TOKEN;
}

void
FormatParser::set(const UnicodeString& pattern) {
    int32_t startPos=0;
    TokenStatus result=START;
    int32_t len=0;
    itemNumber =0;

    do {
        result = setTokens( pattern, startPos, &len );
        if ( result == ADD_TOKEN )
        {
            items[itemNumber++] = UnicodeString(pattern, startPos, len );
            startPos += len;
        }
        else {
            break;
        }
    } while (result==ADD_TOKEN && itemNumber < MAX_DT_TOKEN);
}

int32_t
FormatParser::getCanonicalIndex(const UnicodeString& s, UBool strict) {
    int32_t len = s.length();
    if (len == 0) {
        return -1;
    }
    UChar ch = s.charAt(0);

    // Verify that all are the same character.
    for (int32_t l = 1; l < len; l++) {
        if (ch != s.charAt(l)) {
            return -1;
        }
    }
    int32_t i = 0;
    int32_t bestRow = -1;
    while (dtTypes[i].patternChar != 0x0000) {
        if ( dtTypes[i].patternChar != ch ) {
            ++i;
            continue;
        }
        bestRow = i;
        if (dtTypes[i].patternChar != dtTypes[i+1].patternChar) {
            return i;
        }
        if (dtTypes[i+1].minLen <= len) {
            ++i;
            continue;
        }
        return i;
    }
    return strict ? -1 : bestRow;
}

UBool
FormatParser::isQuoteLiteral(const UnicodeString& s) {
    return (UBool)(s.charAt(0)==SINGLE_QUOTE);
}

// This function aussumes the current itemIndex points to the quote literal.
// Please call isQuoteLiteral prior to this function.
void
FormatParser::getQuoteLiteral(UnicodeString& quote, int32_t *itemIndex) {
    int32_t i=*itemIndex;

    quote.remove();
    if (items[i].charAt(0)==SINGLE_QUOTE) {
        quote += items[i];
        ++i;
    }
    while ( i < itemNumber ) {
        if ( items[i].charAt(0)==SINGLE_QUOTE ) {
            if ( (i+1<itemNumber) && (items[i+1].charAt(0)==SINGLE_QUOTE)) {
                // two single quotes e.g. 'o''clock'
                quote += items[i++];
                quote += items[i++];
                continue;
            }
            else {
                quote += items[i];
                break;
            }
        }
        else {
            quote += items[i];
        }
        ++i;
    }
    *itemIndex=i;
}

UBool
FormatParser::isPatternSeparator(UnicodeString& field) {
    for (int32_t i=0; i<field.length(); ++i ) {
        UChar c= field.charAt(i);
        if ( (c==SINGLE_QUOTE) || (c==BACKSLASH) || (c==SPACE) || (c==COLON) ||
             (c==QUOTATION_MARK) || (c==COMMA) || (c==HYPHEN) ||(items[i].charAt(0)==DOT) ) {
            continue;
        }
        else {
            return FALSE;
        }
    }
    return TRUE;
}

DistanceInfo::~DistanceInfo() {}

void
DistanceInfo::setTo(DistanceInfo &other) {
    missingFieldMask = other.missingFieldMask;
    extraFieldMask= other.extraFieldMask;
}

PatternMapIterator::PatternMapIterator() {
    bootIndex = 0;
    nodePtr = NULL;
    patternMap=NULL;
    matcher= new DateTimeMatcher();
}


PatternMapIterator::~PatternMapIterator() {
    delete matcher;
}

void
PatternMapIterator::set(PatternMap& newPatternMap) {
    this->patternMap=&newPatternMap;
}

PtnSkeleton*
PatternMapIterator::getSkeleton() {
    if ( nodePtr == NULL ) {
        return NULL;
    }
    else {
        return nodePtr->skeleton;
    }
}

UBool
PatternMapIterator::hasNext() {
    int32_t headIndex=bootIndex;
    PtnElem *curPtr=nodePtr;

    if (patternMap==NULL) {
        return FALSE;
    }
    while ( headIndex < MAX_PATTERN_ENTRIES ) {
        if ( curPtr != NULL ) {
            if ( curPtr->next != NULL ) {
                return TRUE;
            }
            else {
                headIndex++;
                curPtr=NULL;
                continue;
            }
        }
        else {
            if ( patternMap->boot[headIndex] != NULL ) {
                return TRUE;
            }
            else {
                headIndex++;
                continue;
            }
        }

    }
    return FALSE;
}

DateTimeMatcher&
PatternMapIterator::next() {
    while ( bootIndex < MAX_PATTERN_ENTRIES ) {
        if ( nodePtr != NULL ) {
            if ( nodePtr->next != NULL ) {
                nodePtr = nodePtr->next;
                break;
            }
            else {
                bootIndex++;
                nodePtr=NULL;
                continue;
            }
        }
        else {
            if ( patternMap->boot[bootIndex] != NULL ) {
                nodePtr = patternMap->boot[bootIndex];
                break;
            }
            else {
                bootIndex++;
                continue;
            }
        }
    }
    if (nodePtr!=NULL) {
        matcher->copyFrom(*nodePtr->skeleton);
    }
    else {
        matcher->copyFrom();
    }
    return *matcher;
}


SkeletonFields::SkeletonFields() {
    // Set initial values to zero
    clear();
}

void SkeletonFields::clear() {
    uprv_memset(chars, 0, sizeof(chars));
    uprv_memset(lengths, 0, sizeof(lengths));
}

void SkeletonFields::copyFrom(const SkeletonFields& other) {
    uprv_memcpy(chars, other.chars, sizeof(chars));
    uprv_memcpy(lengths, other.lengths, sizeof(lengths));
}

void SkeletonFields::clearField(int32_t field) {
    chars[field] = 0;
    lengths[field] = 0;
}

UChar SkeletonFields::getFieldChar(int32_t field) const {
    return chars[field];
}

int32_t SkeletonFields::getFieldLength(int32_t field) const {
    return lengths[field];
}

void SkeletonFields::populate(int32_t field, const UnicodeString& value) {
    populate(field, value.charAt(0), value.length());
}

void SkeletonFields::populate(int32_t field, UChar ch, int32_t length) {
    chars[field] = (int8_t) ch;
    lengths[field] = (int8_t) length;
}

UBool SkeletonFields::isFieldEmpty(int32_t field) const {
    return lengths[field] == 0;
}

UnicodeString& SkeletonFields::appendTo(UnicodeString& string) const {
    for (int32_t i = 0; i < UDATPG_FIELD_COUNT; ++i) {
        appendFieldTo(i, string);
    }
    return string;
}

UnicodeString& SkeletonFields::appendFieldTo(int32_t field, UnicodeString& string) const {
    UChar ch(chars[field]);
    int32_t length = (int32_t) lengths[field];

    for (int32_t i=0; i<length; i++) {
        string += ch;
    }
    return string;
}

UChar SkeletonFields::getFirstChar() const {
    for (int32_t i = 0; i < UDATPG_FIELD_COUNT; ++i) {
        if (lengths[i] != 0) {
            return chars[i];
        }
    }
    return '\0';
}


PtnSkeleton::PtnSkeleton() {
}

PtnSkeleton::PtnSkeleton(const PtnSkeleton& other) {
    copyFrom(other);
}

void PtnSkeleton::copyFrom(const PtnSkeleton& other) {
    uprv_memcpy(type, other.type, sizeof(type));
    original.copyFrom(other.original);
    baseOriginal.copyFrom(other.baseOriginal);
}

void PtnSkeleton::clear() {
    uprv_memset(type, 0, sizeof(type));
    original.clear();
    baseOriginal.clear();
}

UBool
PtnSkeleton::equals(const PtnSkeleton& other) const  {
    return (original == other.original)
        && (baseOriginal == other.baseOriginal)
        && (uprv_memcmp(type, other.type, sizeof(type)) == 0);
}

UnicodeString
PtnSkeleton::getSkeleton() const {
    UnicodeString result;
    result = original.appendTo(result);
    int32_t pos;
    if (addedDefaultDayPeriod && (pos = result.indexOf(LOW_A)) >= 0) {
        // for backward compatibility: if DateTimeMatcher.set added a single 'a' that
        // was not in the provided skeleton, remove it here before returning skeleton.
        result.remove(pos, 1);
    }
    return result;
}

UnicodeString
PtnSkeleton::getBaseSkeleton() const {
    UnicodeString result;
    result = baseOriginal.appendTo(result);
    int32_t pos;
    if (addedDefaultDayPeriod && (pos = result.indexOf(LOW_A)) >= 0) {
        // for backward compatibility: if DateTimeMatcher.set added a single 'a' that
        // was not in the provided skeleton, remove it here before returning skeleton.
        result.remove(pos, 1);
    }
    return result;
}

UChar
PtnSkeleton::getFirstChar() const {
    return baseOriginal.getFirstChar();
}

PtnSkeleton::~PtnSkeleton() {
}

PtnElem::PtnElem(const UnicodeString &basePat, const UnicodeString &pat) :
basePattern(basePat),
skeleton(NULL),
pattern(pat),
next(NULL)
{
}

PtnElem::~PtnElem() {

    if (next!=NULL) {
        delete next;
    }
    delete skeleton;
}

DTSkeletonEnumeration::DTSkeletonEnumeration(PatternMap &patternMap, dtStrEnum type, UErrorCode& status) {
    PtnElem  *curElem;
    PtnSkeleton *curSkeleton;
    UnicodeString s;
    int32_t bootIndex;

    pos=0;
    fSkeletons = new UVector(status);
    if (U_FAILURE(status)) {
        delete fSkeletons;
        return;
    }
    for (bootIndex=0; bootIndex<MAX_PATTERN_ENTRIES; ++bootIndex ) {
        curElem = patternMap.boot[bootIndex];
        while (curElem!=NULL) {
            switch(type) {
                case DT_BASESKELETON:
                    s=curElem->basePattern;
                    break;
                case DT_PATTERN:
                    s=curElem->pattern;
                    break;
                case DT_SKELETON:
                    curSkeleton=curElem->skeleton;
                    s=curSkeleton->getSkeleton();
                    break;
            }
            if ( !isCanonicalItem(s) ) {
                fSkeletons->addElement(new UnicodeString(s), status);
                if (U_FAILURE(status)) {
                    delete fSkeletons;
                    fSkeletons = NULL;
                    return;
                }
            }
            curElem = curElem->next;
        }
    }
    if ((bootIndex==MAX_PATTERN_ENTRIES) && (curElem!=NULL) ) {
        status = U_BUFFER_OVERFLOW_ERROR;
    }
}

const UnicodeString*
DTSkeletonEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fSkeletons->size()) {
        return (const UnicodeString*)fSkeletons->elementAt(pos++);
    }
    return NULL;
}

void
DTSkeletonEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
DTSkeletonEnumeration::count(UErrorCode& /*status*/) const {
   return (fSkeletons==NULL) ? 0 : fSkeletons->size();
}

UBool
DTSkeletonEnumeration::isCanonicalItem(const UnicodeString& item) {
    if ( item.length() != 1 ) {
        return FALSE;
    }
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i) {
        if (item.charAt(0)==Canonical_Items[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

DTSkeletonEnumeration::~DTSkeletonEnumeration() {
    UnicodeString *s;
    for (int32_t i=0; i<fSkeletons->size(); ++i) {
        if ((s=(UnicodeString *)fSkeletons->elementAt(i))!=NULL) {
            delete s;
        }
    }
    delete fSkeletons;
}

DTRedundantEnumeration::DTRedundantEnumeration() {
    pos=0;
    fPatterns = NULL;
}

void
DTRedundantEnumeration::add(const UnicodeString& pattern, UErrorCode& status) {
    if (U_FAILURE(status)) return;
    if (fPatterns == NULL)  {
        fPatterns = new UVector(status);
        if (U_FAILURE(status)) {
            delete fPatterns;
            fPatterns = NULL;
            return;
       }
    }
    fPatterns->addElement(new UnicodeString(pattern), status);
    if (U_FAILURE(status)) {
        delete fPatterns;
        fPatterns = NULL;
        return;
    }
}

const UnicodeString*
DTRedundantEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fPatterns->size()) {
        return (const UnicodeString*)fPatterns->elementAt(pos++);
    }
    return NULL;
}

void
DTRedundantEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
DTRedundantEnumeration::count(UErrorCode& /*status*/) const {
       return (fPatterns==NULL) ? 0 : fPatterns->size();
}

UBool
DTRedundantEnumeration::isCanonicalItem(const UnicodeString& item) {
    if ( item.length() != 1 ) {
        return FALSE;
    }
    for (int32_t i=0; i<UDATPG_FIELD_COUNT; ++i) {
        if (item.charAt(0)==Canonical_Items[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

DTRedundantEnumeration::~DTRedundantEnumeration() {
    UnicodeString *s;
    for (int32_t i=0; i<fPatterns->size(); ++i) {
        if ((s=(UnicodeString *)fPatterns->elementAt(i))!=NULL) {
            delete s;
        }
    }
    delete fPatterns;
}

U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
