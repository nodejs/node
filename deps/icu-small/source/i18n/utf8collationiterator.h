// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* utf8collationiterator.h
*
* created on: 2012nov12 (from utf16collationiterator.h & uitercollationiterator.h)
* created by: Markus W. Scherer
*/

#ifndef __UTF8COLLATIONITERATOR_H__
#define __UTF8COLLATIONITERATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

/**
 * UTF-8 collation element and character iterator.
 * Handles normalized UTF-8 text inline, with length or NUL-terminated.
 * Unnormalized text is handled by a subclass.
 */
class U_I18N_API UTF8CollationIterator : public CollationIterator {
public:
    UTF8CollationIterator(const CollationData *d, UBool numeric,
                          const uint8_t *s, int32_t p, int32_t len)
            : CollationIterator(d, numeric),
              u8(s), pos(p), length(len) {}

    virtual ~UTF8CollationIterator();

    virtual void resetToOffset(int32_t newOffset);

    virtual int32_t getOffset() const;

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    /**
     * For byte sequences that are illegal in UTF-8, an error value may be returned
     * together with a bogus code point. The caller will ignore that code point.
     *
     * Special values may be returned for surrogate code points, which are also illegal in UTF-8,
     * but the caller will treat them like U+FFFD because forbidSurrogateCodePoints() returns TRUE.
     *
     * Valid lead surrogates are returned from inside a normalized text segment,
     * where handleGetTrailSurrogate() will return the matching trail surrogate.
     */
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UBool foundNULTerminator();

    virtual UBool forbidSurrogateCodePoints() const;

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    const uint8_t *u8;
    int32_t pos;
    int32_t length;  // <0 for NUL-terminated strings
};

/**
 * Incrementally checks the input text for FCD and normalizes where necessary.
 */
class U_I18N_API FCDUTF8CollationIterator : public UTF8CollationIterator {
public:
    FCDUTF8CollationIterator(const CollationData *data, UBool numeric,
                             const uint8_t *s, int32_t p, int32_t len)
            : UTF8CollationIterator(data, numeric, s, p, len),
              state(CHECK_FWD), start(p),
              nfcImpl(data->nfcImpl) {}

    virtual ~FCDUTF8CollationIterator();

    virtual void resetToOffset(int32_t newOffset);

    virtual int32_t getOffset() const;

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

    virtual UBool foundNULTerminator();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

private:
    UBool nextHasLccc() const;
    UBool previousHasTccc() const;

    /**
     * Switches to forward checking if possible.
     */
    void switchToForward();

    /**
     * Extends the FCD text segment forward or normalizes around pos.
     * @return TRUE if success
     */
    UBool nextSegment(UErrorCode &errorCode);

    /**
     * Switches to backward checking.
     */
    void switchToBackward();

    /**
     * Extends the FCD text segment backward or normalizes around pos.
     * @return TRUE if success
     */
    UBool previousSegment(UErrorCode &errorCode);

    UBool normalize(const UnicodeString &s, UErrorCode &errorCode);

    enum State {
        /**
         * The input text [start..pos[ passes the FCD check.
         * Moving forward checks incrementally.
         * limit is undefined.
         */
        CHECK_FWD,
        /**
         * The input text [pos..limit[ passes the FCD check.
         * Moving backward checks incrementally.
         * start is undefined.
         */
        CHECK_BWD,
        /**
         * The input text [start..limit[ passes the FCD check.
         * pos tracks the current text index.
         */
        IN_FCD_SEGMENT,
        /**
         * The input text [start..limit[ failed the FCD check and was normalized.
         * pos tracks the current index in the normalized string.
         */
        IN_NORMALIZED
    };

    State state;

    int32_t start;
    int32_t limit;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UTF8COLLATIONITERATOR_H__
