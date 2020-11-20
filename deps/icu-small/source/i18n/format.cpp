// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File FORMAT.CPP
*
* Modification History:
*
*   Date        Name        Description
*   02/19/97    aliu        Converted from java.
*   03/17/97    clhuang     Implemented with new APIs.
*   03/27/97    helena      Updated to pass the simple test after code review.
*   07/20/98    stephen        Added explicit init values for Field/ParsePosition
********************************************************************************
*/
// *****************************************************************************
// This file was generated from the java source file Format.java
// *****************************************************************************

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#ifndef U_I18N_IMPLEMENTATION
#error U_I18N_IMPLEMENTATION not set - must be set for all ICU source files in i18n/ - see https://unicode-org.github.io/icu/userguide/howtouseicu
#endif

/*
 * Dummy code:
 * If all modules in the I18N library are switched off, then there are no
 * library exports and MSVC 6 writes a .dll but not a .lib file.
 * Unless we export _something_ in that case...
 */
#if UCONFIG_NO_COLLATION && UCONFIG_NO_FORMATTING && UCONFIG_NO_TRANSLITERATION
U_CAPI int32_t U_EXPORT2
uprv_icuin_lib_dummy(int32_t i) {
    return -i;
}
#endif

/* Format class implementation ---------------------------------------------- */

#if !UCONFIG_NO_FORMATTING

#include "unicode/format.h"
#include "unicode/ures.h"
#include "cstring.h"
#include "locbased.h"

// *****************************************************************************
// class Format
// *****************************************************************************

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FieldPosition)

FieldPosition::~FieldPosition() {}

FieldPosition *
FieldPosition::clone() const {
    return new FieldPosition(*this);
}

// -------------------------------------
// default constructor

Format::Format()
    : UObject()
{
    *validLocale = *actualLocale = 0;
}

// -------------------------------------

Format::~Format()
{
}

// -------------------------------------
// copy constructor

Format::Format(const Format &that)
    : UObject(that)
{
    *this = that;
}

// -------------------------------------
// assignment operator

Format&
Format::operator=(const Format& that)
{
    if (this != &that) {
        uprv_strcpy(validLocale, that.validLocale);
        uprv_strcpy(actualLocale, that.actualLocale);
    }
    return *this;
}

// -------------------------------------
// Formats the obj and append the result in the buffer, toAppendTo.
// This calls the actual implementation in the concrete subclasses.

UnicodeString&
Format::format(const Formattable& obj,
               UnicodeString& toAppendTo,
               UErrorCode& status) const
{
    if (U_FAILURE(status)) return toAppendTo;

    FieldPosition pos(FieldPosition::DONT_CARE);

    return format(obj, toAppendTo, pos, status);
}

// -------------------------------------
// Default implementation sets unsupported error; subclasses should
// override.

UnicodeString&
Format::format(const Formattable& /* unused obj */,
               UnicodeString& toAppendTo,
               FieldPositionIterator* /* unused posIter */,
               UErrorCode& status) const
{
    if (!U_FAILURE(status)) {
      status = U_UNSUPPORTED_ERROR;
    }
    return toAppendTo;
}

// -------------------------------------
// Parses the source string and create the corresponding
// result object.  Checks the parse position for errors.

void
Format::parseObject(const UnicodeString& source,
                    Formattable& result,
                    UErrorCode& status) const
{
    if (U_FAILURE(status)) return;

    ParsePosition parsePosition(0);
    parseObject(source, result, parsePosition);
    if (parsePosition.getIndex() == 0) {
        status = U_INVALID_FORMAT_ERROR;
    }
}

// -------------------------------------

UBool
Format::operator==(const Format& that) const
{
    // Subclasses: Call this method and then add more specific checks.
    return typeid(*this) == typeid(that);
}
//---------------------------------------

/**
 * Simple function for initializing a UParseError from a UnicodeString.
 *
 * @param pattern The pattern to copy into the parseError
 * @param pos The position in pattern where the error occured
 * @param parseError The UParseError object to fill in
 * @draft ICU 2.4
 */
void Format::syntaxError(const UnicodeString& pattern,
                         int32_t pos,
                         UParseError& parseError) {
    parseError.offset = pos;
    parseError.line=0;  // we are not using line number

    // for pre-context
    int32_t start = (pos < U_PARSE_CONTEXT_LEN)? 0 : (pos - (U_PARSE_CONTEXT_LEN-1
                                                             /* subtract 1 so that we have room for null*/));
    int32_t stop  = pos;
    pattern.extract(start,stop-start,parseError.preContext,0);
    //null terminate the buffer
    parseError.preContext[stop-start] = 0;

    //for post-context
    start = pos+1;
    stop  = ((pos+U_PARSE_CONTEXT_LEN)<=pattern.length()) ? (pos+(U_PARSE_CONTEXT_LEN-1)) :
        pattern.length();
    pattern.extract(start,stop-start,parseError.postContext,0);
    //null terminate the buffer
    parseError.postContext[stop-start]= 0;
}

Locale
Format::getLocale(ULocDataLocaleType type, UErrorCode& status) const {
    U_LOCALE_BASED(locBased, *this);
    return locBased.getLocale(type, status);
}

const char *
Format::getLocaleID(ULocDataLocaleType type, UErrorCode& status) const {
    U_LOCALE_BASED(locBased, *this);
    return locBased.getLocaleID(type, status);
}

void
Format::setLocaleIDs(const char* valid, const char* actual) {
    U_LOCALE_BASED(locBased, *this);
    locBased.setLocaleIDs(valid, actual);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
