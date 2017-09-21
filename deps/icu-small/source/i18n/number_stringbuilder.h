// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_STRINGBUILDER_H__
#define __NUMBER_STRINGBUILDER_H__


#include <cstdint>
#include "unicode/numfmt.h"
#include "unicode/ustring.h"
#include "cstring.h"
#include "uassert.h"
#include "number_types.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

class U_I18N_API NumberStringBuilder : public UMemory {
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
    NumberStringBuilder();

    ~NumberStringBuilder();

    NumberStringBuilder(const NumberStringBuilder &other);

    NumberStringBuilder &operator=(const NumberStringBuilder &other);

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

    NumberStringBuilder &clear();

    int32_t appendCodePoint(UChar32 codePoint, Field field, UErrorCode &status);

    int32_t insertCodePoint(int32_t index, UChar32 codePoint, Field field, UErrorCode &status);

    int32_t append(const UnicodeString &unistr, Field field, UErrorCode &status);

    int32_t insert(int32_t index, const UnicodeString &unistr, Field field, UErrorCode &status);

    int32_t insert(int32_t index, const UnicodeString &unistr, int32_t start, int32_t end, Field field,
                   UErrorCode &status);

    int32_t append(const NumberStringBuilder &other, UErrorCode &status);

    int32_t insert(int32_t index, const NumberStringBuilder &other, UErrorCode &status);

    UnicodeString toUnicodeString() const;

    UnicodeString toDebugString() const;

    const char16_t *chars() const;

    bool contentEquals(const NumberStringBuilder &other) const;

    void populateFieldPosition(FieldPosition &fp, int32_t offset, UErrorCode &status) const;

    void populateFieldPositionIterator(FieldPositionIterator &fpi, UErrorCode &status) const;

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
};

} // namespace impl
} // namespace number
U_NAMESPACE_END


#endif //__NUMBER_STRINGBUILDER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
