// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (c) 2001-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   Date        Name        Description
*   11/19/2001  aliu        Creation.
*   05/19/2010  markus      Rewritten from scratch
**********************************************************************
*/

#ifndef CHARSTRING_H
#define CHARSTRING_H

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "unicode/uobject.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

// Windows needs us to DLL-export the MaybeStackArray template specialization,
// but MacOS X cannot handle it. Same as in digitlst.h.
#if !U_PLATFORM_IS_DARWIN_BASED
template class U_COMMON_API MaybeStackArray<char, 40>;
#endif

/**
 * ICU-internal char * string class.
 * This class does not assume or enforce any particular character encoding.
 * Raw bytes can be stored. The string object owns its characters.
 * A terminating NUL is stored, but the class does not prevent embedded NUL characters.
 *
 * This class wants to be convenient but is also deliberately minimalist.
 * Please do not add methods if they only add minor convenience.
 * For example:
 *   cs.data()[5]='a';  // no need for setCharAt(5, 'a')
 */
class U_COMMON_API CharString : public UMemory {
public:
    CharString() : len(0) { buffer[0]=0; }
    CharString(StringPiece s, UErrorCode &errorCode) : len(0) {
        buffer[0]=0;
        append(s, errorCode);
    }
    CharString(const CharString &s, UErrorCode &errorCode) : len(0) {
        buffer[0]=0;
        append(s, errorCode);
    }
    CharString(const char *s, int32_t sLength, UErrorCode &errorCode) : len(0) {
        buffer[0]=0;
        append(s, sLength, errorCode);
    }
    ~CharString() {}

    /**
     * Replaces this string's contents with the other string's contents.
     * CharString does not support the standard copy constructor nor
     * the assignment operator, to make copies explicit and to
     * use a UErrorCode where memory allocations might be needed.
     */
    CharString &copyFrom(const CharString &other, UErrorCode &errorCode);

    UBool isEmpty() const { return len==0; }
    int32_t length() const { return len; }
    char operator[](int32_t index) const { return buffer[index]; }
    StringPiece toStringPiece() const { return StringPiece(buffer.getAlias(), len); }

    const char *data() const { return buffer.getAlias(); }
    char *data() { return buffer.getAlias(); }

    /** @return last index of c, or -1 if c is not in this string */
    int32_t lastIndexOf(char c) const;

    CharString &clear() { len=0; buffer[0]=0; return *this; }
    CharString &truncate(int32_t newLength);

    CharString &append(char c, UErrorCode &errorCode);
    CharString &append(StringPiece s, UErrorCode &errorCode) {
        return append(s.data(), s.length(), errorCode);
    }
    CharString &append(const CharString &s, UErrorCode &errorCode) {
        return append(s.data(), s.length(), errorCode);
    }
    CharString &append(const char *s, int32_t sLength, UErrorCode &status);
    /**
     * Returns a writable buffer for appending and writes the buffer's capacity to
     * resultCapacity. Guarantees resultCapacity>=minCapacity if U_SUCCESS().
     * There will additionally be space for a terminating NUL right at resultCapacity.
     * (This function is similar to ByteSink.GetAppendBuffer().)
     *
     * The returned buffer is only valid until the next write operation
     * on this string.
     *
     * After writing at most resultCapacity bytes, call append() with the
     * pointer returned from this function and the number of bytes written.
     *
     * @param minCapacity required minimum capacity of the returned buffer;
     *                    must be non-negative
     * @param desiredCapacityHint desired capacity of the returned buffer;
     *                            must be non-negative
     * @param resultCapacity will be set to the capacity of the returned buffer
     * @param errorCode in/out error code
     * @return a buffer with resultCapacity>=min_capacity
     */
    char *getAppendBuffer(int32_t minCapacity,
                          int32_t desiredCapacityHint,
                          int32_t &resultCapacity,
                          UErrorCode &errorCode);

    CharString &appendInvariantChars(const UnicodeString &s, UErrorCode &errorCode);

    /**
     * Appends a filename/path part, e.g., a directory name.
     * First appends a U_FILE_SEP_CHAR if necessary.
     * Does nothing if s is empty.
     */
    CharString &appendPathPart(StringPiece s, UErrorCode &errorCode);

    /**
     * Appends a U_FILE_SEP_CHAR if this string is not empty
     * and does not already end with a U_FILE_SEP_CHAR or U_FILE_ALT_SEP_CHAR.
     */
    CharString &ensureEndsWithFileSeparator(UErrorCode &errorCode);

private:
    MaybeStackArray<char, 40> buffer;
    int32_t len;

    UBool ensureCapacity(int32_t capacity, int32_t desiredCapacityHint, UErrorCode &errorCode);

    CharString(const CharString &other); // forbid copying of this class
    CharString &operator=(const CharString &other); // forbid copying of this class
};

U_NAMESPACE_END

#endif
//eof
