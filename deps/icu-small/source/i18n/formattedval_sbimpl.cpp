// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// This file contains one implementation of FormattedValue.
// Other independent implementations should go into their own cpp file for
// better dependency modularization.

#include "unicode/ustring.h"
#include "formattedval_impl.h"
#include "number_types.h"
#include "formatted_string_builder.h"
#include "number_utils.h"
#include "static_unicode_sets.h"

U_NAMESPACE_BEGIN


typedef FormattedStringBuilder::Field Field;


FormattedValueStringBuilderImpl::FormattedValueStringBuilderImpl(Field numericField)
        : fNumericField(numericField) {
}

FormattedValueStringBuilderImpl::~FormattedValueStringBuilderImpl() {
}


UnicodeString FormattedValueStringBuilderImpl::toString(UErrorCode&) const {
    return fString.toUnicodeString();
}

UnicodeString FormattedValueStringBuilderImpl::toTempString(UErrorCode&) const {
    return fString.toTempUnicodeString();
}

Appendable& FormattedValueStringBuilderImpl::appendTo(Appendable& appendable, UErrorCode&) const {
    appendable.appendString(fString.chars(), fString.length());
    return appendable;
}

UBool FormattedValueStringBuilderImpl::nextPosition(ConstrainedFieldPosition& cfpos, UErrorCode& status) const {
    // NOTE: MSVC sometimes complains when implicitly converting between bool and UBool
    return nextPositionImpl(cfpos, fNumericField, status) ? TRUE : FALSE;
}

UBool FormattedValueStringBuilderImpl::nextFieldPosition(FieldPosition& fp, UErrorCode& status) const {
    int32_t rawField = fp.getField();

    if (rawField == FieldPosition::DONT_CARE) {
        return FALSE;
    }

    if (rawField < 0 || rawField >= UNUM_FIELD_COUNT) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }

    ConstrainedFieldPosition cfpos;
    cfpos.constrainField(UFIELD_CATEGORY_NUMBER, rawField);
    cfpos.setState(UFIELD_CATEGORY_NUMBER, rawField, fp.getBeginIndex(), fp.getEndIndex());
    if (nextPositionImpl(cfpos, 0, status)) {
        fp.setBeginIndex(cfpos.getStart());
        fp.setEndIndex(cfpos.getLimit());
        return TRUE;
    }

    // Special case: fraction should start after integer if fraction is not present
    if (rawField == UNUM_FRACTION_FIELD && fp.getEndIndex() == 0) {
        bool inside = false;
        int32_t i = fString.fZero;
        for (; i < fString.fZero + fString.fLength; i++) {
            if (isIntOrGroup(fString.getFieldPtr()[i]) || fString.getFieldPtr()[i] == UNUM_DECIMAL_SEPARATOR_FIELD) {
                inside = true;
            } else if (inside) {
                break;
            }
        }
        fp.setBeginIndex(i - fString.fZero);
        fp.setEndIndex(i - fString.fZero);
    }

    return FALSE;
}

void FormattedValueStringBuilderImpl::getAllFieldPositions(FieldPositionIteratorHandler& fpih,
                                               UErrorCode& status) const {
    ConstrainedFieldPosition cfpos;
    while (nextPositionImpl(cfpos, 0, status)) {
        fpih.addAttribute(cfpos.getField(), cfpos.getStart(), cfpos.getLimit());
    }
}

// Signal the end of the string using a field that doesn't exist and that is
// different from UNUM_FIELD_COUNT, which is used for "null number field".
static constexpr Field kEndField = 0xff;

bool FormattedValueStringBuilderImpl::nextPositionImpl(ConstrainedFieldPosition& cfpos, Field numericField, UErrorCode& /*status*/) const {
    auto numericCAF = StringBuilderFieldUtils::expand(numericField);
    int32_t fieldStart = -1;
    Field currField = UNUM_FIELD_COUNT;
    for (int32_t i = fString.fZero + cfpos.getLimit(); i <= fString.fZero + fString.fLength; i++) {
        Field _field = (i < fString.fZero + fString.fLength) ? fString.getFieldPtr()[i] : kEndField;
        // Case 1: currently scanning a field.
        if (currField != UNUM_FIELD_COUNT) {
            if (currField != _field) {
                int32_t end = i - fString.fZero;
                // Grouping separators can be whitespace; don't throw them out!
                if (currField != UNUM_GROUPING_SEPARATOR_FIELD) {
                    end = trimBack(i - fString.fZero);
                }
                if (end <= fieldStart) {
                    // Entire field position is ignorable; skip.
                    fieldStart = -1;
                    currField = UNUM_FIELD_COUNT;
                    i--;  // look at this index again
                    continue;
                }
                int32_t start = fieldStart;
                if (currField != UNUM_GROUPING_SEPARATOR_FIELD) {
                    start = trimFront(start);
                }
                auto caf = StringBuilderFieldUtils::expand(currField);
                cfpos.setState(caf.category, caf.field, start, end);
                return true;
            }
            continue;
        }
        // Special case: coalesce the INTEGER if we are pointing at the end of the INTEGER.
        if (cfpos.matchesField(UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD)
                && i > fString.fZero
                // don't return the same field twice in a row:
                && i - fString.fZero > cfpos.getLimit()
                && isIntOrGroup(fString.getFieldPtr()[i - 1])
                && !isIntOrGroup(_field)) {
            int j = i - 1;
            for (; j >= fString.fZero && isIntOrGroup(fString.getFieldPtr()[j]); j--) {}
            cfpos.setState(
                UFIELD_CATEGORY_NUMBER,
                UNUM_INTEGER_FIELD,
                j - fString.fZero + 1,
                i - fString.fZero);
            return true;
        }
        // Special case: coalesce NUMERIC if we are pointing at the end of the NUMERIC.
        if (numericField != 0
                && cfpos.matchesField(numericCAF.category, numericCAF.field)
                && i > fString.fZero
                // don't return the same field twice in a row:
                && (i - fString.fZero > cfpos.getLimit()
                    || cfpos.getCategory() != numericCAF.category
                    || cfpos.getField() != numericCAF.field)
                && isNumericField(fString.getFieldPtr()[i - 1])
                && !isNumericField(_field)) {
            int j = i - 1;
            for (; j >= fString.fZero && isNumericField(fString.getFieldPtr()[j]); j--) {}
            cfpos.setState(
                numericCAF.category,
                numericCAF.field,
                j - fString.fZero + 1,
                i - fString.fZero);
            return true;
        }
        // Special case: skip over INTEGER; will be coalesced later.
        if (_field == UNUM_INTEGER_FIELD) {
            _field = UNUM_FIELD_COUNT;
        }
        // Case 2: no field starting at this position.
        if (_field == UNUM_FIELD_COUNT || _field == kEndField) {
            continue;
        }
        // Case 3: check for field starting at this position
        auto caf = StringBuilderFieldUtils::expand(_field);
        if (cfpos.matchesField(caf.category, caf.field)) {
            fieldStart = i - fString.fZero;
            currField = _field;
        }
    }

    U_ASSERT(currField == UNUM_FIELD_COUNT);
    return false;
}

bool FormattedValueStringBuilderImpl::isIntOrGroup(Field field) {
    return field == UNUM_INTEGER_FIELD
        || field == UNUM_GROUPING_SEPARATOR_FIELD;
}

bool FormattedValueStringBuilderImpl::isNumericField(Field field) {
    return StringBuilderFieldUtils::isNumericField(field);
}

int32_t FormattedValueStringBuilderImpl::trimBack(int32_t limit) const {
    return unisets::get(unisets::DEFAULT_IGNORABLES)->spanBack(
        fString.getCharPtr() + fString.fZero,
        limit,
        USET_SPAN_CONTAINED);
}

int32_t FormattedValueStringBuilderImpl::trimFront(int32_t start) const {
    return start + unisets::get(unisets::DEFAULT_IGNORABLES)->span(
        fString.getCharPtr() + fString.fZero + start,
        fString.fLength - start,
        USET_SPAN_CONTAINED);
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
