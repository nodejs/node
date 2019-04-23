// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// This file contains one implementation of FormattedValue.
// Other independent implementations should go into their own cpp file for
// better dependency modularization.

#include "formattedval_impl.h"
#include "putilimp.h"

U_NAMESPACE_BEGIN


FormattedValueFieldPositionIteratorImpl::FormattedValueFieldPositionIteratorImpl(
        int32_t initialFieldCapacity,
        UErrorCode& status)
        : fFields(initialFieldCapacity * 4, status) {
}

FormattedValueFieldPositionIteratorImpl::~FormattedValueFieldPositionIteratorImpl() = default;

UnicodeString FormattedValueFieldPositionIteratorImpl::toString(
        UErrorCode&) const {
    return fString;
}

UnicodeString FormattedValueFieldPositionIteratorImpl::toTempString(
        UErrorCode&) const {
    // The alias must point to memory owned by this object;
    // fastCopyFrom doesn't do this when using a stack buffer.
    return UnicodeString(TRUE, fString.getBuffer(), fString.length());
}

Appendable& FormattedValueFieldPositionIteratorImpl::appendTo(
        Appendable& appendable,
        UErrorCode&) const {
    appendable.appendString(fString.getBuffer(), fString.length());
    return appendable;
}

UBool FormattedValueFieldPositionIteratorImpl::nextPosition(
        ConstrainedFieldPosition& cfpos,
        UErrorCode&) const {
    U_ASSERT(fFields.size() % 4 == 0);
    int32_t numFields = fFields.size() / 4;
    int32_t i = static_cast<int32_t>(cfpos.getInt64IterationContext());
    for (; i < numFields; i++) {
        UFieldCategory category = static_cast<UFieldCategory>(fFields.elementAti(i * 4));
        int32_t field = fFields.elementAti(i * 4 + 1);
        if (cfpos.matchesField(category, field)) {
            int32_t start = fFields.elementAti(i * 4 + 2);
            int32_t limit = fFields.elementAti(i * 4 + 3);
            cfpos.setState(category, field, start, limit);
            break;
        }
    }
    cfpos.setInt64IterationContext(i == numFields ? i : i + 1);
    return i < numFields;
}


FieldPositionIteratorHandler FormattedValueFieldPositionIteratorImpl::getHandler(
        UErrorCode& status) {
    return FieldPositionIteratorHandler(&fFields, status);
}

void FormattedValueFieldPositionIteratorImpl::appendString(
        UnicodeString string,
        UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    fString.append(string);
    // Make the string NUL-terminated
    if (fString.getTerminatedBuffer() == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}


void FormattedValueFieldPositionIteratorImpl::addOverlapSpans(
        UFieldCategory spanCategory,
        int8_t firstIndex,
        UErrorCode& status) {
    // In order to avoid fancy data structures, this is an O(N^2) algorithm,
    // which should be fine for all real-life applications of this function.
    int32_t s1a = INT32_MAX;
    int32_t s1b = 0;
    int32_t s2a = INT32_MAX;
    int32_t s2b = 0;
    int32_t numFields = fFields.size() / 4;
    for (int32_t i = 0; i<numFields; i++) {
        int32_t field1 = fFields.elementAti(i * 4 + 1);
        for (int32_t j = i + 1; j<numFields; j++) {
            int32_t field2 = fFields.elementAti(j * 4 + 1);
            if (field1 != field2) {
                continue;
            }
            // Found a duplicate
            s1a = uprv_min(s1a, fFields.elementAti(i * 4 + 2));
            s1b = uprv_max(s1b, fFields.elementAti(i * 4 + 3));
            s2a = uprv_min(s2a, fFields.elementAti(j * 4 + 2));
            s2b = uprv_max(s2b, fFields.elementAti(j * 4 + 3));
            break;
        }
    }
    if (s1a != INT32_MAX) {
        // Success: add the two span fields
        fFields.addElement(spanCategory, status);
        fFields.addElement(firstIndex, status);
        fFields.addElement(s1a, status);
        fFields.addElement(s1b, status);
        fFields.addElement(spanCategory, status);
        fFields.addElement(1 - firstIndex, status);
        fFields.addElement(s2a, status);
        fFields.addElement(s2b, status);
    }
}


void FormattedValueFieldPositionIteratorImpl::sort() {
    // Use bubble sort, O(N^2) but easy and no fancy data structures.
    int32_t numFields = fFields.size() / 4;
    while (true) {
        bool isSorted = true;
        for (int32_t i=0; i<numFields-1; i++) {
            int32_t categ1 = fFields.elementAti(i*4 + 0);
            int32_t field1 = fFields.elementAti(i*4 + 1);
            int32_t start1 = fFields.elementAti(i*4 + 2);
            int32_t limit1 = fFields.elementAti(i*4 + 3);
            int32_t categ2 = fFields.elementAti(i*4 + 4);
            int32_t field2 = fFields.elementAti(i*4 + 5);
            int32_t start2 = fFields.elementAti(i*4 + 6);
            int32_t limit2 = fFields.elementAti(i*4 + 7);
            int64_t comparison = 0;
            if (start1 != start2) {
                // Higher start index -> higher rank
                comparison = start2 - start1;
            } else if (limit1 != limit2) {
                // Higher length (end index) -> lower rank
                comparison = limit1 - limit2;
            } else if (categ1 != categ2) {
                // Higher field category -> lower rank
                comparison = categ1 - categ2;
            } else if (field1 != field2) {
                // Higher field -> higher rank
                comparison = field2 - field1;
            }
            if (comparison < 0) {
                // Perform a swap
                isSorted = false;
                fFields.setElementAt(categ2, i*4 + 0);
                fFields.setElementAt(field2, i*4 + 1);
                fFields.setElementAt(start2, i*4 + 2);
                fFields.setElementAt(limit2, i*4 + 3);
                fFields.setElementAt(categ1, i*4 + 4);
                fFields.setElementAt(field1, i*4 + 5);
                fFields.setElementAt(start1, i*4 + 6);
                fFields.setElementAt(limit1, i*4 + 7);
            }
        }
        if (isSorted) {
            break;
        }
    }
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
