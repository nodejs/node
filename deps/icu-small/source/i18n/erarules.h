// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef ERARULES_H_
#define ERARULES_H_

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/localpointer.h"
#include "unicode/uobject.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

class U_I18N_API_CLASS EraRules : public UMemory {
public:
    U_I18N_API ~EraRules();

    U_I18N_API static EraRules* createInstance(const char* calType,
                                               UBool includeTentativeEra,
                                               UErrorCode& status);

    /**
     * Gets number of effective eras
     * @return  number of effective eras (not the same as max era code)
     */
    inline int32_t getNumberOfEras() const {
        return numEras;
    }

    /**
     * Gets maximum defined era code for the current calendar
     * @return  maximum defined era code
     */
    inline int32_t getMaxEraCode() const {
        return minEra + startDatesLength - 1;
    }

    /**
     * Gets start date of an era
     * @param eraCode   Era code
     * @param fields    Receives date fields. The result includes values of year, month,
     *                  day of month in this order. When an era has no start date, the result
     *                  will be January 1st in year whose value is minimum integer.
     * @param status    Receives status.
     */
    void getStartDate(int32_t eraCode, int32_t (&fields)[3], UErrorCode& status) const;

    /**
     * Gets start year of an era
     * @param eraCode   Era code
     * @param status    Receives status.
     * @return The first year of an era. When a era has no start date, minimum int32
     *          value is returned.
     */
    U_I18N_API int32_t getStartYear(int32_t eraCode, UErrorCode& status) const;

    /**
     * Returns era code for the specified year/month/day.
     * @param year  Year
     * @param month Month (1-base)
     * @param day   Day of month
     * @param status    Receives status
     * @return  era code (or code of earliest era when date is before that era)
     */
    U_I18N_API int32_t getEraCode(int32_t year, int32_t month, int32_t day, UErrorCode& status) const;

    /**
     * Gets the current era code. This is calculated only once for an instance of
     * EraRules. The current era calculation is based on the default time zone at
     * the time of instantiation.
     *
     * @return era code of current era (or era code of earliest era when current date is before any era)
     */
    inline int32_t getCurrentEraCode() const {
        return currentEra;
    }

private:
    EraRules(LocalMemory<int32_t>& startDatesIn, int32_t startDatesLengthIn, int32_t minEraIn, int32_t numErasIn);

    void initCurrentEra();

    LocalMemory<int32_t> startDates;
    int32_t startDatesLength;
    int32_t minEra;  // minimum valid era code, for first entry in startDates
    int32_t numEras; // number of valid era codes (not necessarily the same as startDates length
    int32_t currentEra;
};

U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif /* ERARULES_H_ */
