// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 1996-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/ubrk.h"

#include "unicode/brkiter.h"
#include "unicode/uloc.h"
#include "unicode/ustring.h"
#include "unicode/uchriter.h"
#include "unicode/rbbi.h"
#include "rbbirb.h"
#include "uassert.h"
#include "cmemory.h"

U_NAMESPACE_USE

//------------------------------------------------------------------------------
//
//    ubrk_open      Create a canned type of break iterator based on type (word, line, etc.)
//                   and locale.
//
//------------------------------------------------------------------------------
U_CAPI UBreakIterator* U_EXPORT2
ubrk_open(UBreakIteratorType type,
      const char *locale,
      const UChar *text,
      int32_t textLength,
      UErrorCode *status)
{

  if(U_FAILURE(*status)) return 0;

  BreakIterator *result = 0;

  switch(type) {

  case UBRK_CHARACTER:
    result = BreakIterator::createCharacterInstance(Locale(locale), *status);
    break;

  case UBRK_WORD:
    result = BreakIterator::createWordInstance(Locale(locale), *status);
    break;

  case UBRK_LINE:
    result = BreakIterator::createLineInstance(Locale(locale), *status);
    break;

  case UBRK_SENTENCE:
    result = BreakIterator::createSentenceInstance(Locale(locale), *status);
    break;

  case UBRK_TITLE:
    result = BreakIterator::createTitleInstance(Locale(locale), *status);
    break;

  default:
    *status = U_ILLEGAL_ARGUMENT_ERROR;
  }

  // check for allocation error
  if (U_FAILURE(*status)) {
     return 0;
  }
  if(result == 0) {
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
  }


  UBreakIterator *uBI = (UBreakIterator *)result;
  if (text != NULL) {
      ubrk_setText(uBI, text, textLength, status);
  }
  return uBI;
}



//------------------------------------------------------------------------------
//
//   ubrk_openRules      open a break iterator from a set of break rules.
//                       Invokes the rule builder.
//
//------------------------------------------------------------------------------
U_CAPI UBreakIterator* U_EXPORT2
ubrk_openRules(  const UChar        *rules,
                       int32_t       rulesLength,
                 const UChar        *text,
                       int32_t       textLength,
                       UParseError  *parseErr,
                       UErrorCode   *status)  {

    if (status == NULL || U_FAILURE(*status)){
        return 0;
    }

    BreakIterator *result = 0;
    UnicodeString ruleString(rules, rulesLength);
    result = RBBIRuleBuilder::createRuleBasedBreakIterator(ruleString, parseErr, *status);
    if(U_FAILURE(*status)) {
        return 0;
    }

    UBreakIterator *uBI = (UBreakIterator *)result;
    if (text != NULL) {
        ubrk_setText(uBI, text, textLength, status);
    }
    return uBI;
}


U_CAPI UBreakIterator* U_EXPORT2
ubrk_openBinaryRules(const uint8_t *binaryRules, int32_t rulesLength,
                     const UChar *  text, int32_t textLength,
                     UErrorCode *   status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (rulesLength < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    LocalPointer<RuleBasedBreakIterator> lpRBBI(new RuleBasedBreakIterator(binaryRules, rulesLength, *status), *status);
    if (U_FAILURE(*status)) {
        return NULL;
    }
    UBreakIterator *uBI = reinterpret_cast<UBreakIterator *>(lpRBBI.orphan());
    if (text != NULL) {
        ubrk_setText(uBI, text, textLength, status);
    }
    return uBI;
}


U_CAPI UBreakIterator * U_EXPORT2
ubrk_safeClone(
          const UBreakIterator *bi,
          void * /*stackBuffer*/,
          int32_t *pBufferSize,
          UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status)){
        return NULL;
    }
    if (bi == NULL) {
       *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    if (pBufferSize != NULL) {
        int32_t inputSize = *pBufferSize;
        *pBufferSize = 1;
        if (inputSize == 0) {
            return NULL;  // preflighting for deprecated functionality
        }
    }
    BreakIterator *newBI = ((BreakIterator *)bi)->clone();
    if (newBI == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
    } else if (pBufferSize != NULL) {
        *status = U_SAFECLONE_ALLOCATED_WARNING;
    }
    return (UBreakIterator *)newBI;
}

U_CAPI UBreakIterator * U_EXPORT2
ubrk_clone(const UBreakIterator *bi, UErrorCode *status) {
    return ubrk_safeClone(bi, nullptr, nullptr, status);
}


U_CAPI void U_EXPORT2
ubrk_close(UBreakIterator *bi)
{
    delete (BreakIterator *)bi;
}

U_CAPI void U_EXPORT2
ubrk_setText(UBreakIterator* bi,
             const UChar*    text,
             int32_t         textLength,
             UErrorCode*     status)
{
    UText  ut = UTEXT_INITIALIZER;
    utext_openUChars(&ut, text, textLength, status);
    ((BreakIterator*)bi)->setText(&ut, *status);
    // A stack allocated UText wrapping a UChar * string
    //   can be dumped without explicitly closing it.
}



U_CAPI void U_EXPORT2
ubrk_setUText(UBreakIterator *bi,
             UText          *text,
             UErrorCode     *status)
{
  ((BreakIterator*)bi)->setText(text, *status);
}





U_CAPI int32_t U_EXPORT2
ubrk_current(const UBreakIterator *bi)
{

  return ((BreakIterator*)bi)->current();
}

U_CAPI int32_t U_EXPORT2
ubrk_next(UBreakIterator *bi)
{

  return ((BreakIterator*)bi)->next();
}

U_CAPI int32_t U_EXPORT2
ubrk_previous(UBreakIterator *bi)
{

  return ((BreakIterator*)bi)->previous();
}

U_CAPI int32_t U_EXPORT2
ubrk_first(UBreakIterator *bi)
{

  return ((BreakIterator*)bi)->first();
}

U_CAPI int32_t U_EXPORT2
ubrk_last(UBreakIterator *bi)
{

  return ((BreakIterator*)bi)->last();
}

U_CAPI int32_t U_EXPORT2
ubrk_preceding(UBreakIterator *bi,
           int32_t offset)
{

  return ((BreakIterator*)bi)->preceding(offset);
}

U_CAPI int32_t U_EXPORT2
ubrk_following(UBreakIterator *bi,
           int32_t offset)
{

  return ((BreakIterator*)bi)->following(offset);
}

U_CAPI const char* U_EXPORT2
ubrk_getAvailable(int32_t index)
{

  return uloc_getAvailable(index);
}

U_CAPI int32_t U_EXPORT2
ubrk_countAvailable()
{

  return uloc_countAvailable();
}


U_CAPI  UBool U_EXPORT2
ubrk_isBoundary(UBreakIterator *bi, int32_t offset)
{
    return ((BreakIterator*)bi)->isBoundary(offset);
}


U_CAPI  int32_t U_EXPORT2
ubrk_getRuleStatus(UBreakIterator *bi)
{
    return ((BreakIterator*)bi)->getRuleStatus();
}

U_CAPI  int32_t U_EXPORT2
ubrk_getRuleStatusVec(UBreakIterator *bi, int32_t *fillInVec, int32_t capacity, UErrorCode *status)
{
    return ((BreakIterator*)bi)->getRuleStatusVec(fillInVec, capacity, *status);
}


U_CAPI const char* U_EXPORT2
ubrk_getLocaleByType(const UBreakIterator *bi,
                     ULocDataLocaleType type,
                     UErrorCode* status)
{
    if (bi == NULL) {
        if (U_SUCCESS(*status)) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
        }
        return NULL;
    }
    return ((BreakIterator*)bi)->getLocaleID(type, *status);
}


U_CAPI void U_EXPORT2
ubrk_refreshUText(UBreakIterator *bi,
                       UText          *text,
                       UErrorCode     *status)
{
    BreakIterator *bii = reinterpret_cast<BreakIterator *>(bi);
    bii->refreshInputText(text, *status);
}

U_CAPI int32_t U_EXPORT2
ubrk_getBinaryRules(UBreakIterator *bi,
                    uint8_t *       binaryRules, int32_t rulesCapacity,
                    UErrorCode *    status)
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ((binaryRules == NULL && rulesCapacity > 0) || rulesCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    RuleBasedBreakIterator* rbbi;
    if ((rbbi = dynamic_cast<RuleBasedBreakIterator*>(reinterpret_cast<BreakIterator*>(bi))) == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    uint32_t rulesLength;
    const uint8_t * returnedRules = rbbi->getBinaryRules(rulesLength);
    if (rulesLength > INT32_MAX) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }
    if (binaryRules != NULL) { // if not preflighting
        // Here we know rulesLength <= INT32_MAX and rulesCapacity >= 0, can cast safely
        if ((int32_t)rulesLength > rulesCapacity) {
            *status = U_BUFFER_OVERFLOW_ERROR;
        } else {
            uprv_memcpy(binaryRules, returnedRules, rulesLength);
        }
    }
    return (int32_t)rulesLength;
}


#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
