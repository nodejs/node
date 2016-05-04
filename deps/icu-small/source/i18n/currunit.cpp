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

U_NAMESPACE_BEGIN

CurrencyUnit::CurrencyUnit(const UChar* _isoCode, UErrorCode& ec) {
    *isoCode = 0;
    if (U_SUCCESS(ec)) {
        if (_isoCode && u_strlen(_isoCode)==3) {
            u_strcpy(isoCode, _isoCode);
            char simpleIsoCode[4];
            u_UCharsToChars(isoCode, simpleIsoCode, 4);
            initCurrency(simpleIsoCode);
        } else {
            ec = U_ILLEGAL_ARGUMENT_ERROR;
        }
    }
}

CurrencyUnit::CurrencyUnit(const CurrencyUnit& other) :
    MeasureUnit(other) {
    u_strcpy(isoCode, other.isoCode);
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
