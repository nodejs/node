// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2008, Google, International Business Machines Corporation and *
 * others. All Rights Reserved.                                                *
 *******************************************************************************
 */ 

#include "unicode/tmutamt.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(TimeUnitAmount)


TimeUnitAmount::TimeUnitAmount(const Formattable& number, 
                               TimeUnit::UTimeUnitFields timeUnitField,
                               UErrorCode& status)
:    Measure(number, TimeUnit::createInstance(timeUnitField, status), status) {
}


TimeUnitAmount::TimeUnitAmount(double amount, 
                               TimeUnit::UTimeUnitFields timeUnitField,
                               UErrorCode& status)
:   Measure(Formattable(amount), 
            TimeUnit::createInstance(timeUnitField, status),
            status) {
}


TimeUnitAmount::TimeUnitAmount(const TimeUnitAmount& other)
:   Measure(other)
{
}


TimeUnitAmount& 
TimeUnitAmount::operator=(const TimeUnitAmount& other) {
    Measure::operator=(other);
    return *this;
}


bool
TimeUnitAmount::operator==(const UObject& other) const {
    return Measure::operator==(other);
}

TimeUnitAmount* 
TimeUnitAmount::clone() const {
    return new TimeUnitAmount(*this);
}

    
TimeUnitAmount::~TimeUnitAmount() {
}



const TimeUnit&
TimeUnitAmount::getTimeUnit() const {
    return static_cast<const TimeUnit&>(getUnit());
}


TimeUnit::UTimeUnitFields
TimeUnitAmount::getTimeUnitField() const {
    return getTimeUnit().getTimeUnitField();
}
    

U_NAMESPACE_END

#endif
