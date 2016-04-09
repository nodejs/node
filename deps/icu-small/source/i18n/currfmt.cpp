/*
**********************************************************************
* Copyright (c) 2004-2014 International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 20, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "currfmt.h"
#include "unicode/numfmt.h"
#include "unicode/curramt.h"

U_NAMESPACE_BEGIN

CurrencyFormat::CurrencyFormat(const Locale& locale, UErrorCode& ec) :
    MeasureFormat(locale, UMEASFMT_WIDTH_WIDE, ec), fmt(NULL)
{
    fmt = NumberFormat::createCurrencyInstance(locale, ec);
}

CurrencyFormat::CurrencyFormat(const CurrencyFormat& other) :
    MeasureFormat(other), fmt(NULL)
{
    fmt = (NumberFormat*) other.fmt->clone();
}

CurrencyFormat::~CurrencyFormat() {
    delete fmt;
}

Format* CurrencyFormat::clone() const {
    return new CurrencyFormat(*this);
}

UnicodeString& CurrencyFormat::format(const Formattable& obj,
                                      UnicodeString& appendTo,
                                      FieldPosition& pos,
                                      UErrorCode& ec) const
{
    return fmt->format(obj, appendTo, pos, ec);
}

void CurrencyFormat::parseObject(const UnicodeString& source,
                                 Formattable& result,
                                 ParsePosition& pos) const
{
    CurrencyAmount* currAmt = fmt->parseCurrency(source, pos);
    if (currAmt != NULL) {
        result.adoptObject(currAmt);
    }
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CurrencyFormat)

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
