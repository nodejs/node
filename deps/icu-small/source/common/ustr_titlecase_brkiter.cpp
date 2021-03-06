// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ustr_titlecase_brkiter.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may30
*   created by: Markus W. Scherer
*
*   Titlecasing functions that are based on BreakIterator
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/casemap.h"
#include "unicode/chariter.h"
#include "unicode/localpointer.h"
#include "unicode/ubrk.h"
#include "unicode/ucasemap.h"
#include "unicode/utext.h"
#include "cmemory.h"
#include "uassert.h"
#include "ucase.h"
#include "ucasemap_imp.h"

U_NAMESPACE_BEGIN

/**
 * Whole-string BreakIterator.
 * Titlecasing only calls setText(), first(), and next().
 * We implement the rest only to satisfy the abstract interface.
 */
class WholeStringBreakIterator : public BreakIterator {
public:
    WholeStringBreakIterator() : BreakIterator(), length(0) {}
    ~WholeStringBreakIterator() U_OVERRIDE;
    UBool operator==(const BreakIterator&) const U_OVERRIDE;
    WholeStringBreakIterator *clone() const U_OVERRIDE;
    static UClassID U_EXPORT2 getStaticClassID();
    UClassID getDynamicClassID() const U_OVERRIDE;
    CharacterIterator &getText() const U_OVERRIDE;
    UText *getUText(UText *fillIn, UErrorCode &errorCode) const U_OVERRIDE;
    void  setText(const UnicodeString &text) U_OVERRIDE;
    void  setText(UText *text, UErrorCode &errorCode) U_OVERRIDE;
    void  adoptText(CharacterIterator* it) U_OVERRIDE;
    int32_t first() U_OVERRIDE;
    int32_t last() U_OVERRIDE;
    int32_t previous() U_OVERRIDE;
    int32_t next() U_OVERRIDE;
    int32_t current() const U_OVERRIDE;
    int32_t following(int32_t offset) U_OVERRIDE;
    int32_t preceding(int32_t offset) U_OVERRIDE;
    UBool isBoundary(int32_t offset) U_OVERRIDE;
    int32_t next(int32_t n) U_OVERRIDE;
    WholeStringBreakIterator *createBufferClone(void *stackBuffer, int32_t &BufferSize,
                                                UErrorCode &errorCode) U_OVERRIDE;
    WholeStringBreakIterator &refreshInputText(UText *input, UErrorCode &errorCode) U_OVERRIDE;

private:
    int32_t length;
};

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(WholeStringBreakIterator)

WholeStringBreakIterator::~WholeStringBreakIterator() {}
UBool WholeStringBreakIterator::operator==(const BreakIterator&) const { return FALSE; }
WholeStringBreakIterator *WholeStringBreakIterator::clone() const { return nullptr; }

CharacterIterator &WholeStringBreakIterator::getText() const {
    UPRV_UNREACHABLE;  // really should not be called
}
UText *WholeStringBreakIterator::getUText(UText * /*fillIn*/, UErrorCode &errorCode) const {
    if (U_SUCCESS(errorCode)) {
        errorCode = U_UNSUPPORTED_ERROR;
    }
    return nullptr;
}

void  WholeStringBreakIterator::setText(const UnicodeString &text) {
    length = text.length();
}
void  WholeStringBreakIterator::setText(UText *text, UErrorCode &errorCode) {
    if (U_SUCCESS(errorCode)) {
        int64_t length64 = utext_nativeLength(text);
        if (length64 <= INT32_MAX) {
            length = (int32_t)length64;
        } else {
            errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        }
    }
}
void  WholeStringBreakIterator::adoptText(CharacterIterator*) {
    UPRV_UNREACHABLE;  // should not be called
}

int32_t WholeStringBreakIterator::first() { return 0; }
int32_t WholeStringBreakIterator::last() { return length; }
int32_t WholeStringBreakIterator::previous() { return 0; }
int32_t WholeStringBreakIterator::next() { return length; }
int32_t WholeStringBreakIterator::current() const { return 0; }
int32_t WholeStringBreakIterator::following(int32_t /*offset*/) { return length; }
int32_t WholeStringBreakIterator::preceding(int32_t /*offset*/) { return 0; }
UBool WholeStringBreakIterator::isBoundary(int32_t /*offset*/) { return FALSE; }
int32_t WholeStringBreakIterator::next(int32_t /*n*/) { return length; }

WholeStringBreakIterator *WholeStringBreakIterator::createBufferClone(
        void * /*stackBuffer*/, int32_t & /*BufferSize*/, UErrorCode &errorCode) {
    if (U_SUCCESS(errorCode)) {
        errorCode = U_UNSUPPORTED_ERROR;
    }
    return nullptr;
}
WholeStringBreakIterator &WholeStringBreakIterator::refreshInputText(
        UText * /*input*/, UErrorCode &errorCode) {
    if (U_SUCCESS(errorCode)) {
        errorCode = U_UNSUPPORTED_ERROR;
    }
    return *this;
}

U_CFUNC
BreakIterator *ustrcase_getTitleBreakIterator(
        const Locale *locale, const char *locID, uint32_t options, BreakIterator *iter,
        LocalPointer<BreakIterator> &ownedIter, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return nullptr; }
    options &= U_TITLECASE_ITERATOR_MASK;
    if (options != 0 && iter != nullptr) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return nullptr;
    }
    if (iter == nullptr) {
        switch (options) {
        case 0:
            iter = BreakIterator::createWordInstance(
                locale != nullptr ? *locale : Locale(locID), errorCode);
            break;
        case U_TITLECASE_WHOLE_STRING:
            iter = new WholeStringBreakIterator();
            if (iter == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
            }
            break;
        case U_TITLECASE_SENTENCES:
            iter = BreakIterator::createSentenceInstance(
                locale != nullptr ? *locale : Locale(locID), errorCode);
            break;
        default:
            errorCode = U_ILLEGAL_ARGUMENT_ERROR;
            break;
        }
        ownedIter.adoptInstead(iter);
    }
    return iter;
}

int32_t CaseMap::toTitle(
        const char *locale, uint32_t options, BreakIterator *iter,
        const UChar *src, int32_t srcLength,
        UChar *dest, int32_t destCapacity, Edits *edits,
        UErrorCode &errorCode) {
    LocalPointer<BreakIterator> ownedIter;
    iter = ustrcase_getTitleBreakIterator(nullptr, locale, options, iter, ownedIter, errorCode);
    if(iter==NULL) {
        return 0;
    }
    UnicodeString s(srcLength<0, src, srcLength);
    iter->setText(s);
    return ustrcase_map(
        ustrcase_getCaseLocale(locale), options, iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, edits, errorCode);
}

U_NAMESPACE_END

U_NAMESPACE_USE

U_CAPI int32_t U_EXPORT2
u_strToTitle(UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UBreakIterator *titleIter,
             const char *locale,
             UErrorCode *pErrorCode) {
    LocalPointer<BreakIterator> ownedIter;
    BreakIterator *iter = ustrcase_getTitleBreakIterator(
        nullptr, locale, 0, reinterpret_cast<BreakIterator *>(titleIter),
        ownedIter, *pErrorCode);
    if (iter == nullptr) {
        return 0;
    }
    UnicodeString s(srcLength<0, src, srcLength);
    iter->setText(s);
    return ustrcase_mapWithOverlap(
        ustrcase_getCaseLocale(locale), 0, iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ucasemap_toTitle(UCaseMap *csm,
                 UChar *dest, int32_t destCapacity,
                 const UChar *src, int32_t srcLength,
                 UErrorCode *pErrorCode) {
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if (csm->iter == NULL) {
        LocalPointer<BreakIterator> ownedIter;
        BreakIterator *iter = ustrcase_getTitleBreakIterator(
            nullptr, csm->locale, csm->options, nullptr, ownedIter, *pErrorCode);
        if (iter == nullptr) {
            return 0;
        }
        csm->iter = ownedIter.orphan();
    }
    UnicodeString s(srcLength<0, src, srcLength);
    csm->iter->setText(s);
    return ustrcase_map(
        csm->caseLocale, csm->options, csm->iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, NULL, *pErrorCode);
}

#endif  // !UCONFIG_NO_BREAK_ITERATION
