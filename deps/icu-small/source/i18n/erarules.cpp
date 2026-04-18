// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <utility>

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdlib.h>
#include "unicode/ucal.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "unicode/timezone.h"
#include "cmemory.h"
#include "cstring.h"
#include "erarules.h"
#include "gregoimp.h"
#include "uassert.h"
#include "uvectr32.h"

U_NAMESPACE_BEGIN

static const int32_t MAX_ENCODED_START_YEAR = 32767;
static const int32_t MIN_ENCODED_START_YEAR = -32768;
static const int32_t MIN_ENCODED_START = -2147483391;   // encodeDate(MIN_ENCODED_START_YEAR, 1, 1, ...);

static const int32_t YEAR_MASK = 0xFFFF0000;
static const int32_t MONTH_MASK = 0x0000FF00;
static const int32_t DAY_MASK = 0x000000FF;

static const int32_t MAX_INT32 = 0x7FFFFFFF;
static const int32_t MIN_INT32 = 0xFFFFFFFF;

static const char16_t VAL_FALSE[] = {0x66, 0x61, 0x6c, 0x73, 0x65};    // "false"
static const char16_t VAL_FALSE_LEN = 5;

static UBool isSet(int startDate) {
    return startDate != 0;
}

static UBool isValidRuleStartDate(int32_t year, int32_t month, int32_t day) {
    return year >= MIN_ENCODED_START_YEAR && year <= MAX_ENCODED_START_YEAR
            && month >= 1 && month <= 12 && day >=1 && day <= 31;
}

/**
 * Encode year/month/date to a single integer.
 * year is high 16 bits (-32768 to 32767), month is
 * next 8 bits and day of month is last 8 bits.
 *
 * @param year  year
 * @param month month (1-base)
 * @param day   day of month
 * @return  an encoded date.
 */
static int32_t encodeDate(int32_t year, int32_t month, int32_t day) {
    return static_cast<int32_t>(static_cast<uint32_t>(year) << 16) | month << 8 | day;
}

static void decodeDate(int32_t encodedDate, int32_t (&fields)[3]) {
    if (encodedDate == MIN_ENCODED_START) {
        fields[0] = MIN_INT32;
        fields[1] = 1;
        fields[2] = 1;
    } else {
        fields[0] = (encodedDate & YEAR_MASK) >> 16;
        fields[1] = (encodedDate & MONTH_MASK) >> 8;
        fields[2] = encodedDate & DAY_MASK;
    }
}

/**
 * Compare an encoded date with another date specified by year/month/day.
 * @param encoded   An encoded date
 * @param year      Year of another date
 * @param month     Month of another date
 * @param day       Day of another date
 * @return -1 when encoded date is earlier, 0 when two dates are same,
 *          and 1 when encoded date is later.
 */
static int32_t compareEncodedDateWithYMD(int encoded, int year, int month, int day) {
    if (year < MIN_ENCODED_START_YEAR) {
        if (encoded == MIN_ENCODED_START) {
            if (year > MIN_INT32 || month > 1 || day > 1) {
                return -1;
            }
            return 0;
        } else {
            return 1;
        }
    } else if (year > MAX_ENCODED_START_YEAR) {
        return -1;
    } else {
        int tmp = encodeDate(year, month, day);
        if (encoded < tmp) {
            return -1;
        } else if (encoded == tmp) {
            return 0;
        } else {
            return 1;
        }
    }
}

EraRules::EraRules(LocalMemory<int32_t>& startDatesIn, int32_t startDatesLengthIn, int32_t minEraIn, int32_t numErasIn)
    : startDatesLength(startDatesLengthIn), minEra(minEraIn), numEras(numErasIn) {
    startDates = std::move(startDatesIn);
    initCurrentEra();
}

EraRules::~EraRules() {
}

EraRules* EraRules::createInstance(const char *calType, UBool includeTentativeEra, UErrorCode& status) {
    if(U_FAILURE(status)) {
        return nullptr;
    }
    LocalUResourceBundlePointer rb(ures_openDirect(nullptr, "supplementalData", &status));
    ures_getByKey(rb.getAlias(), "calendarData", rb.getAlias(), &status);
    ures_getByKey(rb.getAlias(), calType, rb.getAlias(), &status);
    ures_getByKey(rb.getAlias(), "eras", rb.getAlias(), &status);

    if (U_FAILURE(status)) {
        return nullptr;
    }

    int32_t numEras = ures_getSize(rb.getAlias());
    int32_t firstTentativeIdx = MAX_INT32;

    UVector32 eraStartDates(numEras, status);
    if (U_FAILURE(status)) {
        return nullptr;
    }

    while (ures_hasNext(rb.getAlias())) {
        LocalUResourceBundlePointer eraRuleRes(ures_getNextResource(rb.getAlias(), nullptr, &status));
        if (U_FAILURE(status)) {
            return nullptr;
        }
        const char *eraIdxStr = ures_getKey(eraRuleRes.getAlias());
        char *endp;
        int32_t eraIdx = static_cast<int32_t>(uprv_strtol(eraIdxStr, &endp, 10));
        if (static_cast<size_t>(endp - eraIdxStr) != uprv_strlen(eraIdxStr)) {
            status = U_INVALID_FORMAT_ERROR;
            return nullptr;
        }
        if (eraIdx < 0) {
            status = U_INVALID_FORMAT_ERROR;
            return nullptr;
        }
        if (eraIdx + 1 > eraStartDates.size()) {
            eraStartDates.ensureCapacity(eraIdx + 1, status); // needed only to minimize expansions
            // Fill in 0 for all added slots (else they are undefined)
            while (eraStartDates.size() < eraIdx + 1) {
                eraStartDates.addElement(0, status);
            }
            if (U_FAILURE(status)) {
                return nullptr;
            }
        }
        // Now set the startDate that we just read
        if (isSet(eraStartDates.elementAti(eraIdx))) {
            // start date of the index was already set
            status = U_INVALID_FORMAT_ERROR;
            return nullptr;
        }

        UBool hasName = true;
        UBool hasEnd = true;
        int32_t len;
        while (ures_hasNext(eraRuleRes.getAlias())) {
            LocalUResourceBundlePointer res(ures_getNextResource(eraRuleRes.getAlias(), nullptr, &status));
            if (U_FAILURE(status)) {
                return nullptr;
            }
            const char *key = ures_getKey(res.getAlias());
            if (uprv_strcmp(key, "start") == 0) {
                const int32_t *fields = ures_getIntVector(res.getAlias(), &len, &status);
                if (U_FAILURE(status)) {
                    return nullptr;
                }
                if (len != 3 || !isValidRuleStartDate(fields[0], fields[1], fields[2])) {
                    status = U_INVALID_FORMAT_ERROR;
                    return nullptr;
                }
                eraStartDates.setElementAt(encodeDate(fields[0], fields[1], fields[2]), eraIdx);
            } else if (uprv_strcmp(key, "named") == 0) {
                const char16_t *val = ures_getString(res.getAlias(), &len, &status);
                if (u_strncmp(val, VAL_FALSE, VAL_FALSE_LEN) == 0) {
                    hasName = false;
                }
            } else if (uprv_strcmp(key, "end") == 0) {
                hasEnd = true;
            }
        }

        if (isSet(eraStartDates.elementAti(eraIdx))) {
            if (hasEnd) {
                // This implementation assumes either start or end is available, not both.
                // For now, just ignore the end rule.
            }
        } else {
            if (hasEnd) {
                // The islamic calendars now have an end-only rule for the
                // second (and final) entry; basically they are in reverse order.
                eraStartDates.setElementAt(MIN_ENCODED_START, eraIdx);
            } else {
                status = U_INVALID_FORMAT_ERROR;
                return nullptr;
            }
        }

        if (hasName) {
            if (eraIdx >= firstTentativeIdx) {
                status = U_INVALID_FORMAT_ERROR;
                return nullptr;
            }
        } else {
            if (eraIdx < firstTentativeIdx) {
                firstTentativeIdx = eraIdx;
            }
        }
    }

    // Remove from eraStartDates any tentative eras if they should not be included
    // (these would be the last entries). Also reduce numEras appropriately.
    if (!includeTentativeEra) {
        while (firstTentativeIdx < eraStartDates.size()) {
            int32_t lastEraIdx = eraStartDates.size() - 1;
            if (isSet(eraStartDates.elementAti(lastEraIdx))) { // If there are multiple tentativeEras, some may be unset
                numEras--;
            }
            eraStartDates.removeElementAt(lastEraIdx);
        }
        // Remove any remaining trailing unSet entries
        // (can only have these if tentativeEras have been removed)
        while (eraStartDates.size() > 0 && !isSet(eraStartDates.elementAti(eraStartDates.size() - 1))) {
            eraStartDates.removeElementAt(eraStartDates.size() - 1);
        }
    }
    // Remove from eraStartDates any initial 0 entries, keeping the original index (eraCode)
    // of the first non-zero entry as minEra; then we can add that back to the offset in the
    // compressed array to get the correct eraCode.
    int32_t minEra = 0;
    while (eraStartDates.size() > 0 && !isSet(eraStartDates.elementAti(0))) {
        eraStartDates.removeElementAt(0);
        minEra++;
    }
    // Convert eraStartDates to int32_t array startDates and pass to EraRules constructor,
    // along with startDatesLength, minEra and numEras (which may be different from startDatesLength)
    LocalMemory<int32_t> startDates(static_cast<int32_t *>(uprv_malloc(eraStartDates.size() * sizeof(int32_t))));
    if (startDates.isNull()) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    for (int32_t eraIdx = 0; eraIdx < eraStartDates.size(); eraIdx++) {
        startDates[eraIdx] = eraStartDates.elementAti(eraIdx);
    }
    EraRules *result = new EraRules(startDates, eraStartDates.size(), minEra, numEras);
    if (result == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void EraRules::getStartDate(int32_t eraCode, int32_t (&fields)[3], UErrorCode& status) const {
    if(U_FAILURE(status)) {
        return;
    }
    int32_t startDate = 0;
    if (eraCode >= minEra) {
        int32_t startIdx = eraCode - minEra;
        if (startIdx < startDatesLength) {
            startDate = startDates[startIdx];
        }
    }
    if (isSet(startDate)) {
        decodeDate(startDate, fields);
        return;
    }
    // We did not find the requested eraCode in our data
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return;
}

int32_t EraRules::getStartYear(int32_t eraCode, UErrorCode& status) const {
    int year = MAX_INT32;   // bogus value
    if(U_FAILURE(status)) {
        return year;
    }
    int32_t startDate = 0;
    if (eraCode >= minEra) {
        int32_t startIdx = eraCode - minEra;
        if (startIdx < startDatesLength) {
            startDate = startDates[startIdx];
        }
    }
    if (isSet(startDate)) {
        int fields[3];
        decodeDate(startDate, fields);
        year = fields[0];
        return year;
    }
    // We did not find the requested eraCode in our data
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return year;
}

int32_t EraRules::getEraCode(int32_t year, int32_t month, int32_t day, UErrorCode& status) const {
    if(U_FAILURE(status)) {
        return -1;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return -1;
    }
    if (numEras > 1 && startDates[startDatesLength-1] == MIN_ENCODED_START) {
        // Multiple eras in reverse order, linear search from beginning.
        // Currently only for islamic.
        for (int startIdx = 0; startIdx < startDatesLength; startIdx++) {
            if (!isSet(startDates[startIdx])) {
                continue;
            }
            if (compareEncodedDateWithYMD(startDates[startIdx], year, month, day) <= 0) {
                return minEra + startIdx;
            }
        }
    }
    // Linear search from the end, which should hit the most likely eras first.
    // Also this is the most efficient for any era if we have < 8 or so eras, so only less
    // efficient for early eras in Japanese calendar (while we still have them). Formerly
    // this used binary search which would only be better for those early Japanese eras,
    // but now that is much more difficult since there may be holes in the sorted list.
    // Note with this change, this no longer uses or depends on currentEra.
    for (int startIdx = startDatesLength; startIdx > 0;) {
        if (!isSet(startDates[--startIdx])) {
            continue;
        }
        if (compareEncodedDateWithYMD(startDates[startIdx], year, month, day) <= 0) {
            return minEra + startIdx;
        }
    }
    return minEra;
}

void EraRules::initCurrentEra() {
    // Compute local wall time in millis using ICU's default time zone.
    UErrorCode ec = U_ZERO_ERROR;
    UDate localMillis = ucal_getNow();

    int32_t rawOffset, dstOffset;
    TimeZone* zone = TimeZone::createDefault();
    // If we failed to create the default time zone, we are in a bad state and don't
    // really have many options. Carry on using UTC millis as a fallback.
    if (zone != nullptr) {
        zone->getOffset(localMillis, false, rawOffset, dstOffset, ec);
        delete zone;
        localMillis += (rawOffset + dstOffset);
    }

    int32_t year, mid;
    int8_t  month0, dom;
    Grego::timeToFields(localMillis, year, month0, dom, mid, ec);
    currentEra = minEra;
    if (U_FAILURE(ec)) { return; }
    // Now that getEraCode no longer depends on currentEra, we can just do this:
    currentEra = getEraCode(year, month0 + 1 /* changes to 1-base */, dom, ec);
    if (U_FAILURE(ec)) {
        currentEra = minEra;
    }
}

U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */


