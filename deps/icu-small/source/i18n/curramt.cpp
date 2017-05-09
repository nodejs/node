// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/curramt.h"
#include "unicode/currunit.h"

U_NAMESPACE_BEGIN

CurrencyAmount::CurrencyAmount(const Formattable& amount, ConstChar16Ptr isoCode,
                               UErrorCode& ec) :
    Measure(amount, new CurrencyUnit(isoCode, ec), ec) {
}

CurrencyAmount::CurrencyAmount(double amount, ConstChar16Ptr isoCode,
                               UErrorCode& ec) :
    Measure(Formattable(amount), new CurrencyUnit(isoCode, ec), ec) {
}

CurrencyAmount::CurrencyAmount(const CurrencyAmount& other) :
    Measure(other) {
}

CurrencyAmount& CurrencyAmount::operator=(const CurrencyAmount& other) {
    Measure::operator=(other);
    return *this;
}

UObject* CurrencyAmount::clone() const {
    return new CurrencyAmount(*this);
}

CurrencyAmount::~CurrencyAmount() {
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CurrencyAmount)

U_NAMESPACE_END

#endif // !UCONFIG_NO_FORMATTING
