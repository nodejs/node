// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2008-2014, Google, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/tmunit.h"
#include "uassert.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(TimeUnit)


/*
 * There are only 7 time units.
 * So, TimeUnit could be made as singleton 
 * (similar to uniset_props.cpp, or unorm.cpp,
 * in which a static TimeUnit* array is created, and 
 * the creatInstance() returns a const TimeUnit*).
 * But the constraint is TimeUnit is a data member of Measure.
 * But Measure (which is an existing API) does not expect it's "unit" member
 * as singleton. Meaure takes ownership of the "unit" member.
 * In its constructor, it does not take a const "unit" pointer.
 * Also, Measure can clone and destruct the "unit" pointer.
 * In order to preserve the old behavior and let Measure handle singleton "unit",  
 * 1. a flag need to be added in Measure; 
 * 2. a new constructor which takes const "unit" as parameter need to be added,
 *    and this new constructor will set the flag on.
 * 3. clone and destructor need to check upon this flag to distinguish on how
 *    to handle the "unit". 
 * 
 * Since TimeUnit is such a light weight object, comparing with the heavy weight
 * format operation, we decided to avoid the above complication.
 * 
 * So, both TimeUnit and CurrencyUnit (the 2 subclasses of MeasureUnit) are
 * immutable and non-singleton.
 *
 * Currently, TimeUnitAmount and CurrencyAmount are immutable.
 * If an application needs to create a long list of TimeUnitAmount on the same
 * time unit but different number, for example,
 * 1 hour, 2 hour, 3 hour, ................. 10,000 hour,
 * there might be performance hit because 10,000 TimeUnit object, 
 * although all are the same time unit, will be created in heap and deleted.
 *
 * To address this performance issue, if there is any in the future,
 * we should and need to change TimeUnitAmount and CurrencyAmount to be 
 * immutable by allowing a setter on the number.
 * Or we need to add 2 parallel mutable classes in order to 
 * preserve the existing API.
 * Or we can use freezable.
 */
TimeUnit* U_EXPORT2 
TimeUnit::createInstance(TimeUnit::UTimeUnitFields timeUnitField, 
                         UErrorCode& status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (timeUnitField < 0 || timeUnitField >= UTIMEUNIT_FIELD_COUNT) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    return new TimeUnit(timeUnitField);
}


TimeUnit::TimeUnit(TimeUnit::UTimeUnitFields timeUnitField) {
    fTimeUnitField = timeUnitField;
    switch (fTimeUnitField) {
    case UTIMEUNIT_YEAR:
        initTime("year");
        break;
    case UTIMEUNIT_MONTH:
        initTime("month");
        break;
    case UTIMEUNIT_DAY:
        initTime("day");
        break;
    case UTIMEUNIT_WEEK:
        initTime("week");
        break;
    case UTIMEUNIT_HOUR:
        initTime("hour");
        break;
    case UTIMEUNIT_MINUTE:
        initTime("minute");
        break;
    case UTIMEUNIT_SECOND:
        initTime("second");
        break;
    default:
        U_ASSERT(false);
        break;
    }
}

TimeUnit::TimeUnit(const TimeUnit& other) 
:   MeasureUnit(other), fTimeUnitField(other.fTimeUnitField) {
}

UObject* 
TimeUnit::clone() const {
    return new TimeUnit(*this);
}

TimeUnit&
TimeUnit::operator=(const TimeUnit& other) {
    if (this == &other) {
        return *this;
    }
    MeasureUnit::operator=(other);
    fTimeUnitField = other.fTimeUnitField;
    return *this;
}

TimeUnit::UTimeUnitFields
TimeUnit::getTimeUnitField() const {
    return fTimeUnitField;
}

TimeUnit::~TimeUnit() {
}


U_NAMESPACE_END

#endif
