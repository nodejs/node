// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "formatted_string_builder.h"
#include "putilimp.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "unicode/unum.h" // for UNumberFormatFields literals

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


U_NAMESPACE_BEGIN

FormattedStringBuilder::FormattedStringBuilder() {
#if U_DEBUG
    // Initializing the memory to non-zero helps catch some bugs that involve
    // reading from an improperly terminated string.
    for (int32_t i=0; i<getCapacity(); i++) {
        getCharPtr()[i] = 1;
    }
#endif
}

FormattedStringBuilder::~FormattedStringBuilder() {
    if (fUsingHeap) {
        uprv_free(fChars.heap.ptr);
        uprv_free(fFields.heap.ptr);
    }
}

FormattedStringBuilder::FormattedStringBuilder(const FormattedStringBuilder &other) {
    *this = other;
}

FormattedStringBuilder &FormattedStringBuilder::operator=(const FormattedStringBuilder &other) {
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
        auto* newChars = static_cast<char16_t*>(uprv_malloc(sizeof(char16_t) * capacity));
        auto* newFields = static_cast<Field*>(uprv_malloc(sizeof(Field) * capacity));
        if (newChars == nullptr || newFields == nullptr) {
            // UErrorCode is not available; fail silently.
            uprv_free(newChars);
            uprv_free(newFields);
            *this = FormattedStringBuilder();  // can't fail
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

int32_t FormattedStringBuilder::length() const {
    return fLength;
}

int32_t FormattedStringBuilder::codePointCount() const {
    return u_countChar32(getCharPtr() + fZero, fLength);
}

UChar32 FormattedStringBuilder::getFirstCodePoint() const {
    if (fLength == 0) {
        return -1;
    }
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, 0, fLength, cp);
    return cp;
}

UChar32 FormattedStringBuilder::getLastCodePoint() const {
    if (fLength == 0) {
        return -1;
    }
    int32_t offset = fLength;
    U16_BACK_1(getCharPtr() + fZero, 0, offset);
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, offset, fLength, cp);
    return cp;
}

UChar32 FormattedStringBuilder::codePointAt(int32_t index) const {
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, index, fLength, cp);
    return cp;
}

UChar32 FormattedStringBuilder::codePointBefore(int32_t index) const {
    int32_t offset = index;
    U16_BACK_1(getCharPtr() + fZero, 0, offset);
    UChar32 cp;
    U16_GET(getCharPtr() + fZero, 0, offset, fLength, cp);
    return cp;
}

FormattedStringBuilder &FormattedStringBuilder::clear() {
    // TODO: Reset the heap here?
    fZero = getCapacity() / 2;
    fLength = 0;
    return *this;
}

int32_t
FormattedStringBuilder::insertCodePoint(int32_t index, UChar32 codePoint, Field field, UErrorCode &status) {
    int32_t count = U16_LENGTH(codePoint);
    int32_t position = prepareForInsert(index, count, status);
    if (U_FAILURE(status)) {
        return count;
    }
    auto* charPtr = getCharPtr();
    auto* fieldPtr = getFieldPtr();
    if (count == 1) {
        charPtr[position] = static_cast<char16_t>(codePoint);
        fieldPtr[position] = field;
    } else {
        charPtr[position] = U16_LEAD(codePoint);
        charPtr[position + 1] = U16_TRAIL(codePoint);
        fieldPtr[position] = fieldPtr[position + 1] = field;
    }
    return count;
}

int32_t FormattedStringBuilder::insert(int32_t index, const UnicodeString &unistr, Field field,
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
FormattedStringBuilder::insert(int32_t index, const UnicodeString &unistr, int32_t start, int32_t end,
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
FormattedStringBuilder::splice(int32_t startThis, int32_t endThis,  const UnicodeString &unistr,
                            int32_t startOther, int32_t endOther, Field field, UErrorCode& status) {
    int32_t thisLength = endThis - startThis;
    int32_t otherLength = endOther - startOther;
    int32_t count = otherLength - thisLength;
    if (U_FAILURE(status)) {
        return count;
    }
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

int32_t FormattedStringBuilder::append(const FormattedStringBuilder &other, UErrorCode &status) {
    return insert(fLength, other, status);
}

int32_t
FormattedStringBuilder::insert(int32_t index, const FormattedStringBuilder &other, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
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

void FormattedStringBuilder::writeTerminator(UErrorCode& status) {
    int32_t position = prepareForInsert(fLength, 1, status);
    if (U_FAILURE(status)) {
        return;
    }
    getCharPtr()[position] = 0;
    getFieldPtr()[position] = kUndefinedField;
    fLength--;
}

int32_t FormattedStringBuilder::prepareForInsert(int32_t index, int32_t count, UErrorCode &status) {
    U_ASSERT(index >= 0);
    U_ASSERT(index <= fLength);
    U_ASSERT(count >= 0);
    U_ASSERT(fZero >= 0);
    U_ASSERT(fLength >= 0);
    U_ASSERT(getCapacity() - fZero >= fLength);
    if (U_FAILURE(status)) {
        return count;
    }
    if (index == 0 && fZero - count >= 0) {
        // Append to start
        fZero -= count;
        fLength += count;
        return fZero;
    } else if (index == fLength && count <= getCapacity() - fZero - fLength) {
        // Append to end
        fLength += count;
        return fZero + fLength - count;
    } else {
        // Move chars around and/or allocate more space
        return prepareForInsertHelper(index, count, status);
    }
}

int32_t FormattedStringBuilder::prepareForInsertHelper(int32_t index, int32_t count, UErrorCode &status) {
    int32_t oldCapacity = getCapacity();
    int32_t oldZero = fZero;
    char16_t *oldChars = getCharPtr();
    Field *oldFields = getFieldPtr();
    int32_t newLength;
    if (uprv_add32_overflow(fLength, count, &newLength)) {
        status = U_INPUT_TOO_LONG_ERROR;
        return -1;
    }
    int32_t newZero;
    if (newLength > oldCapacity) {
        if (newLength > INT32_MAX / 2) {
            // We do not support more than 1G char16_t in this code because
            // dealing with >2G *bytes* can cause subtle bugs.
            status = U_INPUT_TOO_LONG_ERROR;
            return -1;
        }
        // Keep newCapacity also to at most 1G char16_t.
        int32_t newCapacity = newLength * 2;
        newZero = (newCapacity - newLength) / 2;

        // C++ note: malloc appears in two places: here and in the assignment operator.
        auto* newChars =
            static_cast<char16_t*>(uprv_malloc(sizeof(char16_t) * static_cast<size_t>(newCapacity)));
        auto* newFields =
            static_cast<Field*>(uprv_malloc(sizeof(Field) * static_cast<size_t>(newCapacity)));
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
    } else {
        newZero = (oldCapacity - newLength) / 2;

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
    }
    fZero = newZero;
    fLength = newLength;
    return fZero + index;
}

int32_t FormattedStringBuilder::remove(int32_t index, int32_t count) {
     U_ASSERT(0 <= index);
     U_ASSERT(index <= fLength);
     U_ASSERT(count <= (fLength - index));
     U_ASSERT(index <= getCapacity() - fZero);

    int32_t position = index + fZero;
    // TODO: Reset the heap here?  (If the string after removal can fit on stack?)
    uprv_memmove2(getCharPtr() + position,
            getCharPtr() + position + count,
            sizeof(char16_t) * (fLength - index - count));
    uprv_memmove2(getFieldPtr() + position,
            getFieldPtr() + position + count,
            sizeof(Field) * (fLength - index - count));
    fLength -= count;
    return position;
}

UnicodeString FormattedStringBuilder::toUnicodeString() const {
    return UnicodeString(getCharPtr() + fZero, fLength);
}

UnicodeString FormattedStringBuilder::toTempUnicodeString() const {
    // Readonly-alias constructor:
    return UnicodeString(false, getCharPtr() + fZero, fLength);
}

UnicodeString FormattedStringBuilder::toDebugString() const {
    UnicodeString sb;
    sb.append(u"<FormattedStringBuilder [", -1);
    sb.append(toUnicodeString());
    sb.append(u"] [", -1);
    for (int i = 0; i < fLength; i++) {
        if (fieldAt(i) == kUndefinedField) {
            sb.append(u'n');
        } else if (fieldAt(i).getCategory() == UFIELD_CATEGORY_NUMBER) {
            char16_t c;
            switch (fieldAt(i).getField()) {
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
                    c = u'0' + fieldAt(i).getField();
                    break;
            }
            sb.append(c);
        } else {
            sb.append(u'0' + fieldAt(i).getCategory());
        }
    }
    sb.append(u"]>", -1);
    return sb;
}

const char16_t *FormattedStringBuilder::chars() const {
    return getCharPtr() + fZero;
}

bool FormattedStringBuilder::contentEquals(const FormattedStringBuilder &other) const {
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

bool FormattedStringBuilder::containsField(Field field) const {
    for (int32_t i = 0; i < fLength; i++) {
        if (field == fieldAt(i)) {
            return true;
        }
    }
    return false;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
