// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/currunit.h"
#include "unicode/ustring.h"
#include "cstring.h"

U_NAMESPACE_BEGIN

CurrencyUnit::CurrencyUnit(ConstChar16Ptr _isoCode, UErrorCode& ec) {
    *isoCode = 0;
    if (U_SUCCESS(ec)) {
        if (_isoCode != nullptr && u_strlen(_isoCode)==3) {
            u_strcpy(isoCode, _isoCode);
            char simpleIsoCode[4];
            u_UCharsToChars(isoCode, simpleIsoCode, 4);
            initCurrency(simpleIsoCode);
        } else {
            ec = U_ILLEGAL_ARGUMENT_ERROR;
        }
    }
}

CurrencyUnit::CurrencyUnit(const CurrencyUnit& other) : MeasureUnit(other) {
    u_strcpy(isoCode, other.isoCode);
}

CurrencyUnit::CurrencyUnit(const MeasureUnit& other, UErrorCode& ec) : MeasureUnit(other) {
    // Make sure this is a currency.
    // OK to hard-code the string because we are comparing against another hard-coded string.
    if (uprv_strcmp("currency", getType()) != 0) {
        ec = U_ILLEGAL_ARGUMENT_ERROR;
        isoCode[0] = 0;
    } else {
        // Get the ISO Code from the subtype field.
        u_charsToUChars(getSubtype(), isoCode, 4);
        isoCode[3] = 0; // make 100% sure it is NUL-terminated
    }
}

CurrencyUnit::CurrencyUnit() : MeasureUnit() {
    u_strcpy(isoCode, u"XXX");
    char simpleIsoCode[4];
    u_UCharsToChars(isoCode, simpleIsoCode, 4);
    initCurrency(simpleIsoCode);
}

CurrencyUnit& CurrencyUnit::operator=(const CurrencyUnit& other) {
    if (this == &other) {
        return *this;
    }
    MeasureUnit::operator=(other);
    u_strcpy(isoCode, other.isoCode);
    return *this;
}

UObject* CurrencyUnit::clone() const {
    return new CurrencyUnit(*this);
}

CurrencyUnit::~CurrencyUnit() {
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CurrencyUnit)

U_NAMESPACE_END

#endif // !UCONFIG_NO_FORMATTING
