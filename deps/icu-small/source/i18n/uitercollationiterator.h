// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* uitercollationiterator.h
*
* created on: 2012sep23 (from utf16collationiterator.h)
* created by: Markus W. Scherer
*/

#ifndef __UITERCOLLATIONITERATOR_H__
#define __UITERCOLLATIONITERATOR_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uiter.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationiterator.h"
#include "normalizer2impl.h"

U_NAMESPACE_BEGIN

/**
 * UCharIterator-based collation element and character iterator.
 * Handles normalized text inline, with length or NUL-terminated.
 * Unnormalized text is handled by a subclass.
 */
class U_I18N_API UIterCollationIterator : public CollationIterator {
public:
    UIterCollationIterator(const CollationData *d, UBool numeric, UCharIterator &ui)
            : CollationIterator(d, numeric), iter(ui) {}

    virtual ~UIterCollationIterator();

    virtual void resetToOffset(int32_t newOffset);

    virtual int32_t getOffset() const;

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    UCharIterator &iter;
};

/**
 * Incrementally checks the input text for FCD and normalizes where necessary.
 */
class U_I18N_API FCDUIterCollationIterator : public UIterCollationIterator {
public:
    FCDUIterCollationIterator(const CollationData *data, UBool numeric, UCharIterator &ui, int32_t startIndex)
            : UIterCollationIterator(data, numeric, ui),
              state(ITER_CHECK_FWD), start(startIndex),
              nfcImpl(data->nfcImpl) {}

    virtual ~FCDUIterCollationIterator();

    virtual void resetToOffset(int32_t newOffset);

    virtual int32_t getOffset() const;

    virtual UChar32 nextCodePoint(UErrorCode &errorCode);

    virtual UChar32 previousCodePoint(UErrorCode &errorCode);

protected:
    virtual uint32_t handleNextCE32(UChar32 &c, UErrorCode &errorCode);

    virtual UChar handleGetTrailSurrogate();

    virtual void forwardNumCodePoints(int32_t num, UErrorCode &errorCode);

    virtual void backwardNumCodePoints(int32_t num, UErrorCode &errorCode);

private:
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
         * The input text [start..(iter index)[ passes the FCD check.
         * Moving forward checks incrementally.
         * pos & limit are undefined.
         */
        ITER_CHECK_FWD,
        /**
         * The input text [(iter index)..limit[ passes the FCD check.
         * Moving backward checks incrementally.
         * start & pos are undefined.
         */
        ITER_CHECK_BWD,
        /**
         * The input text [start..limit[ passes the FCD check.
         * pos tracks the current text index.
         */
        ITER_IN_FCD_SEGMENT,
        /**
         * The input text [start..limit[ failed the FCD check and was normalized.
         * pos tracks the current index in the normalized string.
         * The text iterator is at the limit index.
         */
        IN_NORM_ITER_AT_LIMIT,
        /**
         * The input text [start..limit[ failed the FCD check and was normalized.
         * pos tracks the current index in the normalized string.
         * The text iterator is at the start index.
         */
        IN_NORM_ITER_AT_START
    };

    State state;

    int32_t start;
    int32_t pos;
    int32_t limit;

    const Normalizer2Impl &nfcImpl;
    UnicodeString normalized;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __UITERCOLLATIONITERATOR_H__
