// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// This file contains one implementation of FormattedValue.
// Other independent implementations should go into their own cpp file for
// better dependency modularization.

#include "formattedval_impl.h"

U_NAMESPACE_BEGIN


FormattedValueNumberStringBuilderImpl::FormattedValueNumberStringBuilderImpl(number::impl::Field numericField)
        : fNumericField(numericField) {
}

FormattedValueNumberStringBuilderImpl::~FormattedValueNumberStringBuilderImpl() {
}


UnicodeString FormattedValueNumberStringBuilderImpl::toString(UErrorCode&) const {
    return fString.toUnicodeString();
}

UnicodeString FormattedValueNumberStringBuilderImpl::toTempString(UErrorCode&) const {
    return fString.toTempUnicodeString();
}

Appendable& FormattedValueNumberStringBuilderImpl::appendTo(Appendable& appendable, UErrorCode&) const {
    appendable.appendString(fString.chars(), fString.length());
    return appendable;
}

UBool FormattedValueNumberStringBuilderImpl::nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const {
    // NOTE: MSVC sometimes complains when implicitly converting between bool and UBool
    return fString.nextPosition(cfpos, fNumericField, status) ? TRUE : FALSE;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
