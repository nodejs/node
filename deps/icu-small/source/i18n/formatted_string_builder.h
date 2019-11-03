// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_STRINGBUILDER_H__
#define __NUMBER_STRINGBUILDER_H__


#include <cstdint>
#include "unicode/unum.h" // for UNUM_FIELD_COUNT
#include "cstring.h"
#include "uassert.h"
#include "fphdlimp.h"

U_NAMESPACE_BEGIN

class FormattedValueStringBuilderImpl;

/**
 * A StringBuilder optimized for formatting. It implements the following key
 * features beyond a UnicodeString:
 *
 * <ol>
 * <li>Efficient prepend as well as append.
 * <li>Keeps tracks of Fields in an efficient manner.
 * </ol>
 *
 * See also FormattedValueStringBuilderImpl.
 *
 * @author sffc (Shane Carr)
 */
class U_I18N_API FormattedStringBuilder : public UMemory {
  private:
    static const int32_t DEFAULT_CAPACITY = 40;

    template<typename T>
    union ValueOrHeapArray {
        T value[DEFAULT_CAPACITY];
        struct {
            T *ptr;
            int32_t capacity;
        } heap;
    };

  public:
    FormattedStringBuilder();

    ~FormattedStringBuilder();

    FormattedStringBuilder(const FormattedStringBuilder &other);

    // Convention: bottom 4 bits for field, top 4 bits for field category.
    // Field category 0 implies the number category so that the number field
    // literals can be directly passed as a Field type.
    // See the helper functions in "StringBuilderFieldUtils" below.
    typedef uint8_t Field;

    FormattedStringBuilder &operator=(const FormattedStringBuilder &other);

    int32_t length() const;

    int32_t codePointCount() const;

    inline char16_t charAt(int32_t index) const {
        U_ASSERT(index >= 0);
        U_ASSERT(index < fLength);
        return getCharPtr()[fZero + index];
    }

    inline Field fieldAt(int32_t index) const {
        U_ASSERT(index >= 0);
        U_ASSERT(index < fLength);
        return getFieldPtr()[fZero + index];
    }

    UChar32 getFirstCodePoint() const;

    UChar32 getLastCodePoint() const;

    UChar32 codePointAt(int32_t index) const;

    UChar32 codePointBefore(int32_t index) const;

    FormattedStringBuilder &clear();

    /** Appends a UTF-16 code unit. */
    inline int32_t appendChar16(char16_t codeUnit, Field field, UErrorCode& status) {
        // appendCodePoint handles both code units and code points.
        return insertCodePoint(fLength, codeUnit, field, status);
    }

    /** Inserts a UTF-16 code unit. Note: insert at index 0 is very efficient. */
    inline int32_t insertChar16(int32_t index, char16_t codeUnit, Field field, UErrorCode& status) {
        // insertCodePoint handles both code units and code points.
        return insertCodePoint(index, codeUnit, field, status);
    }

    /** Appends a Unicode code point. */
    inline int32_t appendCodePoint(UChar32 codePoint, Field field, UErrorCode &status) {
        return insertCodePoint(fLength, codePoint, field, status);
    }

    /** Inserts a Unicode code point. Note: insert at index 0 is very efficient. */
    int32_t insertCodePoint(int32_t index, UChar32 codePoint, Field field, UErrorCode &status);

    /** Appends a string. */
    inline int32_t append(const UnicodeString &unistr, Field field, UErrorCode &status) {
        return insert(fLength, unistr, field, status);
    }

    /** Inserts a string. Note: insert at index 0 is very efficient. */
    int32_t insert(int32_t index, const UnicodeString &unistr, Field field, UErrorCode &status);

    /** Inserts a substring. Note: insert at index 0 is very efficient.
     *
     * @param start Start index of the substring of unistr to be inserted.
     * @param end End index of the substring of unistr to be inserted (exclusive).
     */
    int32_t insert(int32_t index, const UnicodeString &unistr, int32_t start, int32_t end, Field field,
                   UErrorCode &status);

    /** Deletes a substring and then inserts a string at that same position.
     * Similar to JavaScript Array.prototype.splice().
     *
     * @param startThis Start of the span to delete.
     * @param endThis End of the span to delete (exclusive).
     * @param unistr The string to insert at the deletion position.
     * @param startOther Start index of the substring of unistr to be inserted.
     * @param endOther End index of the substring of unistr to be inserted (exclusive).
     */
    int32_t splice(int32_t startThis, int32_t endThis,  const UnicodeString &unistr,
                   int32_t startOther, int32_t endOther, Field field, UErrorCode& status);

    /** Appends a formatted string. */
    int32_t append(const FormattedStringBuilder &other, UErrorCode &status);

    /** Inserts a formatted string. Note: insert at index 0 is very efficient. */
    int32_t insert(int32_t index, const FormattedStringBuilder &other, UErrorCode &status);

    /**
     * Ensures that the string buffer contains a NUL terminator. The NUL terminator does
     * not count toward the string length. Any further changes to the string (insert or
     * append) may invalidate the NUL terminator.
     *
     * You should call this method after the formatted string is completely built if you
     * plan to return a pointer to the string from a C API.
     */
    void writeTerminator(UErrorCode& status);

    /**
     * Gets a "safe" UnicodeString that can be used even after the FormattedStringBuilder is destructed.
     */
    UnicodeString toUnicodeString() const;

    /**
     * Gets an "unsafe" UnicodeString that is valid only as long as the FormattedStringBuilder is alive and
     * unchanged. Slightly faster than toUnicodeString().
     */
    const UnicodeString toTempUnicodeString() const;

    UnicodeString toDebugString() const;

    const char16_t *chars() const;

    bool contentEquals(const FormattedStringBuilder &other) const;

    bool containsField(Field field) const;

  private:
    bool fUsingHeap = false;
    ValueOrHeapArray<char16_t> fChars;
    ValueOrHeapArray<Field> fFields;
    int32_t fZero = DEFAULT_CAPACITY / 2;
    int32_t fLength = 0;

    inline char16_t *getCharPtr() {
        return fUsingHeap ? fChars.heap.ptr : fChars.value;
    }

    inline const char16_t *getCharPtr() const {
        return fUsingHeap ? fChars.heap.ptr : fChars.value;
    }

    inline Field *getFieldPtr() {
        return fUsingHeap ? fFields.heap.ptr : fFields.value;
    }

    inline const Field *getFieldPtr() const {
        return fUsingHeap ? fFields.heap.ptr : fFields.value;
    }

    inline int32_t getCapacity() const {
        return fUsingHeap ? fChars.heap.capacity : DEFAULT_CAPACITY;
    }

    int32_t prepareForInsert(int32_t index, int32_t count, UErrorCode &status);

    int32_t prepareForInsertHelper(int32_t index, int32_t count, UErrorCode &status);

    int32_t remove(int32_t index, int32_t count);

    friend class FormattedValueStringBuilderImpl;
};

/**
 * Helper functions for dealing with the Field typedef, which stores fields
 * in a compressed format.
 */
class StringBuilderFieldUtils {
public:
    struct CategoryFieldPair {
        int32_t category;
        int32_t field;
    };

    /** Compile-time function to construct a Field from a category and a field */
    template <int32_t category, int32_t field>
    static constexpr FormattedStringBuilder::Field compress() {
        static_assert(category != 0, "cannot use Undefined category in FieldUtils");
        static_assert(category <= 0xf, "only 4 bits for category");
        static_assert(field <= 0xf, "only 4 bits for field");
        return static_cast<int8_t>((category << 4) | field);
    }

    /** Runtime inline function to unpack the category and field from the Field */
    static inline CategoryFieldPair expand(FormattedStringBuilder::Field field) {
        if (field == UNUM_FIELD_COUNT) {
            return {UFIELD_CATEGORY_UNDEFINED, 0};
        }
        CategoryFieldPair ret = {
            (field >> 4),
            (field & 0xf)
        };
        if (ret.category == 0) {
            ret.category = UFIELD_CATEGORY_NUMBER;
        }
        return ret;
    }

    static inline bool isNumericField(FormattedStringBuilder::Field field) {
        int8_t category = field >> 4;
        return category == 0 || category == UFIELD_CATEGORY_NUMBER;
    }
};

U_NAMESPACE_END


#endif //__NUMBER_STRINGBUILDER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
