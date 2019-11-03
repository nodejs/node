// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2016, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File DTFMTSYM.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   07/21/98    stephen     Added getZoneIndex
*                            Changed weekdays/short weekdays to be one-based
*   06/14/99    stephen     Removed SimpleDateFormat::fgTimeZoneDataSuffix
*   11/16/99    weiv        Added 'Y' and 'e' to fgPatternChars
*   03/27/00    weiv        Keeping resource bundle around!
*   06/30/05    emmons      Added eraNames, narrow month/day, standalone context
*   10/12/05    emmons      Added setters for eraNames, month/day by width/context
*******************************************************************************
*/

#include <utility>

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#include "unicode/ustring.h"
#include "unicode/localpointer.h"
#include "unicode/dtfmtsym.h"
#include "unicode/smpdtfmt.h"
#include "unicode/msgfmt.h"
#include "unicode/numsys.h"
#include "unicode/tznames.h"
#include "cpputils.h"
#include "umutex.h"
#include "cmemory.h"
#include "cstring.h"
#include "charstr.h"
#include "dt_impl.h"
#include "locbased.h"
#include "gregoimp.h"
#include "hash.h"
#include "uassert.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "uvector.h"
#include "shareddateformatsymbols.h"
#include "unicode/calendar.h"
#include "unifiedcache.h"

// *****************************************************************************
// class DateFormatSymbols
// *****************************************************************************

/**
 * These are static arrays we use only in the case where we have no
 * resource data.
 */

#if UDAT_HAS_PATTERN_CHAR_FOR_TIME_SEPARATOR
#define PATTERN_CHARS_LEN 38
#else
#define PATTERN_CHARS_LEN 37
#endif

/**
 * Unlocalized date-time pattern characters. For example: 'y', 'd', etc. All
 * locales use the same these unlocalized pattern characters.
 */
static const UChar gPatternChars[] = {
    // if UDAT_HAS_PATTERN_CHAR_FOR_TIME_SEPARATOR:
    //   GyMdkHmsSEDFwWahKzYeugAZvcLQqVUOXxrbB:
    // else:
    //   GyMdkHmsSEDFwWahKzYeugAZvcLQqVUOXxrbB

    0x47, 0x79, 0x4D, 0x64, 0x6B, 0x48, 0x6D, 0x73, 0x53, 0x45,
    0x44, 0x46, 0x77, 0x57, 0x61, 0x68, 0x4B, 0x7A, 0x59, 0x65,
    0x75, 0x67, 0x41, 0x5A, 0x76, 0x63, 0x4c, 0x51, 0x71, 0x56,
    0x55, 0x4F, 0x58, 0x78, 0x72, 0x62, 0x42,
#if UDAT_HAS_PATTERN_CHAR_FOR_TIME_SEPARATOR
    0x3a,
#endif
    0
};

//------------------------------------------------------
// Strings of last resort.  These are only used if we have no resource
// files.  They aren't designed for actual use, just for backup.

// These are the month names and abbreviations of last resort.
static const UChar gLastResortMonthNames[13][3] =
{
    {0x0030, 0x0031, 0x0000}, /* "01" */
    {0x0030, 0x0032, 0x0000}, /* "02" */
    {0x0030, 0x0033, 0x0000}, /* "03" */
    {0x0030, 0x0034, 0x0000}, /* "04" */
    {0x0030, 0x0035, 0x0000}, /* "05" */
    {0x0030, 0x0036, 0x0000}, /* "06" */
    {0x0030, 0x0037, 0x0000}, /* "07" */
    {0x0030, 0x0038, 0x0000}, /* "08" */
    {0x0030, 0x0039, 0x0000}, /* "09" */
    {0x0031, 0x0030, 0x0000}, /* "10" */
    {0x0031, 0x0031, 0x0000}, /* "11" */
    {0x0031, 0x0032, 0x0000}, /* "12" */
    {0x0031, 0x0033, 0x0000}  /* "13" */
};

// These are the weekday names and abbreviations of last resort.
static const UChar gLastResortDayNames[8][2] =
{
    {0x0030, 0x0000}, /* "0" */
    {0x0031, 0x0000}, /* "1" */
    {0x0032, 0x0000}, /* "2" */
    {0x0033, 0x0000}, /* "3" */
    {0x0034, 0x0000}, /* "4" */
    {0x0035, 0x0000}, /* "5" */
    {0x0036, 0x0000}, /* "6" */
    {0x0037, 0x0000}  /* "7" */
};

// These are the quarter names and abbreviations of last resort.
static const UChar gLastResortQuarters[4][2] =
{
    {0x0031, 0x0000}, /* "1" */
    {0x0032, 0x0000}, /* "2" */
    {0x0033, 0x0000}, /* "3" */
    {0x0034, 0x0000}, /* "4" */
};

// These are the am/pm and BC/AD markers of last resort.
static const UChar gLastResortAmPmMarkers[2][3] =
{
    {0x0041, 0x004D, 0x0000}, /* "AM" */
    {0x0050, 0x004D, 0x0000}  /* "PM" */
};

static const UChar gLastResortEras[2][3] =
{
    {0x0042, 0x0043, 0x0000}, /* "BC" */
    {0x0041, 0x0044, 0x0000}  /* "AD" */
};

/* Sizes for the last resort string arrays */
typedef enum LastResortSize {
    kMonthNum = 13,
    kMonthLen = 3,

    kDayNum = 8,
    kDayLen = 2,

    kAmPmNum = 2,
    kAmPmLen = 3,

    kQuarterNum = 4,
    kQuarterLen = 2,

    kEraNum = 2,
    kEraLen = 3,

    kZoneNum = 5,
    kZoneLen = 4,

    kGmtHourNum = 4,
    kGmtHourLen = 10
} LastResortSize;

U_NAMESPACE_BEGIN

SharedDateFormatSymbols::~SharedDateFormatSymbols() {
}

template<> U_I18N_API
const SharedDateFormatSymbols *
        LocaleCacheKey<SharedDateFormatSymbols>::createObject(
                const void * /*unusedContext*/, UErrorCode &status) const {
    char type[256];
    Calendar::getCalendarTypeFromLocale(fLoc, type, UPRV_LENGTHOF(type), status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    SharedDateFormatSymbols *shared
            = new SharedDateFormatSymbols(fLoc, type, status);
    if (shared == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if (U_FAILURE(status)) {
        delete shared;
        return NULL;
    }
    shared->addRef();
    return shared;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateFormatSymbols)

#define kSUPPLEMENTAL "supplementalData"

/**
 * These are the tags we expect to see in normal resource bundle files associated
 * with a locale and calendar
 */
static const char gCalendarTag[]="calendar";
static const char gGregorianTag[]="gregorian";
static const char gErasTag[]="eras";
static const char gCyclicNameSetsTag[]="cyclicNameSets";
static const char gNameSetYearsTag[]="years";
static const char gNameSetZodiacsTag[]="zodiacs";
static const char gMonthNamesTag[]="monthNames";
static const char gMonthPatternsTag[]="monthPatterns";
static const char gDayNamesTag[]="dayNames";
static const char gNamesWideTag[]="wide";
static const char gNamesAbbrTag[]="abbreviated";
static const char gNamesShortTag[]="short";
static const char gNamesNarrowTag[]="narrow";
static const char gNamesAllTag[]="all";
static const char gNamesFormatTag[]="format";
static const char gNamesStandaloneTag[]="stand-alone";
static const char gNamesNumericTag[]="numeric";
static const char gAmPmMarkersTag[]="AmPmMarkers";
static const char gAmPmMarkersAbbrTag[]="AmPmMarkersAbbr";
static const char gAmPmMarkersNarrowTag[]="AmPmMarkersNarrow";
static const char gQuartersTag[]="quarters";
static const char gNumberElementsTag[]="NumberElements";
static const char gSymbolsTag[]="symbols";
static const char gTimeSeparatorTag[]="timeSeparator";
static const char gDayPeriodTag[]="dayPeriod";

// static const char gZoneStringsTag[]="zoneStrings";

// static const char gLocalPatternCharsTag[]="localPatternChars";

static const char gContextTransformsTag[]="contextTransforms";

/**
 * Jitterbug 2974: MSVC has a bug whereby new X[0] behaves badly.
 * Work around this.
 */
static inline UnicodeString* newUnicodeStringArray(size_t count) {
    return new UnicodeString[count ? count : 1];
}

//------------------------------------------------------

DateFormatSymbols * U_EXPORT2
DateFormatSymbols::createForLocale(
        const Locale& locale, UErrorCode &status) {
    const SharedDateFormatSymbols *shared = NULL;
    UnifiedCache::getByLocale(locale, shared, status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    DateFormatSymbols *result = new DateFormatSymbols(shared->get());
    shared->removeRef();
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    return result;
}

DateFormatSymbols::DateFormatSymbols(const Locale& locale,
                                     UErrorCode& status)
    : UObject()
{
  initializeData(locale, NULL,  status);
}

DateFormatSymbols::DateFormatSymbols(UErrorCode& status)
    : UObject()
{
  initializeData(Locale::getDefault(), NULL, status, TRUE);
}


DateFormatSymbols::DateFormatSymbols(const Locale& locale,
                                     const char *type,
                                     UErrorCode& status)
    : UObject()
{
  initializeData(locale, type,  status);
}

DateFormatSymbols::DateFormatSymbols(const char *type, UErrorCode& status)
    : UObject()
{
  initializeData(Locale::getDefault(), type, status, TRUE);
}

DateFormatSymbols::DateFormatSymbols(const DateFormatSymbols& other)
    : UObject(other)
{
    copyData(other);
}

void
DateFormatSymbols::assignArray(UnicodeString*& dstArray,
                               int32_t& dstCount,
                               const UnicodeString* srcArray,
                               int32_t srcCount)
{
    // assignArray() is only called by copyData() and initializeData(), which in turn
    // implements the copy constructor and the assignment operator.
    // All strings in a DateFormatSymbols object are created in one of the following
    // three ways that all allow to safely use UnicodeString::fastCopyFrom():
    // - readonly-aliases from resource bundles
    // - readonly-aliases or allocated strings from constants
    // - safely cloned strings (with owned buffers) from setXYZ() functions
    //
    // Note that this is true for as long as DateFormatSymbols can be constructed
    // only from a locale bundle or set via the cloning API,
    // *and* for as long as all the strings are in *private* fields, preventing
    // a subclass from creating these strings in an "unsafe" way (with respect to fastCopyFrom()).
    dstCount = srcCount;
    dstArray = newUnicodeStringArray(srcCount);
    if(dstArray != NULL) {
        int32_t i;
        for(i=0; i<srcCount; ++i) {
            dstArray[i].fastCopyFrom(srcArray[i]);
        }
    }
}

/**
 * Create a copy, in fZoneStrings, of the given zone strings array.  The
 * member variables fZoneStringsRowCount and fZoneStringsColCount should
 * be set already by the caller.
 */
void
DateFormatSymbols::createZoneStrings(const UnicodeString *const * otherStrings)
{
    int32_t row, col;
    UBool failed = FALSE;

    fZoneStrings = (UnicodeString **)uprv_malloc(fZoneStringsRowCount * sizeof(UnicodeString *));
    if (fZoneStrings != NULL) {
        for (row=0; row<fZoneStringsRowCount; ++row)
        {
            fZoneStrings[row] = newUnicodeStringArray(fZoneStringsColCount);
            if (fZoneStrings[row] == NULL) {
                failed = TRUE;
                break;
            }
            for (col=0; col<fZoneStringsColCount; ++col) {
                // fastCopyFrom() - see assignArray comments
                fZoneStrings[row][col].fastCopyFrom(otherStrings[row][col]);
            }
        }
    }
    // If memory allocation failed, roll back and delete fZoneStrings
    if (failed) {
        for (int i = row; i >= 0; i--) {
            delete[] fZoneStrings[i];
        }
        uprv_free(fZoneStrings);
        fZoneStrings = NULL;
    }
}

/**
 * Copy all of the other's data to this.
 */
void
DateFormatSymbols::copyData(const DateFormatSymbols& other) {
    UErrorCode status = U_ZERO_ERROR;
    U_LOCALE_BASED(locBased, *this);
    locBased.setLocaleIDs(
        other.getLocale(ULOC_VALID_LOCALE, status),
        other.getLocale(ULOC_ACTUAL_LOCALE, status));
    assignArray(fEras, fErasCount, other.fEras, other.fErasCount);
    assignArray(fEraNames, fEraNamesCount, other.fEraNames, other.fEraNamesCount);
    assignArray(fNarrowEras, fNarrowErasCount, other.fNarrowEras, other.fNarrowErasCount);
    assignArray(fMonths, fMonthsCount, other.fMonths, other.fMonthsCount);
    assignArray(fShortMonths, fShortMonthsCount, other.fShortMonths, other.fShortMonthsCount);
    assignArray(fNarrowMonths, fNarrowMonthsCount, other.fNarrowMonths, other.fNarrowMonthsCount);
    assignArray(fStandaloneMonths, fStandaloneMonthsCount, other.fStandaloneMonths, other.fStandaloneMonthsCount);
    assignArray(fStandaloneShortMonths, fStandaloneShortMonthsCount, other.fStandaloneShortMonths, other.fStandaloneShortMonthsCount);
    assignArray(fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount, other.fStandaloneNarrowMonths, other.fStandaloneNarrowMonthsCount);
    assignArray(fWeekdays, fWeekdaysCount, other.fWeekdays, other.fWeekdaysCount);
    assignArray(fShortWeekdays, fShortWeekdaysCount, other.fShortWeekdays, other.fShortWeekdaysCount);
    assignArray(fShorterWeekdays, fShorterWeekdaysCount, other.fShorterWeekdays, other.fShorterWeekdaysCount);
    assignArray(fNarrowWeekdays, fNarrowWeekdaysCount, other.fNarrowWeekdays, other.fNarrowWeekdaysCount);
    assignArray(fStandaloneWeekdays, fStandaloneWeekdaysCount, other.fStandaloneWeekdays, other.fStandaloneWeekdaysCount);
    assignArray(fStandaloneShortWeekdays, fStandaloneShortWeekdaysCount, other.fStandaloneShortWeekdays, other.fStandaloneShortWeekdaysCount);
    assignArray(fStandaloneShorterWeekdays, fStandaloneShorterWeekdaysCount, other.fStandaloneShorterWeekdays, other.fStandaloneShorterWeekdaysCount);
    assignArray(fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount, other.fStandaloneNarrowWeekdays, other.fStandaloneNarrowWeekdaysCount);
    assignArray(fAmPms, fAmPmsCount, other.fAmPms, other.fAmPmsCount);
    assignArray(fNarrowAmPms, fNarrowAmPmsCount, other.fNarrowAmPms, other.fNarrowAmPmsCount );
    fTimeSeparator.fastCopyFrom(other.fTimeSeparator);  // fastCopyFrom() - see assignArray comments
    assignArray(fQuarters, fQuartersCount, other.fQuarters, other.fQuartersCount);
    assignArray(fShortQuarters, fShortQuartersCount, other.fShortQuarters, other.fShortQuartersCount);
    assignArray(fStandaloneQuarters, fStandaloneQuartersCount, other.fStandaloneQuarters, other.fStandaloneQuartersCount);
    assignArray(fStandaloneShortQuarters, fStandaloneShortQuartersCount, other.fStandaloneShortQuarters, other.fStandaloneShortQuartersCount);
    assignArray(fWideDayPeriods, fWideDayPeriodsCount,
                other.fWideDayPeriods, other.fWideDayPeriodsCount);
    assignArray(fNarrowDayPeriods, fNarrowDayPeriodsCount,
                other.fNarrowDayPeriods, other.fNarrowDayPeriodsCount);
    assignArray(fAbbreviatedDayPeriods, fAbbreviatedDayPeriodsCount,
                other.fAbbreviatedDayPeriods, other.fAbbreviatedDayPeriodsCount);
    assignArray(fStandaloneWideDayPeriods, fStandaloneWideDayPeriodsCount,
                other.fStandaloneWideDayPeriods, other.fStandaloneWideDayPeriodsCount);
    assignArray(fStandaloneNarrowDayPeriods, fStandaloneNarrowDayPeriodsCount,
                other.fStandaloneNarrowDayPeriods, other.fStandaloneNarrowDayPeriodsCount);
    assignArray(fStandaloneAbbreviatedDayPeriods, fStandaloneAbbreviatedDayPeriodsCount,
                other.fStandaloneAbbreviatedDayPeriods, other.fStandaloneAbbreviatedDayPeriodsCount);
    if (other.fLeapMonthPatterns != NULL) {
        assignArray(fLeapMonthPatterns, fLeapMonthPatternsCount, other.fLeapMonthPatterns, other.fLeapMonthPatternsCount);
    } else {
        fLeapMonthPatterns = NULL;
        fLeapMonthPatternsCount = 0;
    }
    if (other.fShortYearNames != NULL) {
        assignArray(fShortYearNames, fShortYearNamesCount, other.fShortYearNames, other.fShortYearNamesCount);
    } else {
        fShortYearNames = NULL;
        fShortYearNamesCount = 0;
    }
    if (other.fShortZodiacNames != NULL) {
        assignArray(fShortZodiacNames, fShortZodiacNamesCount, other.fShortZodiacNames, other.fShortZodiacNamesCount);
    } else {
        fShortZodiacNames = NULL;
        fShortZodiacNamesCount = 0;
    }

    if (other.fZoneStrings != NULL) {
        fZoneStringsColCount = other.fZoneStringsColCount;
        fZoneStringsRowCount = other.fZoneStringsRowCount;
        createZoneStrings((const UnicodeString**)other.fZoneStrings);

    } else {
        fZoneStrings = NULL;
        fZoneStringsColCount = 0;
        fZoneStringsRowCount = 0;
    }
    fZSFLocale = other.fZSFLocale;
    // Other zone strings data is created on demand
    fLocaleZoneStrings = NULL;

    // fastCopyFrom() - see assignArray comments
    fLocalPatternChars.fastCopyFrom(other.fLocalPatternChars);

    uprv_memcpy(fCapitalization, other.fCapitalization, sizeof(fCapitalization));
}

/**
 * Assignment operator.
 */
DateFormatSymbols& DateFormatSymbols::operator=(const DateFormatSymbols& other)
{
    dispose();
    copyData(other);

    return *this;
}

DateFormatSymbols::~DateFormatSymbols()
{
    dispose();
}

void DateFormatSymbols::dispose()
{
    delete[] fEras;
    delete[] fEraNames;
    delete[] fNarrowEras;
    delete[] fMonths;
    delete[] fShortMonths;
    delete[] fNarrowMonths;
    delete[] fStandaloneMonths;
    delete[] fStandaloneShortMonths;
    delete[] fStandaloneNarrowMonths;
    delete[] fWeekdays;
    delete[] fShortWeekdays;
    delete[] fShorterWeekdays;
    delete[] fNarrowWeekdays;
    delete[] fStandaloneWeekdays;
    delete[] fStandaloneShortWeekdays;
    delete[] fStandaloneShorterWeekdays;
    delete[] fStandaloneNarrowWeekdays;
    delete[] fAmPms;
    delete[] fNarrowAmPms;
    delete[] fQuarters;
    delete[] fShortQuarters;
    delete[] fStandaloneQuarters;
    delete[] fStandaloneShortQuarters;
    delete[] fLeapMonthPatterns;
    delete[] fShortYearNames;
    delete[] fShortZodiacNames;
    delete[] fAbbreviatedDayPeriods;
    delete[] fWideDayPeriods;
    delete[] fNarrowDayPeriods;
    delete[] fStandaloneAbbreviatedDayPeriods;
    delete[] fStandaloneWideDayPeriods;
    delete[] fStandaloneNarrowDayPeriods;

    disposeZoneStrings();
}

void DateFormatSymbols::disposeZoneStrings()
{
    if (fZoneStrings) {
        for (int32_t row = 0; row < fZoneStringsRowCount; ++row) {
            delete[] fZoneStrings[row];
        }
        uprv_free(fZoneStrings);
    }
    if (fLocaleZoneStrings) {
        for (int32_t row = 0; row < fZoneStringsRowCount; ++row) {
            delete[] fLocaleZoneStrings[row];
        }
        uprv_free(fLocaleZoneStrings);
    }

    fZoneStrings = NULL;
    fLocaleZoneStrings = NULL;
    fZoneStringsRowCount = 0;
    fZoneStringsColCount = 0;
}

UBool
DateFormatSymbols::arrayCompare(const UnicodeString* array1,
                                const UnicodeString* array2,
                                int32_t count)
{
    if (array1 == array2) return TRUE;
    while (count>0)
    {
        --count;
        if (array1[count] != array2[count]) return FALSE;
    }
    return TRUE;
}

UBool
DateFormatSymbols::operator==(const DateFormatSymbols& other) const
{
    // First do cheap comparisons
    if (this == &other) {
        return TRUE;
    }
    if (fErasCount == other.fErasCount &&
        fEraNamesCount == other.fEraNamesCount &&
        fNarrowErasCount == other.fNarrowErasCount &&
        fMonthsCount == other.fMonthsCount &&
        fShortMonthsCount == other.fShortMonthsCount &&
        fNarrowMonthsCount == other.fNarrowMonthsCount &&
        fStandaloneMonthsCount == other.fStandaloneMonthsCount &&
        fStandaloneShortMonthsCount == other.fStandaloneShortMonthsCount &&
        fStandaloneNarrowMonthsCount == other.fStandaloneNarrowMonthsCount &&
        fWeekdaysCount == other.fWeekdaysCount &&
        fShortWeekdaysCount == other.fShortWeekdaysCount &&
        fShorterWeekdaysCount == other.fShorterWeekdaysCount &&
        fNarrowWeekdaysCount == other.fNarrowWeekdaysCount &&
        fStandaloneWeekdaysCount == other.fStandaloneWeekdaysCount &&
        fStandaloneShortWeekdaysCount == other.fStandaloneShortWeekdaysCount &&
        fStandaloneShorterWeekdaysCount == other.fStandaloneShorterWeekdaysCount &&
        fStandaloneNarrowWeekdaysCount == other.fStandaloneNarrowWeekdaysCount &&
        fAmPmsCount == other.fAmPmsCount &&
        fNarrowAmPmsCount == other.fNarrowAmPmsCount &&
        fQuartersCount == other.fQuartersCount &&
        fShortQuartersCount == other.fShortQuartersCount &&
        fStandaloneQuartersCount == other.fStandaloneQuartersCount &&
        fStandaloneShortQuartersCount == other.fStandaloneShortQuartersCount &&
        fLeapMonthPatternsCount == other.fLeapMonthPatternsCount &&
        fShortYearNamesCount == other.fShortYearNamesCount &&
        fShortZodiacNamesCount == other.fShortZodiacNamesCount &&
        fAbbreviatedDayPeriodsCount == other.fAbbreviatedDayPeriodsCount &&
        fWideDayPeriodsCount == other.fWideDayPeriodsCount &&
        fNarrowDayPeriodsCount == other.fNarrowDayPeriodsCount &&
        fStandaloneAbbreviatedDayPeriodsCount == other.fStandaloneAbbreviatedDayPeriodsCount &&
        fStandaloneWideDayPeriodsCount == other.fStandaloneWideDayPeriodsCount &&
        fStandaloneNarrowDayPeriodsCount == other.fStandaloneNarrowDayPeriodsCount &&
        (uprv_memcmp(fCapitalization, other.fCapitalization, sizeof(fCapitalization))==0))
    {
        // Now compare the arrays themselves
        if (arrayCompare(fEras, other.fEras, fErasCount) &&
            arrayCompare(fEraNames, other.fEraNames, fEraNamesCount) &&
            arrayCompare(fNarrowEras, other.fNarrowEras, fNarrowErasCount) &&
            arrayCompare(fMonths, other.fMonths, fMonthsCount) &&
            arrayCompare(fShortMonths, other.fShortMonths, fShortMonthsCount) &&
            arrayCompare(fNarrowMonths, other.fNarrowMonths, fNarrowMonthsCount) &&
            arrayCompare(fStandaloneMonths, other.fStandaloneMonths, fStandaloneMonthsCount) &&
            arrayCompare(fStandaloneShortMonths, other.fStandaloneShortMonths, fStandaloneShortMonthsCount) &&
            arrayCompare(fStandaloneNarrowMonths, other.fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount) &&
            arrayCompare(fWeekdays, other.fWeekdays, fWeekdaysCount) &&
            arrayCompare(fShortWeekdays, other.fShortWeekdays, fShortWeekdaysCount) &&
            arrayCompare(fShorterWeekdays, other.fShorterWeekdays, fShorterWeekdaysCount) &&
            arrayCompare(fNarrowWeekdays, other.fNarrowWeekdays, fNarrowWeekdaysCount) &&
            arrayCompare(fStandaloneWeekdays, other.fStandaloneWeekdays, fStandaloneWeekdaysCount) &&
            arrayCompare(fStandaloneShortWeekdays, other.fStandaloneShortWeekdays, fStandaloneShortWeekdaysCount) &&
            arrayCompare(fStandaloneShorterWeekdays, other.fStandaloneShorterWeekdays, fStandaloneShorterWeekdaysCount) &&
            arrayCompare(fStandaloneNarrowWeekdays, other.fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount) &&
            arrayCompare(fAmPms, other.fAmPms, fAmPmsCount) &&
            arrayCompare(fNarrowAmPms, other.fNarrowAmPms, fNarrowAmPmsCount) &&
            fTimeSeparator == other.fTimeSeparator &&
            arrayCompare(fQuarters, other.fQuarters, fQuartersCount) &&
            arrayCompare(fShortQuarters, other.fShortQuarters, fShortQuartersCount) &&
            arrayCompare(fStandaloneQuarters, other.fStandaloneQuarters, fStandaloneQuartersCount) &&
            arrayCompare(fStandaloneShortQuarters, other.fStandaloneShortQuarters, fStandaloneShortQuartersCount) &&
            arrayCompare(fLeapMonthPatterns, other.fLeapMonthPatterns, fLeapMonthPatternsCount) &&
            arrayCompare(fShortYearNames, other.fShortYearNames, fShortYearNamesCount) &&
            arrayCompare(fShortZodiacNames, other.fShortZodiacNames, fShortZodiacNamesCount) &&
            arrayCompare(fAbbreviatedDayPeriods, other.fAbbreviatedDayPeriods, fAbbreviatedDayPeriodsCount) &&
            arrayCompare(fWideDayPeriods, other.fWideDayPeriods, fWideDayPeriodsCount) &&
            arrayCompare(fNarrowDayPeriods, other.fNarrowDayPeriods, fNarrowDayPeriodsCount) &&
            arrayCompare(fStandaloneAbbreviatedDayPeriods, other.fStandaloneAbbreviatedDayPeriods,
                         fStandaloneAbbreviatedDayPeriodsCount) &&
            arrayCompare(fStandaloneWideDayPeriods, other.fStandaloneWideDayPeriods,
                         fStandaloneWideDayPeriodsCount) &&
            arrayCompare(fStandaloneNarrowDayPeriods, other.fStandaloneNarrowDayPeriods,
                         fStandaloneWideDayPeriodsCount))
        {
            // Compare the contents of fZoneStrings
            if (fZoneStrings == NULL && other.fZoneStrings == NULL) {
                if (fZSFLocale == other.fZSFLocale) {
                    return TRUE;
                }
            } else if (fZoneStrings != NULL && other.fZoneStrings != NULL) {
                if (fZoneStringsRowCount == other.fZoneStringsRowCount
                    && fZoneStringsColCount == other.fZoneStringsColCount) {
                    UBool cmpres = TRUE;
                    for (int32_t i = 0; (i < fZoneStringsRowCount) && cmpres; i++) {
                        cmpres = arrayCompare(fZoneStrings[i], other.fZoneStrings[i], fZoneStringsColCount);
                    }
                    return cmpres;
                }
            }
            return FALSE;
        }
    }
    return FALSE;
}

//------------------------------------------------------

const UnicodeString*
DateFormatSymbols::getEras(int32_t &count) const
{
    count = fErasCount;
    return fEras;
}

const UnicodeString*
DateFormatSymbols::getEraNames(int32_t &count) const
{
    count = fEraNamesCount;
    return fEraNames;
}

const UnicodeString*
DateFormatSymbols::getNarrowEras(int32_t &count) const
{
    count = fNarrowErasCount;
    return fNarrowEras;
}

const UnicodeString*
DateFormatSymbols::getMonths(int32_t &count) const
{
    count = fMonthsCount;
    return fMonths;
}

const UnicodeString*
DateFormatSymbols::getShortMonths(int32_t &count) const
{
    count = fShortMonthsCount;
    return fShortMonths;
}

const UnicodeString*
DateFormatSymbols::getMonths(int32_t &count, DtContextType context, DtWidthType width ) const
{
    UnicodeString *returnValue = NULL;

    switch (context) {
    case FORMAT :
        switch(width) {
        case WIDE :
            count = fMonthsCount;
            returnValue = fMonths;
            break;
        case ABBREVIATED :
        case SHORT : // no month data for this, defaults to ABBREVIATED
            count = fShortMonthsCount;
            returnValue = fShortMonths;
            break;
        case NARROW :
            count = fNarrowMonthsCount;
            returnValue = fNarrowMonths;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case STANDALONE :
        switch(width) {
        case WIDE :
            count = fStandaloneMonthsCount;
            returnValue = fStandaloneMonths;
            break;
        case ABBREVIATED :
        case SHORT : // no month data for this, defaults to ABBREVIATED
            count = fStandaloneShortMonthsCount;
            returnValue = fStandaloneShortMonths;
            break;
        case NARROW :
            count = fStandaloneNarrowMonthsCount;
            returnValue = fStandaloneNarrowMonths;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
    return returnValue;
}

const UnicodeString*
DateFormatSymbols::getWeekdays(int32_t &count) const
{
    count = fWeekdaysCount;
    return fWeekdays;
}

const UnicodeString*
DateFormatSymbols::getShortWeekdays(int32_t &count) const
{
    count = fShortWeekdaysCount;
    return fShortWeekdays;
}

const UnicodeString*
DateFormatSymbols::getWeekdays(int32_t &count, DtContextType context, DtWidthType width) const
{
    UnicodeString *returnValue = NULL;
    switch (context) {
    case FORMAT :
        switch(width) {
            case WIDE :
                count = fWeekdaysCount;
                returnValue = fWeekdays;
                break;
            case ABBREVIATED :
                count = fShortWeekdaysCount;
                returnValue = fShortWeekdays;
                break;
            case SHORT :
                count = fShorterWeekdaysCount;
                returnValue = fShorterWeekdays;
                break;
            case NARROW :
                count = fNarrowWeekdaysCount;
                returnValue = fNarrowWeekdays;
                break;
            case DT_WIDTH_COUNT :
                break;
        }
        break;
    case STANDALONE :
        switch(width) {
            case WIDE :
                count = fStandaloneWeekdaysCount;
                returnValue = fStandaloneWeekdays;
                break;
            case ABBREVIATED :
                count = fStandaloneShortWeekdaysCount;
                returnValue = fStandaloneShortWeekdays;
                break;
            case SHORT :
                count = fStandaloneShorterWeekdaysCount;
                returnValue = fStandaloneShorterWeekdays;
                break;
            case NARROW :
                count = fStandaloneNarrowWeekdaysCount;
                returnValue = fStandaloneNarrowWeekdays;
                break;
            case DT_WIDTH_COUNT :
                break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
    return returnValue;
}

const UnicodeString*
DateFormatSymbols::getQuarters(int32_t &count, DtContextType context, DtWidthType width ) const
{
    UnicodeString *returnValue = NULL;

    switch (context) {
    case FORMAT :
        switch(width) {
        case WIDE :
            count = fQuartersCount;
            returnValue = fQuarters;
            break;
        case ABBREVIATED :
        case SHORT : // no quarter data for this, defaults to ABBREVIATED
            count = fShortQuartersCount;
            returnValue = fShortQuarters;
            break;
        case NARROW :
            count = 0;
            returnValue = NULL;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case STANDALONE :
        switch(width) {
        case WIDE :
            count = fStandaloneQuartersCount;
            returnValue = fStandaloneQuarters;
            break;
        case ABBREVIATED :
        case SHORT : // no quarter data for this, defaults to ABBREVIATED
            count = fStandaloneShortQuartersCount;
            returnValue = fStandaloneShortQuarters;
            break;
        case NARROW :
            count = 0;
            returnValue = NULL;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
    return returnValue;
}

UnicodeString&
DateFormatSymbols::getTimeSeparatorString(UnicodeString& result) const
{
    // fastCopyFrom() - see assignArray comments
    return result.fastCopyFrom(fTimeSeparator);
}

const UnicodeString*
DateFormatSymbols::getAmPmStrings(int32_t &count) const
{
    count = fAmPmsCount;
    return fAmPms;
}

const UnicodeString*
DateFormatSymbols::getLeapMonthPatterns(int32_t &count) const
{
    count = fLeapMonthPatternsCount;
    return fLeapMonthPatterns;
}

const UnicodeString*
DateFormatSymbols::getYearNames(int32_t& count,
                                DtContextType /*ignored*/, DtWidthType /*ignored*/) const
{
    count = fShortYearNamesCount;
    return fShortYearNames;
}

void
DateFormatSymbols::setYearNames(const UnicodeString* yearNames, int32_t count,
                                DtContextType context, DtWidthType width)
{
    if (context == FORMAT && width == ABBREVIATED) {
        if (fShortYearNames) {
            delete[] fShortYearNames;
        }
        fShortYearNames = newUnicodeStringArray(count);
        uprv_arrayCopy(yearNames, fShortYearNames, count);
        fShortYearNamesCount = count;
    }
}

const UnicodeString*
DateFormatSymbols::getZodiacNames(int32_t& count,
                                DtContextType /*ignored*/, DtWidthType /*ignored*/) const
{
    count = fShortZodiacNamesCount;
    return fShortZodiacNames;
}

void
DateFormatSymbols::setZodiacNames(const UnicodeString* zodiacNames, int32_t count,
                                DtContextType context, DtWidthType width)
{
    if (context == FORMAT && width == ABBREVIATED) {
        if (fShortZodiacNames) {
            delete[] fShortZodiacNames;
        }
        fShortZodiacNames = newUnicodeStringArray(count);
        uprv_arrayCopy(zodiacNames, fShortZodiacNames, count);
        fShortZodiacNamesCount = count;
    }
}

//------------------------------------------------------

void
DateFormatSymbols::setEras(const UnicodeString* erasArray, int32_t count)
{
    // delete the old list if we own it
    if (fEras)
        delete[] fEras;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fEras = newUnicodeStringArray(count);
    uprv_arrayCopy(erasArray,fEras,  count);
    fErasCount = count;
}

void
DateFormatSymbols::setEraNames(const UnicodeString* eraNamesArray, int32_t count)
{
    // delete the old list if we own it
    if (fEraNames)
        delete[] fEraNames;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fEraNames = newUnicodeStringArray(count);
    uprv_arrayCopy(eraNamesArray,fEraNames,  count);
    fEraNamesCount = count;
}

void
DateFormatSymbols::setNarrowEras(const UnicodeString* narrowErasArray, int32_t count)
{
    // delete the old list if we own it
    if (fNarrowEras)
        delete[] fNarrowEras;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fNarrowEras = newUnicodeStringArray(count);
    uprv_arrayCopy(narrowErasArray,fNarrowEras,  count);
    fNarrowErasCount = count;
}

void
DateFormatSymbols::setMonths(const UnicodeString* monthsArray, int32_t count)
{
    // delete the old list if we own it
    if (fMonths)
        delete[] fMonths;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fMonths = newUnicodeStringArray(count);
    uprv_arrayCopy( monthsArray,fMonths,count);
    fMonthsCount = count;
}

void
DateFormatSymbols::setShortMonths(const UnicodeString* shortMonthsArray, int32_t count)
{
    // delete the old list if we own it
    if (fShortMonths)
        delete[] fShortMonths;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fShortMonths = newUnicodeStringArray(count);
    uprv_arrayCopy(shortMonthsArray,fShortMonths,  count);
    fShortMonthsCount = count;
}

void
DateFormatSymbols::setMonths(const UnicodeString* monthsArray, int32_t count, DtContextType context, DtWidthType width)
{
    // delete the old list if we own it
    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)

    switch (context) {
    case FORMAT :
        switch (width) {
        case WIDE :
            if (fMonths)
                delete[] fMonths;
            fMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fMonths,count);
            fMonthsCount = count;
            break;
        case ABBREVIATED :
            if (fShortMonths)
                delete[] fShortMonths;
            fShortMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fShortMonths,count);
            fShortMonthsCount = count;
            break;
        case NARROW :
            if (fNarrowMonths)
                delete[] fNarrowMonths;
            fNarrowMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fNarrowMonths,count);
            fNarrowMonthsCount = count;
            break;
        default :
            break;
        }
        break;
    case STANDALONE :
        switch (width) {
        case WIDE :
            if (fStandaloneMonths)
                delete[] fStandaloneMonths;
            fStandaloneMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fStandaloneMonths,count);
            fStandaloneMonthsCount = count;
            break;
        case ABBREVIATED :
            if (fStandaloneShortMonths)
                delete[] fStandaloneShortMonths;
            fStandaloneShortMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fStandaloneShortMonths,count);
            fStandaloneShortMonthsCount = count;
            break;
        case NARROW :
           if (fStandaloneNarrowMonths)
                delete[] fStandaloneNarrowMonths;
            fStandaloneNarrowMonths = newUnicodeStringArray(count);
            uprv_arrayCopy( monthsArray,fStandaloneNarrowMonths,count);
            fStandaloneNarrowMonthsCount = count;
            break;
        default :
            break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
}

void DateFormatSymbols::setWeekdays(const UnicodeString* weekdaysArray, int32_t count)
{
    // delete the old list if we own it
    if (fWeekdays)
        delete[] fWeekdays;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fWeekdays = newUnicodeStringArray(count);
    uprv_arrayCopy(weekdaysArray,fWeekdays,count);
    fWeekdaysCount = count;
}

void
DateFormatSymbols::setShortWeekdays(const UnicodeString* shortWeekdaysArray, int32_t count)
{
    // delete the old list if we own it
    if (fShortWeekdays)
        delete[] fShortWeekdays;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fShortWeekdays = newUnicodeStringArray(count);
    uprv_arrayCopy(shortWeekdaysArray, fShortWeekdays, count);
    fShortWeekdaysCount = count;
}

void
DateFormatSymbols::setWeekdays(const UnicodeString* weekdaysArray, int32_t count, DtContextType context, DtWidthType width)
{
    // delete the old list if we own it
    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)

    switch (context) {
    case FORMAT :
        switch (width) {
        case WIDE :
            if (fWeekdays)
                delete[] fWeekdays;
            fWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fWeekdays, count);
            fWeekdaysCount = count;
            break;
        case ABBREVIATED :
            if (fShortWeekdays)
                delete[] fShortWeekdays;
            fShortWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fShortWeekdays, count);
            fShortWeekdaysCount = count;
            break;
        case SHORT :
            if (fShorterWeekdays)
                delete[] fShorterWeekdays;
            fShorterWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fShorterWeekdays, count);
            fShorterWeekdaysCount = count;
            break;
        case NARROW :
            if (fNarrowWeekdays)
                delete[] fNarrowWeekdays;
            fNarrowWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fNarrowWeekdays, count);
            fNarrowWeekdaysCount = count;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case STANDALONE :
        switch (width) {
        case WIDE :
            if (fStandaloneWeekdays)
                delete[] fStandaloneWeekdays;
            fStandaloneWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fStandaloneWeekdays, count);
            fStandaloneWeekdaysCount = count;
            break;
        case ABBREVIATED :
            if (fStandaloneShortWeekdays)
                delete[] fStandaloneShortWeekdays;
            fStandaloneShortWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fStandaloneShortWeekdays, count);
            fStandaloneShortWeekdaysCount = count;
            break;
        case SHORT :
            if (fStandaloneShorterWeekdays)
                delete[] fStandaloneShorterWeekdays;
            fStandaloneShorterWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fStandaloneShorterWeekdays, count);
            fStandaloneShorterWeekdaysCount = count;
            break;
        case NARROW :
            if (fStandaloneNarrowWeekdays)
                delete[] fStandaloneNarrowWeekdays;
            fStandaloneNarrowWeekdays = newUnicodeStringArray(count);
            uprv_arrayCopy(weekdaysArray, fStandaloneNarrowWeekdays, count);
            fStandaloneNarrowWeekdaysCount = count;
            break;
        case DT_WIDTH_COUNT :
            break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
}

void
DateFormatSymbols::setQuarters(const UnicodeString* quartersArray, int32_t count, DtContextType context, DtWidthType width)
{
    // delete the old list if we own it
    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)

    switch (context) {
    case FORMAT :
        switch (width) {
        case WIDE :
            if (fQuarters)
                delete[] fQuarters;
            fQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fQuarters,count);
            fQuartersCount = count;
            break;
        case ABBREVIATED :
            if (fShortQuarters)
                delete[] fShortQuarters;
            fShortQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fShortQuarters,count);
            fShortQuartersCount = count;
            break;
        case NARROW :
        /*
            if (fNarrowQuarters)
                delete[] fNarrowQuarters;
            fNarrowQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fNarrowQuarters,count);
            fNarrowQuartersCount = count;
        */
            break;
        default :
            break;
        }
        break;
    case STANDALONE :
        switch (width) {
        case WIDE :
            if (fStandaloneQuarters)
                delete[] fStandaloneQuarters;
            fStandaloneQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fStandaloneQuarters,count);
            fStandaloneQuartersCount = count;
            break;
        case ABBREVIATED :
            if (fStandaloneShortQuarters)
                delete[] fStandaloneShortQuarters;
            fStandaloneShortQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fStandaloneShortQuarters,count);
            fStandaloneShortQuartersCount = count;
            break;
        case NARROW :
        /*
           if (fStandaloneNarrowQuarters)
                delete[] fStandaloneNarrowQuarters;
            fStandaloneNarrowQuarters = newUnicodeStringArray(count);
            uprv_arrayCopy( quartersArray,fStandaloneNarrowQuarters,count);
            fStandaloneNarrowQuartersCount = count;
        */
            break;
        default :
            break;
        }
        break;
    case DT_CONTEXT_COUNT :
        break;
    }
}

void
DateFormatSymbols::setAmPmStrings(const UnicodeString* amPmsArray, int32_t count)
{
    // delete the old list if we own it
    if (fAmPms) delete[] fAmPms;

    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fAmPms = newUnicodeStringArray(count);
    uprv_arrayCopy(amPmsArray,fAmPms,count);
    fAmPmsCount = count;
}

void
DateFormatSymbols::setTimeSeparatorString(const UnicodeString& newTimeSeparator)
{
    fTimeSeparator = newTimeSeparator;
}

const UnicodeString**
DateFormatSymbols::getZoneStrings(int32_t& rowCount, int32_t& columnCount) const
{
    const UnicodeString **result = NULL;
    static UMutex LOCK;

    umtx_lock(&LOCK);
    if (fZoneStrings == NULL) {
        if (fLocaleZoneStrings == NULL) {
            ((DateFormatSymbols*)this)->initZoneStringsArray();
        }
        result = (const UnicodeString**)fLocaleZoneStrings;
    } else {
        result = (const UnicodeString**)fZoneStrings;
    }
    rowCount = fZoneStringsRowCount;
    columnCount = fZoneStringsColCount;
    umtx_unlock(&LOCK);

    return result;
}

// For now, we include all zones
#define ZONE_SET UCAL_ZONE_TYPE_ANY

// This code must be called within a synchronized block
void
DateFormatSymbols::initZoneStringsArray(void) {
    if (fZoneStrings != NULL || fLocaleZoneStrings != NULL) {
        return;
    }

    UErrorCode status = U_ZERO_ERROR;

    StringEnumeration *tzids = NULL;
    UnicodeString ** zarray = NULL;
    TimeZoneNames *tzNames = NULL;
    int32_t rows = 0;

    static const UTimeZoneNameType TYPES[] = {
        UTZNM_LONG_STANDARD, UTZNM_SHORT_STANDARD,
        UTZNM_LONG_DAYLIGHT, UTZNM_SHORT_DAYLIGHT
    };
    static const int32_t NUM_TYPES = 4;

    do { // dummy do-while

        tzids = TimeZone::createTimeZoneIDEnumeration(ZONE_SET, NULL, NULL, status);
        rows = tzids->count(status);
        if (U_FAILURE(status)) {
            break;
        }

        // Allocate array
        int32_t size = rows * sizeof(UnicodeString*);
        zarray = (UnicodeString**)uprv_malloc(size);
        if (zarray == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            break;
        }
        uprv_memset(zarray, 0, size);

        tzNames = TimeZoneNames::createInstance(fZSFLocale, status);
        tzNames->loadAllDisplayNames(status);
        if (U_FAILURE(status)) { break; }

        const UnicodeString *tzid;
        int32_t i = 0;
        UDate now = Calendar::getNow();
        UnicodeString tzDispName;

        while ((tzid = tzids->snext(status)) != 0) {
            if (U_FAILURE(status)) {
                break;
            }

            zarray[i] = new UnicodeString[5];
            if (zarray[i] == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                break;
            }

            zarray[i][0].setTo(*tzid);
            tzNames->getDisplayNames(*tzid, TYPES, NUM_TYPES, now, zarray[i]+1, status);
            i++;
        }

    } while (FALSE);

    if (U_FAILURE(status)) {
        if (zarray) {
            for (int32_t i = 0; i < rows; i++) {
                if (zarray[i]) {
                    delete[] zarray[i];
                }
            }
            uprv_free(zarray);
            zarray = NULL;
        }
    }

    if (tzNames) {
        delete tzNames;
    }
    if (tzids) {
        delete tzids;
    }

    fLocaleZoneStrings = zarray;
    fZoneStringsRowCount = rows;
    fZoneStringsColCount = 1 + NUM_TYPES;
}

void
DateFormatSymbols::setZoneStrings(const UnicodeString* const *strings, int32_t rowCount, int32_t columnCount)
{
    // since deleting a 2-d array is a pain in the butt, we offload that task to
    // a separate function
    disposeZoneStrings();
    // we always own the new list, which we create here (we duplicate rather
    // than adopting the list passed in)
    fZoneStringsRowCount = rowCount;
    fZoneStringsColCount = columnCount;
    createZoneStrings((const UnicodeString**)strings);
}

//------------------------------------------------------

const char16_t * U_EXPORT2
DateFormatSymbols::getPatternUChars(void)
{
    return gPatternChars;
}

UDateFormatField U_EXPORT2
DateFormatSymbols::getPatternCharIndex(UChar c) {
    const UChar *p = u_strchr(gPatternChars, c);
    if (p == NULL) {
        return UDAT_FIELD_COUNT;
    } else {
        return static_cast<UDateFormatField>(p - gPatternChars);
    }
}

static const uint64_t kNumericFieldsAlways =
    ((uint64_t)1 << UDAT_YEAR_FIELD) |                      // y
    ((uint64_t)1 << UDAT_DATE_FIELD) |                      // d
    ((uint64_t)1 << UDAT_HOUR_OF_DAY1_FIELD) |              // k
    ((uint64_t)1 << UDAT_HOUR_OF_DAY0_FIELD) |              // H
    ((uint64_t)1 << UDAT_MINUTE_FIELD) |                    // m
    ((uint64_t)1 << UDAT_SECOND_FIELD) |                    // s
    ((uint64_t)1 << UDAT_FRACTIONAL_SECOND_FIELD) |         // S
    ((uint64_t)1 << UDAT_DAY_OF_YEAR_FIELD) |               // D
    ((uint64_t)1 << UDAT_DAY_OF_WEEK_IN_MONTH_FIELD) |      // F
    ((uint64_t)1 << UDAT_WEEK_OF_YEAR_FIELD) |              // w
    ((uint64_t)1 << UDAT_WEEK_OF_MONTH_FIELD) |             // W
    ((uint64_t)1 << UDAT_HOUR1_FIELD) |                     // h
    ((uint64_t)1 << UDAT_HOUR0_FIELD) |                     // K
    ((uint64_t)1 << UDAT_YEAR_WOY_FIELD) |                  // Y
    ((uint64_t)1 << UDAT_EXTENDED_YEAR_FIELD) |             // u
    ((uint64_t)1 << UDAT_JULIAN_DAY_FIELD) |                // g
    ((uint64_t)1 << UDAT_MILLISECONDS_IN_DAY_FIELD) |       // A
    ((uint64_t)1 << UDAT_RELATED_YEAR_FIELD);               // r

static const uint64_t kNumericFieldsForCount12 =
    ((uint64_t)1 << UDAT_MONTH_FIELD) |                     // M or MM
    ((uint64_t)1 << UDAT_DOW_LOCAL_FIELD) |                 // e or ee
    ((uint64_t)1 << UDAT_STANDALONE_DAY_FIELD) |            // c or cc
    ((uint64_t)1 << UDAT_STANDALONE_MONTH_FIELD) |          // L or LL
    ((uint64_t)1 << UDAT_QUARTER_FIELD) |                   // Q or QQ
    ((uint64_t)1 << UDAT_STANDALONE_QUARTER_FIELD);         // q or qq

UBool U_EXPORT2
DateFormatSymbols::isNumericField(UDateFormatField f, int32_t count) {
    if (f == UDAT_FIELD_COUNT) {
        return FALSE;
    }
    uint64_t flag = ((uint64_t)1 << f);
    return ((kNumericFieldsAlways & flag) != 0 || ((kNumericFieldsForCount12 & flag) != 0 && count < 3));
}

UBool U_EXPORT2
DateFormatSymbols::isNumericPatternChar(UChar c, int32_t count) {
    return isNumericField(getPatternCharIndex(c), count);
}

//------------------------------------------------------

UnicodeString&
DateFormatSymbols::getLocalPatternChars(UnicodeString& result) const
{
    // fastCopyFrom() - see assignArray comments
    return result.fastCopyFrom(fLocalPatternChars);
}

//------------------------------------------------------

void
DateFormatSymbols::setLocalPatternChars(const UnicodeString& newLocalPatternChars)
{
    fLocalPatternChars = newLocalPatternChars;
}

//------------------------------------------------------

namespace {

// Constants declarations
static const UChar kCalendarAliasPrefixUChar[] = {
    SOLIDUS, CAP_L, CAP_O, CAP_C, CAP_A, CAP_L, CAP_E, SOLIDUS,
    LOW_C, LOW_A, LOW_L, LOW_E, LOW_N, LOW_D, LOW_A, LOW_R, SOLIDUS
};
static const UChar kGregorianTagUChar[] = {
    LOW_G, LOW_R, LOW_E, LOW_G, LOW_O, LOW_R, LOW_I, LOW_A, LOW_N
};
static const UChar kVariantTagUChar[] = {
    PERCENT, LOW_V, LOW_A, LOW_R, LOW_I, LOW_A, LOW_N, LOW_T
};
static const UChar kLeapTagUChar[] = {
    LOW_L, LOW_E, LOW_A, LOW_P
};
static const UChar kCyclicNameSetsTagUChar[] = {
    LOW_C, LOW_Y, LOW_C, LOW_L, LOW_I, LOW_C, CAP_N, LOW_A, LOW_M, LOW_E, CAP_S, LOW_E, LOW_T, LOW_S
};
static const UChar kYearsTagUChar[] = {
    SOLIDUS, LOW_Y, LOW_E, LOW_A, LOW_R, LOW_S
};
static const UChar kZodiacsUChar[] = {
    SOLIDUS, LOW_Z, LOW_O, LOW_D, LOW_I, LOW_A, LOW_C, LOW_S
};
static const UChar kDayPartsTagUChar[] = {
    SOLIDUS, LOW_D, LOW_A, LOW_Y, CAP_P, LOW_A, LOW_R, LOW_T, LOW_S
};
static const UChar kFormatTagUChar[] = {
    SOLIDUS, LOW_F, LOW_O, LOW_R, LOW_M, LOW_A, LOW_T
};
static const UChar kAbbrTagUChar[] = {
    SOLIDUS, LOW_A, LOW_B, LOW_B, LOW_R, LOW_E, LOW_V, LOW_I, LOW_A, LOW_T, LOW_E, LOW_D
};

// ResourceSink to enumerate all calendar resources
struct CalendarDataSink : public ResourceSink {

    // Enum which specifies the type of alias received, or no alias
    enum AliasType {
        SAME_CALENDAR,
        DIFFERENT_CALENDAR,
        GREGORIAN,
        NONE
    };

    // Data structures to store resources from the current resource bundle
    Hashtable arrays;
    Hashtable arraySizes;
    Hashtable maps;
    /** 
     * Whenever there are aliases, the same object will be added twice to 'map'.
     * To avoid double deletion, 'maps' won't take ownership of the objects. Instead,
     * 'mapRefs' will own them and will delete them when CalendarDataSink is deleted.
     */
    MemoryPool<Hashtable> mapRefs;

    // Paths and the aliases they point to
    UVector aliasPathPairs;

    // Current and next calendar resource table which should be loaded
    UnicodeString currentCalendarType;
    UnicodeString nextCalendarType;

    // Resources to visit when enumerating fallback calendars
    LocalPointer<UVector> resourcesToVisit;

    // Alias' relative path populated whenever an alias is read
    UnicodeString aliasRelativePath;

    // Initializes CalendarDataSink with default values
    CalendarDataSink(UErrorCode& status)
    :   arrays(FALSE, status), arraySizes(FALSE, status), maps(FALSE, status),
        mapRefs(),
        aliasPathPairs(uprv_deleteUObject, uhash_compareUnicodeString, status),
        currentCalendarType(), nextCalendarType(),
        resourcesToVisit(NULL), aliasRelativePath() {
        if (U_FAILURE(status)) { return; }
    }
    virtual ~CalendarDataSink();

    // Configure the CalendarSink to visit all the resources
    void visitAllResources() {
        resourcesToVisit.adoptInstead(NULL);
    }

    // Actions to be done before enumerating
    void preEnumerate(const UnicodeString &calendarType) {
        currentCalendarType = calendarType;
        nextCalendarType.setToBogus();
        aliasPathPairs.removeAllElements();
    }

    virtual void put(const char *key, ResourceValue &value, UBool, UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) { return; }
        U_ASSERT(!currentCalendarType.isEmpty());

        // Stores the resources to visit on the next calendar.
        LocalPointer<UVector> resourcesToVisitNext(NULL);
        ResourceTable calendarData = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }

        // Enumerate all resources for this calendar
        for (int i = 0; calendarData.getKeyAndValue(i, key, value); i++) {
            UnicodeString keyUString(key, -1, US_INV);

            // == Handle aliases ==
            AliasType aliasType = processAliasFromValue(keyUString, value, errorCode);
            if (U_FAILURE(errorCode)) { return; }
            if (aliasType == GREGORIAN) {
                // Ignore aliases to the gregorian calendar, all of its resources will be loaded anyway.
                continue;

            } else if (aliasType == DIFFERENT_CALENDAR) {
                // Whenever an alias to the next calendar (except gregorian) is encountered, register the
                // calendar type it's pointing to
                if (resourcesToVisitNext.isNull()) {
                    resourcesToVisitNext
                        .adoptInsteadAndCheckErrorCode(new UVector(uprv_deleteUObject, uhash_compareUnicodeString, errorCode),
                                                       errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                }
                LocalPointer<UnicodeString> aliasRelativePathCopy(new UnicodeString(aliasRelativePath), errorCode);
                resourcesToVisitNext->addElement(aliasRelativePathCopy.getAlias(), errorCode);
                if (U_FAILURE(errorCode)) { return; }
                // Only release ownership after resourcesToVisitNext takes it (no error happened):
                aliasRelativePathCopy.orphan();
                continue;

            } else if (aliasType == SAME_CALENDAR) {
                // Register same-calendar alias
                if (arrays.get(aliasRelativePath) == NULL && maps.get(aliasRelativePath) == NULL) {
                    LocalPointer<UnicodeString> aliasRelativePathCopy(new UnicodeString(aliasRelativePath), errorCode);
                    aliasPathPairs.addElement(aliasRelativePathCopy.getAlias(), errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                    // Only release ownership after aliasPathPairs takes it (no error happened):
                    aliasRelativePathCopy.orphan();
                    LocalPointer<UnicodeString> keyUStringCopy(new UnicodeString(keyUString), errorCode);
                    aliasPathPairs.addElement(keyUStringCopy.getAlias(), errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                    // Only release ownership after aliasPathPairs takes it (no error happened):
                    keyUStringCopy.orphan();
                }
                continue;
            }

            // Only visit the resources that were referenced by an alias on the previous calendar
            // (AmPmMarkersAbbr is an exception).
            if (!resourcesToVisit.isNull() && !resourcesToVisit->isEmpty() && !resourcesToVisit->contains(&keyUString)
                && uprv_strcmp(key, gAmPmMarkersAbbrTag) != 0) { continue; }

            // == Handle data ==
            if (uprv_strcmp(key, gAmPmMarkersTag) == 0
                || uprv_strcmp(key, gAmPmMarkersAbbrTag) == 0
                || uprv_strcmp(key, gAmPmMarkersNarrowTag) == 0) {
                if (arrays.get(keyUString) == NULL) {
                    ResourceArray resourceArray = value.getArray(errorCode);
                    int32_t arraySize = resourceArray.getSize();
                    LocalArray<UnicodeString> stringArray(new UnicodeString[arraySize], errorCode);
                    value.getStringArray(stringArray.getAlias(), arraySize, errorCode);
                    arrays.put(keyUString, stringArray.orphan(), errorCode);
                    arraySizes.puti(keyUString, arraySize, errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                }
            } else if (uprv_strcmp(key, gErasTag) == 0
                       || uprv_strcmp(key, gDayNamesTag) == 0
                       || uprv_strcmp(key, gMonthNamesTag) == 0
                       || uprv_strcmp(key, gQuartersTag) == 0
                       || uprv_strcmp(key, gDayPeriodTag) == 0
                       || uprv_strcmp(key, gMonthPatternsTag) == 0
                       || uprv_strcmp(key, gCyclicNameSetsTag) == 0) {
                processResource(keyUString, key, value, errorCode);
            }
        }

        // Apply same-calendar aliases
        UBool modified;
        do {
            modified = false;
            for (int32_t i = 0; i < aliasPathPairs.size();) {
                UBool mod = false;
                UnicodeString *alias = (UnicodeString*)aliasPathPairs[i];
                UnicodeString *aliasArray;
                Hashtable *aliasMap;
                if ((aliasArray = (UnicodeString*)arrays.get(*alias)) != NULL) {
                    UnicodeString *path = (UnicodeString*)aliasPathPairs[i + 1];
                    if (arrays.get(*path) == NULL) {
                        // Clone the array
                        int32_t aliasArraySize = arraySizes.geti(*alias);
                        LocalArray<UnicodeString> aliasArrayCopy(new UnicodeString[aliasArraySize], errorCode);
                        if (U_FAILURE(errorCode)) { return; }
                        uprv_arrayCopy(aliasArray, aliasArrayCopy.getAlias(), aliasArraySize);
                        // Put the array on the 'arrays' map
                        arrays.put(*path, aliasArrayCopy.orphan(), errorCode);
                        arraySizes.puti(*path, aliasArraySize, errorCode);
                    }
                    if (U_FAILURE(errorCode)) { return; }
                    mod = true;
                } else if ((aliasMap = (Hashtable*)maps.get(*alias)) != NULL) {
                    UnicodeString *path = (UnicodeString*)aliasPathPairs[i + 1];
                    if (maps.get(*path) == NULL) {
                        maps.put(*path, aliasMap, errorCode);
                    }
                    if (U_FAILURE(errorCode)) { return; }
                    mod = true;
                }
                if (mod) {
                    aliasPathPairs.removeElementAt(i + 1);
                    aliasPathPairs.removeElementAt(i);
                    modified = true;
                } else {
                    i += 2;
                }
            }
        } while (modified && !aliasPathPairs.isEmpty());

        // Set the resources to visit on the next calendar
        if (!resourcesToVisitNext.isNull()) {
            resourcesToVisit = std::move(resourcesToVisitNext);
        }
    }

    // Process the nested resource bundle tables
    void processResource(UnicodeString &path, const char *key, ResourceValue &value, UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) return;

        ResourceTable table = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) return;
        Hashtable* stringMap = NULL;

        // Iterate over all the elements of the table and add them to the map
        for (int i = 0; table.getKeyAndValue(i, key, value); i++) {
            UnicodeString keyUString(key, -1, US_INV);

            // Ignore '%variant' keys
            if (keyUString.endsWith(kVariantTagUChar, UPRV_LENGTHOF(kVariantTagUChar))) {
                continue;
            }

            // == Handle String elements ==
            if (value.getType() == URES_STRING) {
                // We are on a leaf, store the map elements into the stringMap
                if (i == 0) {
                    // mapRefs will keep ownership of 'stringMap':
                    stringMap = mapRefs.create(FALSE, errorCode);
                    if (stringMap == NULL) {
                        errorCode = U_MEMORY_ALLOCATION_ERROR;
                        return;
                    }
                    maps.put(path, stringMap, errorCode);
                    if (U_FAILURE(errorCode)) { return; }
                    stringMap->setValueDeleter(uprv_deleteUObject);
                }
                U_ASSERT(stringMap != NULL);
                int32_t valueStringSize;
                const UChar *valueString = value.getString(valueStringSize, errorCode);
                if (U_FAILURE(errorCode)) { return; }
                LocalPointer<UnicodeString> valueUString(new UnicodeString(TRUE, valueString, valueStringSize), errorCode);
                stringMap->put(keyUString, valueUString.orphan(), errorCode);
                if (U_FAILURE(errorCode)) { return; }
                continue;
            }
            U_ASSERT(stringMap == NULL);

            // Store the current path's length and append the current key to the path.
            int32_t pathLength = path.length();
            path.append(SOLIDUS).append(keyUString);

            // In cyclicNameSets ignore everything but years/format/abbreviated
            // and zodiacs/format/abbreviated
            if (path.startsWith(kCyclicNameSetsTagUChar, UPRV_LENGTHOF(kCyclicNameSetsTagUChar))) {
                UBool skip = TRUE;
                int32_t startIndex = UPRV_LENGTHOF(kCyclicNameSetsTagUChar);
                int32_t length = 0;
                if (startIndex == path.length()
                    || path.compare(startIndex, (length = UPRV_LENGTHOF(kZodiacsUChar)), kZodiacsUChar, 0, UPRV_LENGTHOF(kZodiacsUChar)) == 0
                    || path.compare(startIndex, (length = UPRV_LENGTHOF(kYearsTagUChar)), kYearsTagUChar, 0, UPRV_LENGTHOF(kYearsTagUChar)) == 0
                    || path.compare(startIndex, (length = UPRV_LENGTHOF(kDayPartsTagUChar)), kDayPartsTagUChar, 0, UPRV_LENGTHOF(kDayPartsTagUChar)) == 0) {
                    startIndex += length;
                    length = 0;
                    if (startIndex == path.length()
                        || path.compare(startIndex, (length = UPRV_LENGTHOF(kFormatTagUChar)), kFormatTagUChar, 0, UPRV_LENGTHOF(kFormatTagUChar)) == 0) {
                        startIndex += length;
                        length = 0;
                        if (startIndex == path.length()
                            || path.compare(startIndex, (length = UPRV_LENGTHOF(kAbbrTagUChar)), kAbbrTagUChar, 0, UPRV_LENGTHOF(kAbbrTagUChar)) == 0) {
                            skip = FALSE;
                        }
                    }
                }
                if (skip) {
                    // Drop the latest key on the path and continue
                    path.retainBetween(0, pathLength);
                    continue;
                }
            }

            // == Handle aliases ==
            if (arrays.get(path) != NULL || maps.get(path) != NULL) {
                // Drop the latest key on the path and continue
                path.retainBetween(0, pathLength);
                continue;
            }

            AliasType aliasType = processAliasFromValue(path, value, errorCode);
            if (U_FAILURE(errorCode)) { return; }
            if (aliasType == SAME_CALENDAR) {
                // Store the alias path and the current path on aliasPathPairs
                LocalPointer<UnicodeString> aliasRelativePathCopy(new UnicodeString(aliasRelativePath), errorCode);
                aliasPathPairs.addElement(aliasRelativePathCopy.getAlias(), errorCode);
                if (U_FAILURE(errorCode)) { return; }
                // Only release ownership after aliasPathPairs takes it (no error happened):
                aliasRelativePathCopy.orphan();
                LocalPointer<UnicodeString> pathCopy(new UnicodeString(path), errorCode);
                aliasPathPairs.addElement(pathCopy.getAlias(), errorCode);
                if (U_FAILURE(errorCode)) { return; }
                // Only release ownership after aliasPathPairs takes it (no error happened):
                pathCopy.orphan();

                // Drop the latest key on the path and continue
                path.retainBetween(0, pathLength);
                continue;
            }
            U_ASSERT(aliasType == NONE);

            // == Handle data ==
            if (value.getType() == URES_ARRAY) {
                // We are on a leaf, store the array
                ResourceArray rDataArray = value.getArray(errorCode);
                int32_t dataArraySize = rDataArray.getSize();
                LocalArray<UnicodeString> dataArray(new UnicodeString[dataArraySize], errorCode);
                value.getStringArray(dataArray.getAlias(), dataArraySize, errorCode);
                arrays.put(path, dataArray.orphan(), errorCode);
                arraySizes.puti(path, dataArraySize, errorCode);
                if (U_FAILURE(errorCode)) { return; }
            } else if (value.getType() == URES_TABLE) {
                // We are not on a leaf, recursively process the subtable.
                processResource(path, key, value, errorCode);
                if (U_FAILURE(errorCode)) { return; }
            }

            // Drop the latest key on the path
            path.retainBetween(0, pathLength);
        }
    }

    // Populates an AliasIdentifier with the alias information contained on the UResource.Value.
    AliasType processAliasFromValue(UnicodeString &currentRelativePath, ResourceValue &value,
                                    UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) { return NONE; }

        if (value.getType() == URES_ALIAS) {
            int32_t aliasPathSize;
            const UChar* aliasPathUChar = value.getAliasString(aliasPathSize, errorCode);
            if (U_FAILURE(errorCode)) { return NONE; }
            UnicodeString aliasPath(aliasPathUChar, aliasPathSize);
            const int32_t aliasPrefixLength = UPRV_LENGTHOF(kCalendarAliasPrefixUChar);
            if (aliasPath.startsWith(kCalendarAliasPrefixUChar, aliasPrefixLength)
                && aliasPath.length() > aliasPrefixLength) {
                int32_t typeLimit = aliasPath.indexOf(SOLIDUS, aliasPrefixLength);
                if (typeLimit > aliasPrefixLength) {
                    const UnicodeString aliasCalendarType =
                        aliasPath.tempSubStringBetween(aliasPrefixLength, typeLimit);
                    aliasRelativePath.setTo(aliasPath, typeLimit + 1, aliasPath.length());

                    if (currentCalendarType == aliasCalendarType
                        && currentRelativePath != aliasRelativePath) {
                        // If we have an alias to the same calendar, the path to the resource must be different
                        return SAME_CALENDAR;

                    } else if (currentCalendarType != aliasCalendarType
                               && currentRelativePath == aliasRelativePath) {
                        // If we have an alias to a different calendar, the path to the resource must be the same
                        if (aliasCalendarType.compare(kGregorianTagUChar, UPRV_LENGTHOF(kGregorianTagUChar)) == 0) {
                            return GREGORIAN;
                        } else if (nextCalendarType.isBogus()) {
                            nextCalendarType = aliasCalendarType;
                            return DIFFERENT_CALENDAR;
                        } else if (nextCalendarType == aliasCalendarType) {
                            return DIFFERENT_CALENDAR;
                        }
                    }
                }
            }
            errorCode = U_INTERNAL_PROGRAM_ERROR;
            return NONE;
        }
        return NONE;
    }

    // Deleter function to be used by 'arrays'
    static void U_CALLCONV deleteUnicodeStringArray(void *uArray) {
        delete[] static_cast<UnicodeString *>(uArray);
    }
};
// Virtual destructors have to be defined out of line
CalendarDataSink::~CalendarDataSink() {
    arrays.setValueDeleter(deleteUnicodeStringArray);
}
}

//------------------------------------------------------

static void
initField(UnicodeString **field, int32_t& length, const UChar *data, LastResortSize numStr, LastResortSize strLen, UErrorCode &status) {
    if (U_SUCCESS(status)) {
        length = numStr;
        *field = newUnicodeStringArray((size_t)numStr);
        if (*field) {
            for(int32_t i = 0; i<length; i++) {
                // readonly aliases - all "data" strings are constant
                // -1 as length for variable-length strings (gLastResortDayNames[0] is empty)
                (*(field)+i)->setTo(TRUE, data+(i*((int32_t)strLen)), -1);
            }
        }
        else {
            length = 0;
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
}

static void
initField(UnicodeString **field, int32_t& length, CalendarDataSink &sink, CharString &key, UErrorCode &status) {
    if (U_SUCCESS(status)) {
        UnicodeString keyUString(key.data(), -1, US_INV);
        UnicodeString* array = static_cast<UnicodeString*>(sink.arrays.get(keyUString));

        if (array != NULL) {
            length = sink.arraySizes.geti(keyUString);
            *field = array;
            // DateFormatSymbols takes ownership of the array:
            sink.arrays.remove(keyUString);
        } else {
            length = 0;
            status = U_MISSING_RESOURCE_ERROR;
        }
    }
}

static void
initField(UnicodeString **field, int32_t& length, CalendarDataSink &sink, CharString &key, int32_t arrayOffset, UErrorCode &status) {
    if (U_SUCCESS(status)) {
        UnicodeString keyUString(key.data(), -1, US_INV);
        UnicodeString* array = static_cast<UnicodeString*>(sink.arrays.get(keyUString));

        if (array != NULL) {
            int32_t arrayLength = sink.arraySizes.geti(keyUString);
            length = arrayLength + arrayOffset;
            *field = new UnicodeString[length];
            if (*field == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            uprv_arrayCopy(array, 0, *field, arrayOffset, arrayLength);
        } else {
            length = 0;
            status = U_MISSING_RESOURCE_ERROR;
        }
    }
}

static void
initLeapMonthPattern(UnicodeString *field, int32_t index, CalendarDataSink &sink, CharString &path, UErrorCode &status) {
    field[index].remove();
    if (U_SUCCESS(status)) {
        UnicodeString pathUString(path.data(), -1, US_INV);
        Hashtable *leapMonthTable = static_cast<Hashtable*>(sink.maps.get(pathUString));
        if (leapMonthTable != NULL) {
            UnicodeString leapLabel(FALSE, kLeapTagUChar, UPRV_LENGTHOF(kLeapTagUChar));
            UnicodeString *leapMonthPattern = static_cast<UnicodeString*>(leapMonthTable->get(leapLabel));
            if (leapMonthPattern != NULL) {
                field[index].fastCopyFrom(*leapMonthPattern);
            } else {
                field[index].setToBogus();
            }
            return;
        }
        status = U_MISSING_RESOURCE_ERROR;
    }
}

static CharString
&buildResourcePath(CharString &path, const char* segment1, UErrorCode &errorCode) {
    return path.clear().append(segment1, -1, errorCode);
}

static CharString
&buildResourcePath(CharString &path, const char* segment1, const char* segment2,
                   UErrorCode &errorCode) {
    return buildResourcePath(path, segment1, errorCode).append('/', errorCode)
                                                       .append(segment2, -1, errorCode);
}

static CharString
&buildResourcePath(CharString &path, const char* segment1, const char* segment2,
                   const char* segment3, UErrorCode &errorCode) {
    return buildResourcePath(path, segment1, segment2, errorCode).append('/', errorCode)
                                                                 .append(segment3, -1, errorCode);
}

static CharString
&buildResourcePath(CharString &path, const char* segment1, const char* segment2,
                   const char* segment3, const char* segment4, UErrorCode &errorCode) {
    return buildResourcePath(path, segment1, segment2, segment3, errorCode).append('/', errorCode)
                                                                           .append(segment4, -1, errorCode);
}

typedef struct {
    const char * usageTypeName;
    DateFormatSymbols::ECapitalizationContextUsageType usageTypeEnumValue;
} ContextUsageTypeNameToEnumValue;

static const ContextUsageTypeNameToEnumValue contextUsageTypeMap[] = {
   // Entries must be sorted by usageTypeName; entry with NULL name terminates list.
    { "day-format-except-narrow", DateFormatSymbols::kCapContextUsageDayFormat },
    { "day-narrow",     DateFormatSymbols::kCapContextUsageDayNarrow },
    { "day-standalone-except-narrow", DateFormatSymbols::kCapContextUsageDayStandalone },
    { "era-abbr",       DateFormatSymbols::kCapContextUsageEraAbbrev },
    { "era-name",       DateFormatSymbols::kCapContextUsageEraWide },
    { "era-narrow",     DateFormatSymbols::kCapContextUsageEraNarrow },
    { "metazone-long",  DateFormatSymbols::kCapContextUsageMetazoneLong },
    { "metazone-short", DateFormatSymbols::kCapContextUsageMetazoneShort },
    { "month-format-except-narrow", DateFormatSymbols::kCapContextUsageMonthFormat },
    { "month-narrow",   DateFormatSymbols::kCapContextUsageMonthNarrow },
    { "month-standalone-except-narrow", DateFormatSymbols::kCapContextUsageMonthStandalone },
    { "zone-long",      DateFormatSymbols::kCapContextUsageZoneLong },
    { "zone-short",     DateFormatSymbols::kCapContextUsageZoneShort },
    { NULL, (DateFormatSymbols::ECapitalizationContextUsageType)0 },
};

// Resource keys to look up localized strings for day periods.
// The first one must be midnight and the second must be noon, so that their indices coincide
// with the am/pm field. Formatting and parsing code for day periods relies on this coincidence.
static const char *dayPeriodKeys[] = {"midnight", "noon",
                         "morning1", "afternoon1", "evening1", "night1",
                         "morning2", "afternoon2", "evening2", "night2"};

UnicodeString* loadDayPeriodStrings(CalendarDataSink &sink, CharString &path,
                                    int32_t &stringCount,  UErrorCode &status) {
    if (U_FAILURE(status)) { return NULL; }

    UnicodeString pathUString(path.data(), -1, US_INV);
    Hashtable* map = static_cast<Hashtable*>(sink.maps.get(pathUString));

    stringCount = UPRV_LENGTHOF(dayPeriodKeys);
    UnicodeString *strings = new UnicodeString[stringCount];
    if (strings == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    if (map != NULL) {
        for (int32_t i = 0; i < stringCount; ++i) {
            UnicodeString dayPeriodKey(dayPeriodKeys[i], -1, US_INV);
            UnicodeString *dayPeriod = static_cast<UnicodeString*>(map->get(dayPeriodKey));
            if (dayPeriod != NULL) {
                strings[i].fastCopyFrom(*dayPeriod);
            } else {
                strings[i].setToBogus();
            }
        }
    } else {
        for (int32_t i = 0; i < stringCount; i++) {
            strings[i].setToBogus();
        }
    }
    return strings;
}


void
DateFormatSymbols::initializeData(const Locale& locale, const char *type, UErrorCode& status, UBool useLastResortData)
{
    int32_t len = 0;
    /* In case something goes wrong, initialize all of the data to NULL. */
    fEras = NULL;
    fErasCount = 0;
    fEraNames = NULL;
    fEraNamesCount = 0;
    fNarrowEras = NULL;
    fNarrowErasCount = 0;
    fMonths = NULL;
    fMonthsCount=0;
    fShortMonths = NULL;
    fShortMonthsCount=0;
    fNarrowMonths = NULL;
    fNarrowMonthsCount=0;
    fStandaloneMonths = NULL;
    fStandaloneMonthsCount=0;
    fStandaloneShortMonths = NULL;
    fStandaloneShortMonthsCount=0;
    fStandaloneNarrowMonths = NULL;
    fStandaloneNarrowMonthsCount=0;
    fWeekdays = NULL;
    fWeekdaysCount=0;
    fShortWeekdays = NULL;
    fShortWeekdaysCount=0;
    fShorterWeekdays = NULL;
    fShorterWeekdaysCount=0;
    fNarrowWeekdays = NULL;
    fNarrowWeekdaysCount=0;
    fStandaloneWeekdays = NULL;
    fStandaloneWeekdaysCount=0;
    fStandaloneShortWeekdays = NULL;
    fStandaloneShortWeekdaysCount=0;
    fStandaloneShorterWeekdays = NULL;
    fStandaloneShorterWeekdaysCount=0;
    fStandaloneNarrowWeekdays = NULL;
    fStandaloneNarrowWeekdaysCount=0;
    fAmPms = NULL;
    fAmPmsCount=0;
    fNarrowAmPms = NULL;
    fNarrowAmPmsCount=0;
    fTimeSeparator.setToBogus();
    fQuarters = NULL;
    fQuartersCount = 0;
    fShortQuarters = NULL;
    fShortQuartersCount = 0;
    fStandaloneQuarters = NULL;
    fStandaloneQuartersCount = 0;
    fStandaloneShortQuarters = NULL;
    fStandaloneShortQuartersCount = 0;
    fLeapMonthPatterns = NULL;
    fLeapMonthPatternsCount = 0;
    fShortYearNames = NULL;
    fShortYearNamesCount = 0;
    fShortZodiacNames = NULL;
    fShortZodiacNamesCount = 0;
    fZoneStringsRowCount = 0;
    fZoneStringsColCount = 0;
    fZoneStrings = NULL;
    fLocaleZoneStrings = NULL;
    fAbbreviatedDayPeriods = NULL;
    fAbbreviatedDayPeriodsCount = 0;
    fWideDayPeriods = NULL;
    fWideDayPeriodsCount = 0;
    fNarrowDayPeriods = NULL;
    fNarrowDayPeriodsCount = 0;
    fStandaloneAbbreviatedDayPeriods = NULL;
    fStandaloneAbbreviatedDayPeriodsCount = 0;
    fStandaloneWideDayPeriods = NULL;
    fStandaloneWideDayPeriodsCount = 0;
    fStandaloneNarrowDayPeriods = NULL;
    fStandaloneNarrowDayPeriodsCount = 0;
    uprv_memset(fCapitalization, 0, sizeof(fCapitalization));

    // We need to preserve the requested locale for
    // lazy ZoneStringFormat instantiation.  ZoneStringFormat
    // is region sensitive, thus, bundle locale bundle's locale
    // is not sufficient.
    fZSFLocale = locale;

    if (U_FAILURE(status)) return;

    // Create a CalendarDataSink to process this data and the resouce bundles
    CalendarDataSink calendarSink(status);
    UResourceBundle *rb = ures_open(NULL, locale.getBaseName(), &status);
    UResourceBundle *cb = ures_getByKey(rb, gCalendarTag, NULL, &status);

    if (U_FAILURE(status)) return;

    // Iterate over the resource bundle data following the fallbacks through different calendar types
    UnicodeString calendarType((type != NULL && *type != '\0')? type : gGregorianTag, -1, US_INV);
    while (!calendarType.isBogus()) {
        CharString calendarTypeBuffer;
        calendarTypeBuffer.appendInvariantChars(calendarType, status);
        if (U_FAILURE(status)) { return; }
        const char *calendarTypeCArray = calendarTypeBuffer.data();

        // Enumerate this calendar type. If the calendar is not found fallback to gregorian
        UErrorCode oldStatus = status;
        UResourceBundle *ctb = ures_getByKeyWithFallback(cb, calendarTypeCArray, NULL, &status);
        if (status == U_MISSING_RESOURCE_ERROR) {
            ures_close(ctb);
            if (uprv_strcmp(calendarTypeCArray, gGregorianTag) != 0) {
                calendarType.setTo(FALSE, kGregorianTagUChar, UPRV_LENGTHOF(kGregorianTagUChar));
                calendarSink.visitAllResources();
                status = oldStatus;
                continue;
            }
            return;
        }

        calendarSink.preEnumerate(calendarType);
        ures_getAllItemsWithFallback(ctb, "", calendarSink, status);
        ures_close(ctb);
        if (U_FAILURE(status)) break;

        // Stop loading when gregorian was loaded
        if (uprv_strcmp(calendarTypeCArray, gGregorianTag) == 0) {
            break;
        }

        // Get the next calendar type to process from the sink
        calendarType = calendarSink.nextCalendarType;

        // Gregorian is always the last fallback
        if (calendarType.isBogus()) {
            calendarType.setTo(FALSE, kGregorianTagUChar, UPRV_LENGTHOF(kGregorianTagUChar));
            calendarSink.visitAllResources();
        }
    }

    // CharString object to build paths
    CharString path;

    // Load Leap Month Patterns
    UErrorCode tempStatus = status;
    fLeapMonthPatterns = newUnicodeStringArray(kMonthPatternsCount);
    if (fLeapMonthPatterns) {
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternFormatWide, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesFormatTag, gNamesWideTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternFormatAbbrev, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesFormatTag, gNamesAbbrTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternFormatNarrow, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesFormatTag, gNamesNarrowTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternStandaloneWide, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesStandaloneTag, gNamesWideTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternStandaloneAbbrev, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesStandaloneTag, gNamesAbbrTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternStandaloneNarrow, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesStandaloneTag, gNamesNarrowTag, tempStatus), tempStatus);
        initLeapMonthPattern(fLeapMonthPatterns, kLeapMonthPatternNumeric, calendarSink,
                             buildResourcePath(path, gMonthPatternsTag, gNamesNumericTag, gNamesAllTag, tempStatus), tempStatus);
        if (U_SUCCESS(tempStatus)) {
            // Hack to fix bad C inheritance for dangi monthPatterns (OK in J); this should be handled by aliases in root, but isn't.
            // The ordering of the following statements is important.
            if (fLeapMonthPatterns[kLeapMonthPatternFormatAbbrev].isEmpty()) {
                fLeapMonthPatterns[kLeapMonthPatternFormatAbbrev].setTo(fLeapMonthPatterns[kLeapMonthPatternFormatWide]);
            }
            if (fLeapMonthPatterns[kLeapMonthPatternFormatNarrow].isEmpty()) {
                fLeapMonthPatterns[kLeapMonthPatternFormatNarrow].setTo(fLeapMonthPatterns[kLeapMonthPatternStandaloneNarrow]);
            }
            if (fLeapMonthPatterns[kLeapMonthPatternStandaloneWide].isEmpty()) {
                fLeapMonthPatterns[kLeapMonthPatternStandaloneWide].setTo(fLeapMonthPatterns[kLeapMonthPatternFormatWide]);
            }
            if (fLeapMonthPatterns[kLeapMonthPatternStandaloneAbbrev].isEmpty()) {
                fLeapMonthPatterns[kLeapMonthPatternStandaloneAbbrev].setTo(fLeapMonthPatterns[kLeapMonthPatternFormatAbbrev]);
            }
            // end of hack
            fLeapMonthPatternsCount = kMonthPatternsCount;
        } else {
            delete[] fLeapMonthPatterns;
            fLeapMonthPatterns = NULL;
        }
    }

    // Load cyclic names sets
    tempStatus = status;
    initField(&fShortYearNames, fShortYearNamesCount, calendarSink,
              buildResourcePath(path, gCyclicNameSetsTag, gNameSetYearsTag, gNamesFormatTag, gNamesAbbrTag, tempStatus), tempStatus);
    initField(&fShortZodiacNames, fShortZodiacNamesCount, calendarSink,
              buildResourcePath(path, gCyclicNameSetsTag, gNameSetZodiacsTag, gNamesFormatTag, gNamesAbbrTag, tempStatus), tempStatus);

    // Load context transforms and capitalization
    tempStatus = U_ZERO_ERROR;
    UResourceBundle *localeBundle = ures_open(NULL, locale.getName(), &tempStatus);
    if (U_SUCCESS(tempStatus)) {
        UResourceBundle *contextTransforms = ures_getByKeyWithFallback(localeBundle, gContextTransformsTag, NULL, &tempStatus);
        if (U_SUCCESS(tempStatus)) {
            UResourceBundle *contextTransformUsage;
            while ( (contextTransformUsage = ures_getNextResource(contextTransforms, NULL, &tempStatus)) != NULL ) {
                const int32_t * intVector = ures_getIntVector(contextTransformUsage, &len, &status);
                if (U_SUCCESS(tempStatus) && intVector != NULL && len >= 2) {
                    const char* usageType = ures_getKey(contextTransformUsage);
                    if (usageType != NULL) {
                        const ContextUsageTypeNameToEnumValue * typeMapPtr = contextUsageTypeMap;
                        int32_t compResult = 0;
                        // linear search; list is short and we cannot be sure that bsearch is available
                        while ( typeMapPtr->usageTypeName != NULL && (compResult = uprv_strcmp(usageType, typeMapPtr->usageTypeName)) > 0 ) {
                            ++typeMapPtr;
                        }
                        if (typeMapPtr->usageTypeName != NULL && compResult == 0) {
                            fCapitalization[typeMapPtr->usageTypeEnumValue][0] = static_cast<UBool>(intVector[0]);
                            fCapitalization[typeMapPtr->usageTypeEnumValue][1] = static_cast<UBool>(intVector[1]);
                        }
                    }
                }
                tempStatus = U_ZERO_ERROR;
                ures_close(contextTransformUsage);
            }
            ures_close(contextTransforms);
        }

        tempStatus = U_ZERO_ERROR;
        const LocalPointer<NumberingSystem> numberingSystem(
                NumberingSystem::createInstance(locale, tempStatus), tempStatus);
        if (U_SUCCESS(tempStatus)) {
            // These functions all fail gracefully if passed NULL pointers and
            // do nothing unless U_SUCCESS(tempStatus), so it's only necessary
            // to check for errors once after all calls are made.
            const LocalUResourceBundlePointer numberElementsData(ures_getByKeyWithFallback(
                    localeBundle, gNumberElementsTag, NULL, &tempStatus));
            const LocalUResourceBundlePointer nsNameData(ures_getByKeyWithFallback(
                    numberElementsData.getAlias(), numberingSystem->getName(), NULL, &tempStatus));
            const LocalUResourceBundlePointer symbolsData(ures_getByKeyWithFallback(
                    nsNameData.getAlias(), gSymbolsTag, NULL, &tempStatus));
            fTimeSeparator = ures_getUnicodeStringByKey(
                    symbolsData.getAlias(), gTimeSeparatorTag, &tempStatus);
            if (U_FAILURE(tempStatus)) {
                fTimeSeparator.setToBogus();
            }
        }

        ures_close(localeBundle);
    }

    if (fTimeSeparator.isBogus()) {
        fTimeSeparator.setTo(DateFormatSymbols::DEFAULT_TIME_SEPARATOR);
    }

    // Load day periods
    fWideDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesFormatTag, gNamesWideTag, status),
                            fWideDayPeriodsCount, status);
    fNarrowDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesFormatTag, gNamesNarrowTag, status),
                            fNarrowDayPeriodsCount, status);
    fAbbreviatedDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesFormatTag, gNamesAbbrTag, status),
                            fAbbreviatedDayPeriodsCount, status);
    fStandaloneWideDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesStandaloneTag, gNamesWideTag, status),
                            fStandaloneWideDayPeriodsCount, status);
    fStandaloneNarrowDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesStandaloneTag, gNamesNarrowTag, status),
                            fStandaloneNarrowDayPeriodsCount, status);
    fStandaloneAbbreviatedDayPeriods = loadDayPeriodStrings(calendarSink,
                            buildResourcePath(path, gDayPeriodTag, gNamesStandaloneTag, gNamesAbbrTag, status),
                            fStandaloneAbbreviatedDayPeriodsCount, status);

    U_LOCALE_BASED(locBased, *this);
    // if we make it to here, the resource data is cool, and we can get everything out
    // of it that we need except for the time-zone and localized-pattern data, which
    // are stored in a separate file
    locBased.setLocaleIDs(ures_getLocaleByType(cb, ULOC_VALID_LOCALE, &status),
                          ures_getLocaleByType(cb, ULOC_ACTUAL_LOCALE, &status));

    // Load eras
    initField(&fEras, fErasCount, calendarSink, buildResourcePath(path, gErasTag, gNamesAbbrTag, status), status);
    UErrorCode oldStatus = status;
    initField(&fEraNames, fEraNamesCount, calendarSink, buildResourcePath(path, gErasTag, gNamesWideTag, status), status);
    if (status == U_MISSING_RESOURCE_ERROR) { // Workaround because eras/wide was omitted from CLDR 1.3
        status = oldStatus;
        assignArray(fEraNames, fEraNamesCount, fEras, fErasCount);
    }
    // current ICU4J falls back to abbreviated if narrow eras are missing, so we will too
    oldStatus = status;
    initField(&fNarrowEras, fNarrowErasCount, calendarSink, buildResourcePath(path, gErasTag, gNamesNarrowTag, status), status);
    if (status == U_MISSING_RESOURCE_ERROR) { // Workaround because eras/wide was omitted from CLDR 1.3
        status = oldStatus;
        assignArray(fNarrowEras, fNarrowErasCount, fEras, fErasCount);
    }

    // Load month names
    initField(&fMonths, fMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesFormatTag, gNamesWideTag, status), status);
    initField(&fShortMonths, fShortMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesFormatTag, gNamesAbbrTag, status), status);
    initField(&fStandaloneMonths, fStandaloneMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesStandaloneTag, gNamesWideTag, status), status);
    if (status == U_MISSING_RESOURCE_ERROR) { /* If standalone/wide not available, use format/wide */
        status = U_ZERO_ERROR;
        assignArray(fStandaloneMonths, fStandaloneMonthsCount, fMonths, fMonthsCount);
    }
    initField(&fStandaloneShortMonths, fStandaloneShortMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesStandaloneTag, gNamesAbbrTag, status), status);
    if (status == U_MISSING_RESOURCE_ERROR) { /* If standalone/abbreviated not available, use format/abbreviated */
        status = U_ZERO_ERROR;
        assignArray(fStandaloneShortMonths, fStandaloneShortMonthsCount, fShortMonths, fShortMonthsCount);
    }

    UErrorCode narrowMonthsEC = status;
    UErrorCode standaloneNarrowMonthsEC = status;
    initField(&fNarrowMonths, fNarrowMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesFormatTag, gNamesNarrowTag, narrowMonthsEC), narrowMonthsEC);
    initField(&fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount, calendarSink,
              buildResourcePath(path, gMonthNamesTag, gNamesStandaloneTag, gNamesNarrowTag, narrowMonthsEC), standaloneNarrowMonthsEC);
    if (narrowMonthsEC == U_MISSING_RESOURCE_ERROR && standaloneNarrowMonthsEC != U_MISSING_RESOURCE_ERROR) {
        // If format/narrow not available, use standalone/narrow
        assignArray(fNarrowMonths, fNarrowMonthsCount, fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount);
    } else if (narrowMonthsEC != U_MISSING_RESOURCE_ERROR && standaloneNarrowMonthsEC == U_MISSING_RESOURCE_ERROR) {
        // If standalone/narrow not availabe, use format/narrow
        assignArray(fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount, fNarrowMonths, fNarrowMonthsCount);
    } else if (narrowMonthsEC == U_MISSING_RESOURCE_ERROR && standaloneNarrowMonthsEC == U_MISSING_RESOURCE_ERROR) {
        // If neither is available, use format/abbreviated
        assignArray(fNarrowMonths, fNarrowMonthsCount, fShortMonths, fShortMonthsCount);
        assignArray(fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount, fShortMonths, fShortMonthsCount);
    }

    // Load AM/PM markers; if wide or narrow not available, use short
    UErrorCode ampmStatus = U_ZERO_ERROR;
    initField(&fAmPms, fAmPmsCount, calendarSink,
              buildResourcePath(path, gAmPmMarkersTag, ampmStatus), ampmStatus);
    if (U_FAILURE(ampmStatus)) {
        initField(&fAmPms, fAmPmsCount, calendarSink,
                  buildResourcePath(path, gAmPmMarkersAbbrTag, status), status);
    }
    ampmStatus = U_ZERO_ERROR;
    initField(&fNarrowAmPms, fNarrowAmPmsCount, calendarSink,
              buildResourcePath(path, gAmPmMarkersNarrowTag, ampmStatus), ampmStatus);
    if (U_FAILURE(ampmStatus)) {
        initField(&fNarrowAmPms, fNarrowAmPmsCount, calendarSink,
                  buildResourcePath(path, gAmPmMarkersAbbrTag, status), status);
    }

    // Load quarters
    initField(&fQuarters, fQuartersCount, calendarSink,
              buildResourcePath(path, gQuartersTag, gNamesFormatTag, gNamesWideTag, status), status);
    initField(&fShortQuarters, fShortQuartersCount, calendarSink,
              buildResourcePath(path, gQuartersTag, gNamesFormatTag, gNamesAbbrTag, status), status);

    initField(&fStandaloneQuarters, fStandaloneQuartersCount, calendarSink,
              buildResourcePath(path, gQuartersTag, gNamesStandaloneTag, gNamesWideTag, status), status);
    if(status == U_MISSING_RESOURCE_ERROR) {
        status = U_ZERO_ERROR;
        assignArray(fStandaloneQuarters, fStandaloneQuartersCount, fQuarters, fQuartersCount);
    }
    initField(&fStandaloneShortQuarters, fStandaloneShortQuartersCount, calendarSink,
              buildResourcePath(path, gQuartersTag, gNamesStandaloneTag, gNamesAbbrTag, status), status);
    if(status == U_MISSING_RESOURCE_ERROR) {
        status = U_ZERO_ERROR;
        assignArray(fStandaloneShortQuarters, fStandaloneShortQuartersCount, fShortQuarters, fShortQuartersCount);
    }

    // ICU 3.8 or later version no longer uses localized date-time pattern characters by default (ticket#5597)
    /*
    // fastCopyFrom()/setTo() - see assignArray comments
    resStr = ures_getStringByKey(fResourceBundle, gLocalPatternCharsTag, &len, &status);
    fLocalPatternChars.setTo(TRUE, resStr, len);
    // If the locale data does not include new pattern chars, use the defaults
    // TODO: Consider making this an error, since this may add conflicting characters.
    if (len < PATTERN_CHARS_LEN) {
        fLocalPatternChars.append(UnicodeString(TRUE, &gPatternChars[len], PATTERN_CHARS_LEN-len));
    }
    */
    fLocalPatternChars.setTo(TRUE, gPatternChars, PATTERN_CHARS_LEN);

    // Format wide weekdays -> fWeekdays
    // {sfb} fixed to handle 1-based weekdays
    initField(&fWeekdays, fWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesFormatTag, gNamesWideTag, status), 1, status);

    // Format abbreviated weekdays -> fShortWeekdays
    initField(&fShortWeekdays, fShortWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesFormatTag, gNamesAbbrTag, status), 1, status);

    // Format short weekdays -> fShorterWeekdays (fall back to abbreviated)
    initField(&fShorterWeekdays, fShorterWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesFormatTag, gNamesShortTag, status), 1, status);
    if (status == U_MISSING_RESOURCE_ERROR) {
        status = U_ZERO_ERROR;
        assignArray(fShorterWeekdays, fShorterWeekdaysCount, fShortWeekdays, fShortWeekdaysCount);
    }

    // Stand-alone wide weekdays -> fStandaloneWeekdays
    initField(&fStandaloneWeekdays, fStandaloneWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesStandaloneTag, gNamesWideTag, status), 1, status);
    if (status == U_MISSING_RESOURCE_ERROR) { /* If standalone/wide is not available, use format/wide */
        status = U_ZERO_ERROR;
        assignArray(fStandaloneWeekdays, fStandaloneWeekdaysCount, fWeekdays, fWeekdaysCount);
    }

    // Stand-alone abbreviated weekdays -> fStandaloneShortWeekdays
    initField(&fStandaloneShortWeekdays, fStandaloneShortWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesStandaloneTag, gNamesAbbrTag, status), 1, status);
    if (status == U_MISSING_RESOURCE_ERROR) { /* If standalone/abbreviated is not available, use format/abbreviated */
        status = U_ZERO_ERROR;
        assignArray(fStandaloneShortWeekdays, fStandaloneShortWeekdaysCount, fShortWeekdays, fShortWeekdaysCount);
    }

    // Stand-alone short weekdays -> fStandaloneShorterWeekdays (fall back to format abbreviated)
    initField(&fStandaloneShorterWeekdays, fStandaloneShorterWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesStandaloneTag, gNamesShortTag, status), 1, status);
    if (status == U_MISSING_RESOURCE_ERROR) { /* If standalone/short is not available, use format/short */
        status = U_ZERO_ERROR;
        assignArray(fStandaloneShorterWeekdays, fStandaloneShorterWeekdaysCount, fShorterWeekdays, fShorterWeekdaysCount);
    }

    // Format narrow weekdays -> fNarrowWeekdays
    UErrorCode narrowWeeksEC = status;
    initField(&fNarrowWeekdays, fNarrowWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesFormatTag, gNamesNarrowTag, status), 1, narrowWeeksEC);
    // Stand-alone narrow weekdays -> fStandaloneNarrowWeekdays
    UErrorCode standaloneNarrowWeeksEC = status;
    initField(&fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount, calendarSink,
              buildResourcePath(path, gDayNamesTag, gNamesStandaloneTag, gNamesNarrowTag, status), 1, standaloneNarrowWeeksEC);

    if (narrowWeeksEC == U_MISSING_RESOURCE_ERROR && standaloneNarrowWeeksEC != U_MISSING_RESOURCE_ERROR) {
        // If format/narrow not available, use standalone/narrow
        assignArray(fNarrowWeekdays, fNarrowWeekdaysCount, fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount);
    } else if (narrowWeeksEC != U_MISSING_RESOURCE_ERROR && standaloneNarrowWeeksEC == U_MISSING_RESOURCE_ERROR) {
        // If standalone/narrow not available, use format/narrow
        assignArray(fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount, fNarrowWeekdays, fNarrowWeekdaysCount);
    } else if (narrowWeeksEC == U_MISSING_RESOURCE_ERROR && standaloneNarrowWeeksEC == U_MISSING_RESOURCE_ERROR ) {
        // If neither is available, use format/abbreviated
        assignArray(fNarrowWeekdays, fNarrowWeekdaysCount, fShortWeekdays, fShortWeekdaysCount);
        assignArray(fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount, fShortWeekdays, fShortWeekdaysCount);
    }

    // Last resort fallback in case previous data wasn't loaded
    if (U_FAILURE(status))
    {
        if (useLastResortData)
        {
            // Handle the case in which there is no resource data present.
            // We don't have to generate usable patterns in this situation;
            // we just need to produce something that will be semi-intelligible
            // in most locales.

            status = U_USING_FALLBACK_WARNING;
            //TODO(fabalbon): make sure we are storing las resort data for all fields in here.
            initField(&fEras, fErasCount, (const UChar *)gLastResortEras, kEraNum, kEraLen, status);
            initField(&fEraNames, fEraNamesCount, (const UChar *)gLastResortEras, kEraNum, kEraLen, status);
            initField(&fNarrowEras, fNarrowErasCount, (const UChar *)gLastResortEras, kEraNum, kEraLen, status);
            initField(&fMonths, fMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen,  status);
            initField(&fShortMonths, fShortMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen, status);
            initField(&fNarrowMonths, fNarrowMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen, status);
            initField(&fStandaloneMonths, fStandaloneMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen,  status);
            initField(&fStandaloneShortMonths, fStandaloneShortMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen, status);
            initField(&fStandaloneNarrowMonths, fStandaloneNarrowMonthsCount, (const UChar *)gLastResortMonthNames, kMonthNum, kMonthLen, status);
            initField(&fWeekdays, fWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fShortWeekdays, fShortWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fShorterWeekdays, fShorterWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fNarrowWeekdays, fNarrowWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fStandaloneWeekdays, fStandaloneWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fStandaloneShortWeekdays, fStandaloneShortWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fStandaloneShorterWeekdays, fStandaloneShorterWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fStandaloneNarrowWeekdays, fStandaloneNarrowWeekdaysCount, (const UChar *)gLastResortDayNames, kDayNum, kDayLen, status);
            initField(&fAmPms, fAmPmsCount, (const UChar *)gLastResortAmPmMarkers, kAmPmNum, kAmPmLen, status);
            initField(&fNarrowAmPms, fNarrowAmPmsCount, (const UChar *)gLastResortAmPmMarkers, kAmPmNum, kAmPmLen, status);
            initField(&fQuarters, fQuartersCount, (const UChar *)gLastResortQuarters, kQuarterNum, kQuarterLen, status);
            initField(&fShortQuarters, fShortQuartersCount, (const UChar *)gLastResortQuarters, kQuarterNum, kQuarterLen, status);
            initField(&fStandaloneQuarters, fStandaloneQuartersCount, (const UChar *)gLastResortQuarters, kQuarterNum, kQuarterLen, status);
            initField(&fStandaloneShortQuarters, fStandaloneShortQuartersCount, (const UChar *)gLastResortQuarters, kQuarterNum, kQuarterLen, status);
            fLocalPatternChars.setTo(TRUE, gPatternChars, PATTERN_CHARS_LEN);
        }
    }

    // Close resources
    ures_close(cb);
    ures_close(rb);
}

Locale
DateFormatSymbols::getLocale(ULocDataLocaleType type, UErrorCode& status) const {
    U_LOCALE_BASED(locBased, *this);
    return locBased.getLocale(type, status);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
