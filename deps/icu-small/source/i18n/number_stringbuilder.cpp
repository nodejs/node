// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_stringbuilder.h"
#include "static_unicode_sets.h"
#include "unicode/utf16.h"
#include "number_utils.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

// A version of uprv_memcpy that checks for length 0.
// By default, uprv_memcpy requires a length of at least 1.
inline void uprv_memcpy2(void* dest, const void* src, size_t len) {
    if (len > 0) {
        uprv_memcpy(dest, src, len);
    }
}

// A version of uprv_memmove that checks for length 0.
// By default, uprv_memmove requires a length of at least 1.
inline void uprv_memmove2(void* dest, const void* src, size_t len) {
    if (len > 0) {
        uprv_memmove(dest, src, len);
    }
}

} // namespace

NumberStringBuilder::NumberStringBuilder() {
#if U_DEBUG
    // Initializing the memory to non-zero helps catch some bugs that involve
    // reading from an improperly terminated string.
    for (int32_t i=0; i<getCapacity(); i++) {
        getCharPtr()[i] = 1;
    }
#endif
}

NumberStringBuilder::~NumberStringBuilder() {
    if (fUsingHeap) {
        uprv_free(fChars.heap.ptr);
        uprv_free(fFields.heap.ptr);
    }
}

NumberStringBuilder::NumberStringBuilder(const NumberStringBuilder &other) {
    *this = other;
}

NumberStringBuilder &NumberStringBuilder::operator=(const NumberStringBuilder &other) {
    // Check for self-assignment
    if (this == &other) {
        return *this;
    }

    // Continue with deallocation and copying
    if (fUsingHeap) {
        uprv_free(fChars.heap.ptr);
        uprv_free(fFields.heap.ptr);
        fUsingHeap = false;
    }

    int32_t capacity = other.getCapacity();
    if (capacity > DEFAULT_CAPACITY) {
        // FIXME: uprv_malloc
        // C++ note: malloc appears in two places: here and in prepareForInsertHelper.
        auto newChars = static_cast<char16_t *> (uprv_malloc(sizeof(char16_t) * capacity));
        auto newFields = static_cast<Field *>(uprv_malloc(sizeof(Field) * capacity));
        if (newChars == nullptr || newFields == nullptr) {
            // UErrorCode is not available; fail silently.
            uprv_free(newChars);
            uprv_free(newFields);
            *this = NumberStringBuilder();  // can't fail
            return *this;
        }

        fUsingHeap = true;
        fChars.heap.capacity = capacity;
        fChars.heap.ptr = newChars;
        fFields.heap.capacity = capacity;
        fFields.heap.ptr = newFields;
    }

    uprv_memcpy2(getCharPtr(), other.getCharPtr(), sizeof(char16_t) * capacity);
    uprv_memcpy2(getFieldPtr(), other.getFieldPtr(), sizeof(Field) * capacity);

    fZero = other.fZero;
    fLength = other.fLength;
    return *this;
}

int32_t NumberStringBuilder::length() const {
    return fLength;
}

int32_t NumberStringBuilder::codePointCount() const {
    return u_countChar32(getCharPtr() + fZero, fLength);
}

UChar32 NumberStringBuilder::getFirstCodePoint() const {
    if (fLength == 0) {
        return -1;
    }
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, 0, fLength, cp);
    return cp;
}

UChar32 NumberStringBuilder::getLastCodePoint() const {
    if (fLength == 0) {
        return -1;
    }
    int32_t offset = fLength;
    U16_BACK_1(getCharPtr() + fZero, 0, offset);
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, offset, fLength, cp);
    return cp;
}

UChar32 NumberStringBuilder::codePointAt(int32_t index) const {
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, index, fLength, cp);
    return cp;
}

UChar32 NumberStringBuilder::codePointBefore(int32_t index) const {
    int32_t offset = index;
    U16_BACK_1(getCharPtr() + fZero, 0, offset);
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, offset, fLength, cp);
    return cp;
}

NumberStringBuilder &NumberStringBuilder::clear() {
    // TODO: Reset the heap here?
    fZero = getCapacity() / 2;
    fLength = 0;
    return *this;
}

int32_t NumberStringBuilder::appendCodePoint(UChar32 codePoint, Field field, UErrorCode &status) {
    return insertCodePoint(fLength, codePoint, field, status);
}

int32_t
NumberStringBuilder::insertCodePoint(int32_t index, UChar32 codePoint, Field field, UErrorCode &status) {
    int32_t count = U16_LENGTH(codePoint);
    int32_t position = prepareForInsert(index, count, status);
    if (U_FAILURE(status)) {
        return count;
    }
    if (count == 1) {
        getCharPtr()[position] = (char16_t) codePoint;
        getFieldPtr()[position] = field;
    } else {
        getCharPtr()[position] = U16_LEAD(codePoint);
        getCharPtr()[position + 1] = U16_TRAIL(codePoint);
        getFieldPtr()[position] = getFieldPtr()[position + 1] = field;
    }
    return count;
}

int32_t NumberStringBuilder::append(const UnicodeString &unistr, Field field, UErrorCode &status) {
    return insert(fLength, unistr, field, status);
}

int32_t NumberStringBuilder::insert(int32_t index, const UnicodeString &unistr, Field field,
                                    UErrorCode &status) {
    if (unistr.length() == 0) {
        // Nothing to insert.
        return 0;
    } else if (unistr.length() == 1) {
        // Fast path: insert using insertCodePoint.
        return insertCodePoint(index, unistr.charAt(0), field, status);
    } else {
        return insert(index, unistr, 0, unistr.length(), field, status);
    }
}

int32_t
NumberStringBuilder::insert(int32_t index, const UnicodeString &unistr, int32_t start, int32_t end,
                            Field field, UErrorCode &status) {
    int32_t count = end - start;
    int32_t position = prepareForInsert(index, count, status);
    if (U_FAILURE(status)) {
        return count;
    }
    for (int32_t i = 0; i < count; i++) {
        getCharPtr()[position + i] = unistr.charAt(start + i);
        getFieldPtr()[position + i] = field;
    }
    return count;
}

int32_t
NumberStringBuilder::splice(int32_t startThis, int32_t endThis,  const UnicodeString &unistr,
                            int32_t startOther, int32_t endOther, Field field, UErrorCode& status) {
    int32_t thisLength = endThis - startThis;
    int32_t otherLength = endOther - startOther;
    int32_t count = otherLength - thisLength;
    int32_t position;
    if (count > 0) {
        // Overall, chars need to be added.
        position = prepareForInsert(startThis, count, status);
    } else {
        // Overall, chars need to be removed or kept the same.
        position = remove(startThis, -count);
    }
    if (U_FAILURE(status)) {
        return count;
    }
    for (int32_t i = 0; i < otherLength; i++) {
        getCharPtr()[position + i] = unistr.charAt(startOther + i);
        getFieldPtr()[position + i] = field;
    }
    return count;
}

int32_t NumberStringBuilder::append(const NumberStringBuilder &other, UErrorCode &status) {
    return insert(fLength, other, status);
}

int32_t
NumberStringBuilder::insert(int32_t index, const NumberStringBuilder &other, UErrorCode &status) {
    if (this == &other) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int32_t count = other.fLength;
    if (count == 0) {
        // Nothing to insert.
        return 0;
    }
    int32_t position = prepareForInsert(index, count, status);
    if (U_FAILURE(status)) {
        return count;
    }
    for (int32_t i = 0; i < count; i++) {
        getCharPtr()[position + i] = other.charAt(i);
        getFieldPtr()[position + i] = other.fieldAt(i);
    }
    return count;
}

void NumberStringBuilder::writeTerminator(UErrorCode& status) {
    int32_t position = prepareForInsert(fLength, 1, status);
    if (U_FAILURE(status)) {
        return;
    }
    getCharPtr()[position] = 0;
    getFieldPtr()[position] = UNUM_FIELD_COUNT;
    fLength--;
}

int32_t NumberStringBuilder::prepareForInsert(int32_t index, int32_t count, UErrorCode &status) {
    U_ASSERT(index >= 0);
    U_ASSERT(index <= fLength);
    U_ASSERT(count >= 0);
    if (index == 0 && fZero - count >= 0) {
        // Append to start
        fZero -= count;
        fLength += count;
        return fZero;
    } else if (index == fLength && fZero + fLength + count < getCapacity()) {
        // Append to end
        fLength += count;
        return fZero + fLength - count;
    } else {
        // Move chars around and/or allocate more space
        return prepareForInsertHelper(index, count, status);
    }
}

int32_t NumberStringBuilder::prepareForInsertHelper(int32_t index, int32_t count, UErrorCode &status) {
    int32_t oldCapacity = getCapacity();
    int32_t oldZero = fZero;
    char16_t *oldChars = getCharPtr();
    Field *oldFields = getFieldPtr();
    if (fLength + count > oldCapacity) {
        int32_t newCapacity = (fLength + count) * 2;
        int32_t newZero = newCapacity / 2 - (fLength + count) / 2;

        // C++ note: malloc appears in two places: here and in the assignment operator.
        auto newChars = static_cast<char16_t *> (uprv_malloc(sizeof(char16_t) * newCapacity));
        auto newFields = static_cast<Field *>(uprv_malloc(sizeof(Field) * newCapacity));
        if (newChars == nullptr || newFields == nullptr) {
            uprv_free(newChars);
            uprv_free(newFields);
            status = U_MEMORY_ALLOCATION_ERROR;
            return -1;
        }

        // First copy the prefix and then the suffix, leaving room for the new chars that the
        // caller wants to insert.
        // C++ note: memcpy is OK because the src and dest do not overlap.
        uprv_memcpy2(newChars + newZero, oldChars + oldZero, sizeof(char16_t) * index);
        uprv_memcpy2(newChars + newZero + index + count,
                oldChars + oldZero + index,
                sizeof(char16_t) * (fLength - index));
        uprv_memcpy2(newFields + newZero, oldFields + oldZero, sizeof(Field) * index);
        uprv_memcpy2(newFields + newZero + index + count,
                oldFields + oldZero + index,
                sizeof(Field) * (fLength - index));

        if (fUsingHeap) {
            uprv_free(oldChars);
            uprv_free(oldFields);
        }
        fUsingHeap = true;
        fChars.heap.ptr = newChars;
        fChars.heap.capacity = newCapacity;
        fFields.heap.ptr = newFields;
        fFields.heap.capacity = newCapacity;
        fZero = newZero;
        fLength += count;
    } else {
        int32_t newZero = oldCapacity / 2 - (fLength + count) / 2;

        // C++ note: memmove is required because src and dest may overlap.
        // First copy the entire string to the location of the prefix, and then move the suffix
        // to make room for the new chars that the caller wants to insert.
        uprv_memmove2(oldChars + newZero, oldChars + oldZero, sizeof(char16_t) * fLength);
        uprv_memmove2(oldChars + newZero + index + count,
                oldChars + newZero + index,
                sizeof(char16_t) * (fLength - index));
        uprv_memmove2(oldFields + newZero, oldFields + oldZero, sizeof(Field) * fLength);
        uprv_memmove2(oldFields + newZero + index + count,
                oldFields + newZero + index,
                sizeof(Field) * (fLength - index));

        fZero = newZero;
        fLength += count;
    }
    return fZero + index;
}

int32_t NumberStringBuilder::remove(int32_t index, int32_t count) {
    // TODO: Reset the heap here?  (If the string after removal can fit on stack?)
    int32_t position = index + fZero;
    uprv_memmove2(getCharPtr() + position,
            getCharPtr() + position + count,
            sizeof(char16_t) * (fLength - index - count));
    uprv_memmove2(getFieldPtr() + position,
            getFieldPtr() + position + count,
            sizeof(Field) * (fLength - index - count));
    fLength -= count;
    return position;
}

UnicodeString NumberStringBuilder::toUnicodeString() const {
    return UnicodeString(getCharPtr() + fZero, fLength);
}

const UnicodeString NumberStringBuilder::toTempUnicodeString() const {
    // Readonly-alias constructor:
    return UnicodeString(FALSE, getCharPtr() + fZero, fLength);
}

UnicodeString NumberStringBuilder::toDebugString() const {
    UnicodeString sb;
    sb.append(u"<NumberStringBuilder [", -1);
    sb.append(toUnicodeString());
    sb.append(u"] [", -1);
    for (int i = 0; i < fLength; i++) {
        if (fieldAt(i) == UNUM_FIELD_COUNT) {
            sb.append(u'n');
        } else {
            char16_t c;
            switch (fieldAt(i)) {
                case UNUM_SIGN_FIELD:
                    c = u'-';
                    break;
                case UNUM_INTEGER_FIELD:
                    c = u'i';
                    break;
                case UNUM_FRACTION_FIELD:
                    c = u'f';
                    break;
                case UNUM_EXPONENT_FIELD:
                    c = u'e';
                    break;
                case UNUM_EXPONENT_SIGN_FIELD:
                    c = u'+';
                    break;
                case UNUM_EXPONENT_SYMBOL_FIELD:
                    c = u'E';
                    break;
                case UNUM_DECIMAL_SEPARATOR_FIELD:
                    c = u'.';
                    break;
                case UNUM_GROUPING_SEPARATOR_FIELD:
                    c = u',';
                    break;
                case UNUM_PERCENT_FIELD:
                    c = u'%';
                    break;
                case UNUM_PERMILL_FIELD:
                    c = u'‰';
                    break;
                case UNUM_CURRENCY_FIELD:
                    c = u'$';
                    break;
                default:
                    c = u'?';
                    break;
            }
            sb.append(c);
        }
    }
    sb.append(u"]>", -1);
    return sb;
}

const char16_t *NumberStringBuilder::chars() const {
    return getCharPtr() + fZero;
}

bool NumberStringBuilder::contentEquals(const NumberStringBuilder &other) const {
    if (fLength != other.fLength) {
        return false;
    }
    for (int32_t i = 0; i < fLength; i++) {
        if (charAt(i) != other.charAt(i) || fieldAt(i) != other.fieldAt(i)) {
            return false;
        }
    }
    return true;
}

bool NumberStringBuilder::nextFieldPosition(FieldPosition& fp, UErrorCode& status) const {
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
    if (nextPosition(cfpos, 0, status)) {
        fp.setBeginIndex(cfpos.getStart());
        fp.setEndIndex(cfpos.getLimit());
        return true;
    }

    // Special case: fraction should start after integer if fraction is not present
    if (rawField == UNUM_FRACTION_FIELD && fp.getEndIndex() == 0) {
        bool inside = false;
        int32_t i = fZero;
        for (; i < fZero + fLength; i++) {
            if (isIntOrGroup(getFieldPtr()[i]) || getFieldPtr()[i] == UNUM_DECIMAL_SEPARATOR_FIELD) {
                inside = true;
            } else if (inside) {
                break;
            }
        }
        fp.setBeginIndex(i - fZero);
        fp.setEndIndex(i - fZero);
    }

    return false;
}

void NumberStringBuilder::getAllFieldPositions(FieldPositionIteratorHandler& fpih,
                                               UErrorCode& status) const {
    ConstrainedFieldPosition cfpos;
    while (nextPosition(cfpos, 0, status)) {
        fpih.addAttribute(cfpos.getField(), cfpos.getStart(), cfpos.getLimit());
    }
}

// Signal the end of the string using a field that doesn't exist and that is
// different from UNUM_FIELD_COUNT, which is used for "null number field".
static constexpr Field kEndField = 0xff;

bool NumberStringBuilder::nextPosition(ConstrainedFieldPosition& cfpos, Field numericField, UErrorCode& /*status*/) const {
    auto numericCAF = NumFieldUtils::expand(numericField);
    int32_t fieldStart = -1;
    Field currField = UNUM_FIELD_COUNT;
    for (int32_t i = fZero + cfpos.getLimit(); i <= fZero + fLength; i++) {
        Field _field = (i < fZero + fLength) ? getFieldPtr()[i] : kEndField;
        // Case 1: currently scanning a field.
        if (currField != UNUM_FIELD_COUNT) {
            if (currField != _field) {
                int32_t end = i - fZero;
                // Grouping separators can be whitespace; don't throw them out!
                if (currField != UNUM_GROUPING_SEPARATOR_FIELD) {
                    end = trimBack(i - fZero);
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
                auto caf = NumFieldUtils::expand(currField);
                cfpos.setState(caf.category, caf.field, start, end);
                return true;
            }
            continue;
        }
        // Special case: coalesce the INTEGER if we are pointing at the end of the INTEGER.
        if (cfpos.matchesField(UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD)
                && i > fZero
                // don't return the same field twice in a row:
                && i - fZero > cfpos.getLimit()
                && isIntOrGroup(getFieldPtr()[i - 1])
                && !isIntOrGroup(_field)) {
            int j = i - 1;
            for (; j >= fZero && isIntOrGroup(getFieldPtr()[j]); j--) {}
            cfpos.setState(UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD, j - fZero + 1, i - fZero);
            return true;
        }
        // Special case: coalesce NUMERIC if we are pointing at the end of the NUMERIC.
        if (numericField != 0
                && cfpos.matchesField(numericCAF.category, numericCAF.field)
                && i > fZero
                // don't return the same field twice in a row:
                && (i - fZero > cfpos.getLimit()
                    || cfpos.getCategory() != numericCAF.category
                    || cfpos.getField() != numericCAF.field)
                && isNumericField(getFieldPtr()[i - 1])
                && !isNumericField(_field)) {
            int j = i - 1;
            for (; j >= fZero && isNumericField(getFieldPtr()[j]); j--) {}
            cfpos.setState(numericCAF.category, numericCAF.field, j - fZero + 1, i - fZero);
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
        auto caf = NumFieldUtils::expand(_field);
        if (cfpos.matchesField(caf.category, caf.field)) {
            fieldStart = i - fZero;
            currField = _field;
        }
    }

    U_ASSERT(currField == UNUM_FIELD_COUNT);
    return false;
}

bool NumberStringBuilder::containsField(Field field) const {
    for (int32_t i = 0; i < fLength; i++) {
        if (field == fieldAt(i)) {
            return true;
        }
    }
    return false;
}

bool NumberStringBuilder::isIntOrGroup(Field field) {
    return field == UNUM_INTEGER_FIELD
        || field == UNUM_GROUPING_SEPARATOR_FIELD;
}

bool NumberStringBuilder::isNumericField(Field field) {
    return NumFieldUtils::isNumericField(field);
}

int32_t NumberStringBuilder::trimBack(int32_t limit) const {
    return unisets::get(unisets::DEFAULT_IGNORABLES)->spanBack(
        getCharPtr() + fZero,
        limit,
        USET_SPAN_CONTAINED);
}

int32_t NumberStringBuilder::trimFront(int32_t start) const {
    return start + unisets::get(unisets::DEFAULT_IGNORABLES)->span(
        getCharPtr() + fZero + start,
        fLength - start,
        USET_SPAN_CONTAINED);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
