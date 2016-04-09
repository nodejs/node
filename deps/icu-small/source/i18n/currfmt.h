/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 20, 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef CURRENCYFORMAT_H
#define CURRENCYFORMAT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/measfmt.h"

U_NAMESPACE_BEGIN

class NumberFormat;

/**
 * Temporary internal concrete subclass of MeasureFormat implementing
 * parsing and formatting of currency amount objects.  This class is
 * likely to be redesigned and rewritten in the near future.
 *
 * <p>This class currently delegates to DecimalFormat for parsing and
 * formatting.
 *
 * @see MeasureFormat
 * @author Alan Liu
 * @internal
 */
class CurrencyFormat : public MeasureFormat {

 public:

    /**
     * Construct a CurrencyFormat for the given locale.
     */
    CurrencyFormat(const Locale& locale, UErrorCode& ec);

    /**
     * Copy constructor.
     */
    CurrencyFormat(const CurrencyFormat& other);

    /**
     * Destructor.
     */
    virtual ~CurrencyFormat();

    /**
     * Override Format API.
     */
    virtual Format* clone() const;


    using MeasureFormat::format;

    /**
     * Override Format API.
     */
    virtual UnicodeString& format(const Formattable& obj,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos,
                                  UErrorCode& ec) const;

    /**
     * Override Format API.
     */
    virtual void parseObject(const UnicodeString& source,
                             Formattable& result,
                             ParsePosition& pos) const;

    /**
     * Override Format API.
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * Returns the class ID for this class.
     */
    static UClassID U_EXPORT2 getStaticClassID();

 private:

    NumberFormat* fmt;
};

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_FORMATTING
#endif // #ifndef CURRENCYFORMAT_H
