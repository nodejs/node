/*
******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*
* File unistrappender.h
******************************************************************************
*/

#ifndef __UNISTRAPPENDER_H__
#define __UNISTRAPPENDER_H__

#include "unicode/unistr.h"
#include "unicode/uobject.h"
#include "unicode/utf16.h"
#include "unicode/utypes.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

/**
 * An optimization for the slowness of calling UnicodeString::append()
 * one character at a time in a loop. It stores appends in a buffer while
 * never actually calling append on the unicode string unless the buffer
 * fills up or is flushed.
 * 
 * proper usage:
 * {
 *     UnicodeStringAppender appender(astring);
 *     for (int32_t i = 0; i < 100; ++i) {
 *        appender.append((UChar) i);
 *     }
 *     // appender flushed automatically when it goes out of scope.
 * }
 */
class UnicodeStringAppender : public UMemory {
public:
    
    /**
     * dest is the UnicodeString being appended to. It must always
     * exist while this instance exists.
     */
    UnicodeStringAppender(UnicodeString &dest) : fDest(&dest), fIdx(0) { }

    inline void append(UChar x) {
        if (fIdx == UPRV_LENGTHOF(fBuffer)) {
            fDest->append(fBuffer, 0, fIdx);
            fIdx = 0;
        }
        fBuffer[fIdx++] = x;
    }

    inline void append(UChar32 x) {
        if (fIdx >= UPRV_LENGTHOF(fBuffer) - 1) {
            fDest->append(fBuffer, 0, fIdx);
            fIdx = 0;
        }
        U16_APPEND_UNSAFE(fBuffer, fIdx, x);
    }

    /**
     * Ensures that all appended characters have been written out to dest.
     */
    inline void flush() {
        if (fIdx) {
            fDest->append(fBuffer, 0, fIdx);
        }
        fIdx = 0;
    }

    /**
     * flush the buffer when we go out of scope.
     */
    ~UnicodeStringAppender() {
        flush();
    }
private:
    UnicodeString *fDest;
    int32_t fIdx;
    UChar fBuffer[32];
    UnicodeStringAppender(const UnicodeStringAppender &other);
    UnicodeStringAppender &operator=(const UnicodeStringAppender &other);
};

U_NAMESPACE_END

#endif
