// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "number_stringbuilder.h"
#include "unicode/utf16.h"
#include "uvectr32.h"

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

NumberStringBuilder::NumberStringBuilder() = default;

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

int32_t NumberStringBuilder::prepareForInsert(int32_t index, int32_t count, UErrorCode &status) {
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

UnicodeString NumberStringBuilder::toUnicodeString() const {
    return UnicodeString(getCharPtr() + fZero, fLength);
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

void NumberStringBuilder::populateFieldPosition(FieldPosition &fp, int32_t offset, UErrorCode &status) const {
    int32_t rawField = fp.getField();

    if (rawField == FieldPosition::DONT_CARE) {
        return;
    }

    if (rawField < 0 || rawField >= UNUM_FIELD_COUNT) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    auto field = static_cast<Field>(rawField);

    bool seenStart = false;
    int32_t fractionStart = -1;
    for (int i = fZero; i <= fZero + fLength; i++) {
        Field _field = UNUM_FIELD_COUNT;
        if (i < fZero + fLength) {
            _field = getFieldPtr()[i];
        }
        if (seenStart && field != _field) {
            // Special case: GROUPING_SEPARATOR counts as an INTEGER.
            if (field == UNUM_INTEGER_FIELD && _field == UNUM_GROUPING_SEPARATOR_FIELD) {
                continue;
            }
            fp.setEndIndex(i - fZero + offset);
            break;
        } else if (!seenStart && field == _field) {
            fp.setBeginIndex(i - fZero + offset);
            seenStart = true;
        }
        if (_field == UNUM_INTEGER_FIELD || _field == UNUM_DECIMAL_SEPARATOR_FIELD) {
            fractionStart = i - fZero + 1;
        }
    }

    // Backwards compatibility: FRACTION needs to start after INTEGER if empty
    if (field == UNUM_FRACTION_FIELD && !seenStart) {
        fp.setBeginIndex(fractionStart + offset);
        fp.setEndIndex(fractionStart + offset);
    }
}

void NumberStringBuilder::populateFieldPositionIterator(FieldPositionIterator &fpi, UErrorCode &status) const {
    // TODO: Set an initial capacity on uvec?
    LocalPointer <UVector32> uvec(new UVector32(status));
    if (U_FAILURE(status)) {
        return;
    }

    Field current = UNUM_FIELD_COUNT;
    int32_t currentStart = -1;
    for (int32_t i = 0; i < fLength; i++) {
        Field field = fieldAt(i);
        if (current == UNUM_INTEGER_FIELD && field == UNUM_GROUPING_SEPARATOR_FIELD) {
            // Special case: GROUPING_SEPARATOR counts as an INTEGER.
            // Add the field, followed by the start index, followed by the end index to uvec.
            uvec->addElement(UNUM_GROUPING_SEPARATOR_FIELD, status);
            uvec->addElement(i, status);
            uvec->addElement(i + 1, status);
        } else if (current != field) {
            if (current != UNUM_FIELD_COUNT) {
                // Add the field, followed by the start index, followed by the end index to uvec.
                uvec->addElement(current, status);
                uvec->addElement(currentStart, status);
                uvec->addElement(i, status);
            }
            current = field;
            currentStart = i;
        }
        if (U_FAILURE(status)) {
            return;
        }
    }
    if (current != UNUM_FIELD_COUNT) {
        // Add the field, followed by the start index, followed by the end index to uvec.
        uvec->addElement(current, status);
        uvec->addElement(currentStart, status);
        uvec->addElement(fLength, status);
    }

    // Give uvec to the FieldPositionIterator, which adopts it.
    fpi.setData(uvec.orphan(), status);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
