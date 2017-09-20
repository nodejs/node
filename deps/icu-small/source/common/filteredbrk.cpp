// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014-2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"
#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_FILTERED_BREAK_ITERATION

#include "cmemory.h"

#include "unicode/filteredbrk.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/ures.h"

#include "uresimp.h" // ures_getByKeyWithFallback
#include "ubrkimpl.h" // U_ICUDATA_BRKITR
#include "uvector.h"
#include "cmemory.h"

U_NAMESPACE_BEGIN

#ifndef FB_DEBUG
#define FB_DEBUG 0
#endif

#if FB_DEBUG
#include <stdio.h>
static void _fb_trace(const char *m, const UnicodeString *s, UBool b, int32_t d, const char *f, int l) {
  char buf[2048];
  if(s) {
    s->extract(0,s->length(),buf,2048);
  } else {
    strcpy(buf,"NULL");
  }
  fprintf(stderr,"%s:%d: %s. s='%s'(%p), b=%c, d=%d\n",
          f, l, m, buf, (const void*)s, b?'T':'F',(int)d);
}

#define FB_TRACE(m,s,b,d) _fb_trace(m,s,b,d,__FILE__,__LINE__)
#else
#define FB_TRACE(m,s,b,d)
#endif

/**
 * Used with sortedInsert()
 */
static int8_t U_CALLCONV compareUnicodeString(UElement t1, UElement t2) {
    const UnicodeString &a = *(const UnicodeString*)t1.pointer;
    const UnicodeString &b = *(const UnicodeString*)t2.pointer;
    return a.compare(b);
}

/**
 * A UVector which implements a set of strings.
 */
class U_COMMON_API UStringSet : public UVector {
 public:
  UStringSet(UErrorCode &status) : UVector(uprv_deleteUObject,
                                           uhash_compareUnicodeString,
                                           1,
                                           status) {}
  virtual ~UStringSet();
  /**
   * Is this UnicodeSet contained?
   */
  inline UBool contains(const UnicodeString& s) {
    return contains((void*) &s);
  }
  using UVector::contains;
  /**
   * Return the ith UnicodeString alias
   */
  inline const UnicodeString* getStringAt(int32_t i) const {
    return (const UnicodeString*)elementAt(i);
  }
  /**
   * Adopt the UnicodeString if not already contained.
   * Caller no longer owns the pointer in any case.
   * @return true if adopted successfully, false otherwise (error, or else duplicate)
   */
  inline UBool adopt(UnicodeString *str, UErrorCode &status) {
    if(U_FAILURE(status) || contains(*str)) {
      delete str;
      return false;
    } else {
      sortedInsert(str, compareUnicodeString, status);
      if(U_FAILURE(status)) {
        delete str;
        return false;
      }
      return true;
    }
  }
  /**
   * Add by value.
   * @return true if successfully adopted.
   */
  inline UBool add(const UnicodeString& str, UErrorCode &status) {
    if(U_FAILURE(status)) return false;
    UnicodeString *t = new UnicodeString(str);
    if(t==NULL) {
      status = U_MEMORY_ALLOCATION_ERROR; return false;
    }
    return adopt(t, status);
  }
  /**
   * Remove this string.
   * @return true if successfully removed, false otherwise (error, or else it wasn't there)
   */
  inline UBool remove(const UnicodeString &s, UErrorCode &status) {
    if(U_FAILURE(status)) return false;
    return removeElement((void*) &s);
  }
};

/**
 * Virtual, won't be inlined
 */
UStringSet::~UStringSet() {}

/* ----------------------------------------------------------- */


/* Filtered Break constants */
static const int32_t kPARTIAL = (1<<0); //< partial - need to run through forward trie
static const int32_t kMATCH   = (1<<1); //< exact match - skip this one.
static const int32_t kSuppressInReverse = (1<<0);
static const int32_t kAddToForward = (1<<1);
static const UChar   kFULLSTOP = 0x002E; // '.'

/**
 * Shared data for SimpleFilteredSentenceBreakIterator
 */
class SimpleFilteredSentenceBreakData : public UMemory {
public:
  SimpleFilteredSentenceBreakData(UCharsTrie *forwards, UCharsTrie *backwards )
      : fForwardsPartialTrie(forwards), fBackwardsTrie(backwards), refcount(1) { }
  SimpleFilteredSentenceBreakData *incr() { refcount++;  return this; }
  SimpleFilteredSentenceBreakData *decr() { if((--refcount) <= 0) delete this; return 0; }
  virtual ~SimpleFilteredSentenceBreakData();

  LocalPointer<UCharsTrie>    fForwardsPartialTrie; //  Has ".a" for "a.M."
  LocalPointer<UCharsTrie>    fBackwardsTrie; //  i.e. ".srM" for Mrs.
  int32_t                     refcount;
};

SimpleFilteredSentenceBreakData::~SimpleFilteredSentenceBreakData() {}

/**
 * Concrete implementation
 */
class SimpleFilteredSentenceBreakIterator : public BreakIterator {
public:
  SimpleFilteredSentenceBreakIterator(BreakIterator *adopt, UCharsTrie *forwards, UCharsTrie *backwards, UErrorCode &status);
  SimpleFilteredSentenceBreakIterator(const SimpleFilteredSentenceBreakIterator& other);
  virtual ~SimpleFilteredSentenceBreakIterator();
private:
  SimpleFilteredSentenceBreakData *fData;
  LocalPointer<BreakIterator> fDelegate;
  LocalUTextPointer           fText;

  /* -- subclass interface -- */
public:
  /* -- cloning and other subclass stuff -- */
  virtual BreakIterator *  createBufferClone(void * /*stackBuffer*/,
                                             int32_t &/*BufferSize*/,
                                             UErrorCode &status) {
    // for now - always deep clone
    status = U_SAFECLONE_ALLOCATED_WARNING;
    return clone();
  }
  virtual BreakIterator* clone(void) const { return new SimpleFilteredSentenceBreakIterator(*this); }
  virtual UClassID getDynamicClassID(void) const { return NULL; }
  virtual UBool operator==(const BreakIterator& o) const { if(this==&o) return true; return false; }

  /* -- text modifying -- */
  virtual void setText(UText *text, UErrorCode &status) { fDelegate->setText(text,status); }
  virtual BreakIterator &refreshInputText(UText *input, UErrorCode &status) { fDelegate->refreshInputText(input,status); return *this; }
  virtual void adoptText(CharacterIterator* it) { fDelegate->adoptText(it); }
  virtual void setText(const UnicodeString &text) { fDelegate->setText(text); }

  /* -- other functions that are just delegated -- */
  virtual UText *getUText(UText *fillIn, UErrorCode &status) const { return fDelegate->getUText(fillIn,status); }
  virtual CharacterIterator& getText(void) const { return fDelegate->getText(); }

  /* -- ITERATION -- */
  virtual int32_t first(void);
  virtual int32_t preceding(int32_t offset);
  virtual int32_t previous(void);
  virtual UBool isBoundary(int32_t offset);
  virtual int32_t current(void) const { return fDelegate->current(); } // we keep the delegate current, so this should be correct.

  virtual int32_t next(void);

  virtual int32_t next(int32_t n);
  virtual int32_t following(int32_t offset);
  virtual int32_t last(void);

private:
    /**
     * Given that the fDelegate has already given its "initial" answer,
     * find the NEXT actual (non-excepted) break.
     * @param n initial position from delegate
     * @return new break position or UBRK_DONE
     */
    int32_t internalNext(int32_t n);
    /**
     * Given that the fDelegate has already given its "initial" answer,
     * find the PREV actual (non-excepted) break.
     * @param n initial position from delegate
     * @return new break position or UBRK_DONE
     */
    int32_t internalPrev(int32_t n);
    /**
     * set up the UText with the value of the fDelegate.
     * Call this before calling breakExceptionAt.
     * May be able to avoid excess calls
     */
    void resetState(UErrorCode &status);
    /**
     * Is there a match  (exception) at this spot?
     */
    enum EFBMatchResult { kNoExceptionHere, kExceptionHere };
    /**
     * Determine if there is an exception at this spot
     * @param n spot to check
     * @return kNoExceptionHere or kExceptionHere
     **/
    enum EFBMatchResult breakExceptionAt(int32_t n);
};

SimpleFilteredSentenceBreakIterator::SimpleFilteredSentenceBreakIterator(const SimpleFilteredSentenceBreakIterator& other)
  : BreakIterator(other), fData(other.fData->incr()), fDelegate(other.fDelegate->clone())
{
}


SimpleFilteredSentenceBreakIterator::SimpleFilteredSentenceBreakIterator(BreakIterator *adopt, UCharsTrie *forwards, UCharsTrie *backwards, UErrorCode &status) :
  BreakIterator(adopt->getLocale(ULOC_VALID_LOCALE,status),adopt->getLocale(ULOC_ACTUAL_LOCALE,status)),
  fData(new SimpleFilteredSentenceBreakData(forwards, backwards)),
  fDelegate(adopt)
{
  // all set..
}

SimpleFilteredSentenceBreakIterator::~SimpleFilteredSentenceBreakIterator() {
    fData = fData->decr();
}

void SimpleFilteredSentenceBreakIterator::resetState(UErrorCode &status) {
  fText.adoptInstead(fDelegate->getUText(fText.orphan(), status));
}

SimpleFilteredSentenceBreakIterator::EFBMatchResult
SimpleFilteredSentenceBreakIterator::breakExceptionAt(int32_t n) {
    int64_t bestPosn = -1;
    int32_t bestValue = -1;
    // loops while 'n' points to an exception.
    utext_setNativeIndex(fText.getAlias(), n); // from n..
    fData->fBackwardsTrie->reset();
    UChar32 uch;

    //if(debug2) u_printf(" n@ %d\n", n);
    // Assume a space is following the '.'  (so we handle the case:  "Mr. /Brown")
    if((uch=utext_previous32(fText.getAlias()))==(UChar32)0x0020) {  // TODO: skip a class of chars here??
      // TODO only do this the 1st time?
      //if(debug2) u_printf("skipping prev: |%C| \n", (UChar)uch);
    } else {
      //if(debug2) u_printf("not skipping prev: |%C| \n", (UChar)uch);
      uch = utext_next32(fText.getAlias());
      //if(debug2) u_printf(" -> : |%C| \n", (UChar)uch);
    }

    UStringTrieResult r = USTRINGTRIE_INTERMEDIATE_VALUE;

    while((uch=utext_previous32(fText.getAlias()))!=U_SENTINEL  &&   // more to consume backwards and..
          USTRINGTRIE_HAS_NEXT(r=fData->fBackwardsTrie->nextForCodePoint(uch))) {// more in the trie
      if(USTRINGTRIE_HAS_VALUE(r)) { // remember the best match so far
        bestPosn = utext_getNativeIndex(fText.getAlias());
        bestValue = fData->fBackwardsTrie->getValue();
      }
      //if(debug2) u_printf("rev< /%C/ cont?%d @%d\n", (UChar)uch, r, utext_getNativeIndex(fText.getAlias()));
    }

    if(USTRINGTRIE_MATCHES(r)) { // exact match?
      //if(debug2) u_printf("rev<?/%C/?end of seq.. r=%d, bestPosn=%d, bestValue=%d\n", (UChar)uch, r, bestPosn, bestValue);
      bestValue = fData->fBackwardsTrie->getValue();
      bestPosn = utext_getNativeIndex(fText.getAlias());
      //if(debug2) u_printf("rev<+/%C/+end of seq.. r=%d, bestPosn=%d, bestValue=%d\n", (UChar)uch, r, bestPosn, bestValue);
    }

    if(bestPosn>=0) {
      //if(debug2) u_printf("rev< /%C/ end of seq.. r=%d, bestPosn=%d, bestValue=%d\n", (UChar)uch, r, bestPosn, bestValue);

      //if(USTRINGTRIE_MATCHES(r)) {  // matched - so, now what?
      //int32_t bestValue = fBackwardsTrie->getValue();
      ////if(debug2) u_printf("rev< /%C/ matched, skip..%d  bestValue=%d\n", (UChar)uch, r, bestValue);

      if(bestValue == kMATCH) { // exact match!
        //if(debug2) u_printf(" exact backward match\n");
        return kExceptionHere; // See if the next is another exception.
      } else if(bestValue == kPARTIAL
                && fData->fForwardsPartialTrie.isValid()) { // make sure there's a forward trie
        //if(debug2) u_printf(" partial backward match\n");
        // We matched the "Ph." in "Ph.D." - now we need to run everything through the forwards trie
        // to see if it matches something going forward.
        fData->fForwardsPartialTrie->reset();
        UStringTrieResult rfwd = USTRINGTRIE_INTERMEDIATE_VALUE;
        utext_setNativeIndex(fText.getAlias(), bestPosn); // hope that's close ..
        //if(debug2) u_printf("Retrying at %d\n", bestPosn);
        while((uch=utext_next32(fText.getAlias()))!=U_SENTINEL &&
              USTRINGTRIE_HAS_NEXT(rfwd=fData->fForwardsPartialTrie->nextForCodePoint(uch))) {
          //if(debug2) u_printf("fwd> /%C/ cont?%d @%d\n", (UChar)uch, rfwd, utext_getNativeIndex(fText.getAlias()));
        }
        if(USTRINGTRIE_MATCHES(rfwd)) {
          //if(debug2) u_printf("fwd> /%C/ == forward match!\n", (UChar)uch);
          // only full matches here, nothing to check
          // skip the next:
            return kExceptionHere;
        } else {
          //if(debug2) u_printf("fwd> /%C/ no match.\n", (UChar)uch);
          // no match (no exception) -return the 'underlying' break
          return kNoExceptionHere;
        }
      } else {
        return kNoExceptionHere; // internal error and/or no forwards trie
      }
    } else {
      //if(debug2) u_printf("rev< /%C/ .. no match..%d\n", (UChar)uch, r);  // no best match
      return kNoExceptionHere; // No match - so exit. Not an exception.
    }
}

// the workhorse single next.
int32_t
SimpleFilteredSentenceBreakIterator::internalNext(int32_t n) {
  if(n == UBRK_DONE || // at end  or
    fData->fBackwardsTrie.isNull()) { // .. no backwards table loaded == no exceptions
      return n;
  }
  // OK, do we need to break here?
  UErrorCode status = U_ZERO_ERROR;
  // refresh text
  resetState(status);
  if(U_FAILURE(status)) return UBRK_DONE; // bail out
  int64_t utextLen = utext_nativeLength(fText.getAlias());

  //if(debug2) u_printf("str, native len=%d\n", utext_nativeLength(fText.getAlias()));
  while (n != UBRK_DONE && n != utextLen) { // outer loop runs once per underlying break (from fDelegate).
    SimpleFilteredSentenceBreakIterator::EFBMatchResult m = breakExceptionAt(n);

    switch(m) {
    case kExceptionHere:
      n = fDelegate->next(); // skip this one. Find the next lowerlevel break.
      continue;

    default:
    case kNoExceptionHere:
      return n;
    }
  }
  return n;
}

int32_t
SimpleFilteredSentenceBreakIterator::internalPrev(int32_t n) {
  if(n == 0 || n == UBRK_DONE || // at end  or
    fData->fBackwardsTrie.isNull()) { // .. no backwards table loaded == no exceptions
      return n;
  }
  // OK, do we need to break here?
  UErrorCode status = U_ZERO_ERROR;
  // refresh text
  resetState(status);
  if(U_FAILURE(status)) return UBRK_DONE; // bail out

  //if(debug2) u_printf("str, native len=%d\n", utext_nativeLength(fText.getAlias()));
  while (n != UBRK_DONE && n != 0) { // outer loop runs once per underlying break (from fDelegate).
    SimpleFilteredSentenceBreakIterator::EFBMatchResult m = breakExceptionAt(n);

    switch(m) {
    case kExceptionHere:
      n = fDelegate->previous(); // skip this one. Find the next lowerlevel break.
      continue;

    default:
    case kNoExceptionHere:
      return n;
    }
  }
  return n;
}


int32_t
SimpleFilteredSentenceBreakIterator::next() {
  return internalNext(fDelegate->next());
}

int32_t
SimpleFilteredSentenceBreakIterator::first(void) {
  // Don't suppress a break opportunity at the beginning of text.
  return fDelegate->first();
}

int32_t
SimpleFilteredSentenceBreakIterator::preceding(int32_t offset) {
  return internalPrev(fDelegate->preceding(offset));
}

int32_t
SimpleFilteredSentenceBreakIterator::previous(void) {
  return internalPrev(fDelegate->previous());
}

UBool SimpleFilteredSentenceBreakIterator::isBoundary(int32_t offset) {
  if (!fDelegate->isBoundary(offset)) return false; // no break to suppress

  if (fData->fBackwardsTrie.isNull()) return true; // no data = no suppressions

  UErrorCode status = U_ZERO_ERROR;
  resetState(status);

  SimpleFilteredSentenceBreakIterator::EFBMatchResult m = breakExceptionAt(offset);

  switch(m) {
  case kExceptionHere:
    return false;
  default:
  case kNoExceptionHere:
    return true;
  }
}

int32_t
SimpleFilteredSentenceBreakIterator::next(int32_t offset) {
  return internalNext(fDelegate->next(offset));
}

int32_t
SimpleFilteredSentenceBreakIterator::following(int32_t offset) {
  return internalNext(fDelegate->following(offset));
}

int32_t
SimpleFilteredSentenceBreakIterator::last(void) {
  // Don't suppress a break opportunity at the end of text.
  return fDelegate->last();
}


/**
 * Concrete implementation of builder class.
 */
class U_COMMON_API SimpleFilteredBreakIteratorBuilder : public FilteredBreakIteratorBuilder {
public:
  virtual ~SimpleFilteredBreakIteratorBuilder();
  SimpleFilteredBreakIteratorBuilder(const Locale &fromLocale, UErrorCode &status);
  SimpleFilteredBreakIteratorBuilder(UErrorCode &status);
  virtual UBool suppressBreakAfter(const UnicodeString& exception, UErrorCode& status);
  virtual UBool unsuppressBreakAfter(const UnicodeString& exception, UErrorCode& status);
  virtual BreakIterator *build(BreakIterator* adoptBreakIterator, UErrorCode& status);
private:
  UStringSet fSet;
};

SimpleFilteredBreakIteratorBuilder::~SimpleFilteredBreakIteratorBuilder()
{
}

SimpleFilteredBreakIteratorBuilder::SimpleFilteredBreakIteratorBuilder(UErrorCode &status)
  : fSet(status)
{
}

SimpleFilteredBreakIteratorBuilder::SimpleFilteredBreakIteratorBuilder(const Locale &fromLocale, UErrorCode &status)
  : fSet(status)
{
  if(U_SUCCESS(status)) {
    UErrorCode subStatus = U_ZERO_ERROR;
    LocalUResourceBundlePointer b(ures_open(U_ICUDATA_BRKITR, fromLocale.getBaseName(), &subStatus));
    if (U_FAILURE(subStatus) || (subStatus == U_USING_DEFAULT_WARNING) ) {
      status = subStatus; // copy the failing status
#if FB_DEBUG
      fprintf(stderr, "open BUNDLE %s : %s, %s\n", fromLocale.getBaseName(), "[exit]", u_errorName(status));
#endif
      return;  // leaves the builder empty, if you try to use it.
    }
    LocalUResourceBundlePointer exceptions(ures_getByKeyWithFallback(b.getAlias(), "exceptions", NULL, &subStatus));
    if (U_FAILURE(subStatus) || (subStatus == U_USING_DEFAULT_WARNING) ) {
      status = subStatus; // copy the failing status
#if FB_DEBUG
      fprintf(stderr, "open EXCEPTIONS %s : %s, %s\n", fromLocale.getBaseName(), "[exit]", u_errorName(status));
#endif
      return;  // leaves the builder empty, if you try to use it.
    }
    LocalUResourceBundlePointer breaks(ures_getByKeyWithFallback(exceptions.getAlias(), "SentenceBreak", NULL, &subStatus));

#if FB_DEBUG
    {
      UErrorCode subsub = subStatus;
      fprintf(stderr, "open SentenceBreak %s => %s, %s\n", fromLocale.getBaseName(), ures_getLocale(breaks.getAlias(), &subsub), u_errorName(subStatus));
    }
#endif

    if (U_FAILURE(subStatus) || (subStatus == U_USING_DEFAULT_WARNING) ) {
      status = subStatus; // copy the failing status
#if FB_DEBUG
      fprintf(stderr, "open %s : %s, %s\n", fromLocale.getBaseName(), "[exit]", u_errorName(status));
#endif
      return;  // leaves the builder empty, if you try to use it.
    }

    LocalUResourceBundlePointer strs;
    subStatus = status; // Pick up inherited warning status now
    do {
      strs.adoptInstead(ures_getNextResource(breaks.getAlias(), strs.orphan(), &subStatus));
      if(strs.isValid() && U_SUCCESS(subStatus)) {
        UnicodeString str(ures_getUnicodeString(strs.getAlias(), &status));
        suppressBreakAfter(str, status); // load the string
      }
    } while (strs.isValid() && U_SUCCESS(subStatus));
    if(U_FAILURE(subStatus)&&subStatus!=U_INDEX_OUTOFBOUNDS_ERROR&&U_SUCCESS(status)) {
      status = subStatus;
    }
  }
}

UBool
SimpleFilteredBreakIteratorBuilder::suppressBreakAfter(const UnicodeString& exception, UErrorCode& status)
{
  UBool r = fSet.add(exception, status);
  FB_TRACE("suppressBreakAfter",&exception,r,0);
  return r;
}

UBool
SimpleFilteredBreakIteratorBuilder::unsuppressBreakAfter(const UnicodeString& exception, UErrorCode& status)
{
  UBool r = fSet.remove(exception, status);
  FB_TRACE("unsuppressBreakAfter",&exception,r,0);
  return r;
}

/**
 * Jitterbug 2974: MSVC has a bug whereby new X[0] behaves badly.
 * Work around this.
 *
 * Note: "new UnicodeString[subCount]" ends up calling global operator new
 * on MSVC2012 for some reason.
 */
static inline UnicodeString* newUnicodeStringArray(size_t count) {
    return new UnicodeString[count ? count : 1];
}

BreakIterator *
SimpleFilteredBreakIteratorBuilder::build(BreakIterator* adoptBreakIterator, UErrorCode& status) {
  LocalPointer<BreakIterator> adopt(adoptBreakIterator);

  LocalPointer<UCharsTrieBuilder> builder(new UCharsTrieBuilder(status), status);
  LocalPointer<UCharsTrieBuilder> builder2(new UCharsTrieBuilder(status), status);
  if(U_FAILURE(status)) {
    return NULL;
  }

  int32_t revCount = 0;
  int32_t fwdCount = 0;

  int32_t subCount = fSet.size();

  UnicodeString *ustrs_ptr = newUnicodeStringArray(subCount);

  LocalArray<UnicodeString> ustrs(ustrs_ptr);

  LocalMemory<int> partials;
  partials.allocateInsteadAndReset(subCount);

  LocalPointer<UCharsTrie>    backwardsTrie; //  i.e. ".srM" for Mrs.
  LocalPointer<UCharsTrie>    forwardsPartialTrie; //  Has ".a" for "a.M."

  int n=0;
  for ( int32_t i = 0;
        i<fSet.size();
        i++) {
    const UnicodeString *abbr = fSet.getStringAt(i);
    if(abbr) {
      FB_TRACE("build",abbr,TRUE,i);
      ustrs[n] = *abbr; // copy by value
      FB_TRACE("ustrs[n]",&ustrs[n],TRUE,i);
    } else {
      FB_TRACE("build",abbr,FALSE,i);
      status = U_MEMORY_ALLOCATION_ERROR;
      return NULL;
    }
    partials[n] = 0; // default: not partial
    n++;
  }
  // first pass - find partials.
  for(int i=0;i<subCount;i++) {
    int nn = ustrs[i].indexOf(kFULLSTOP); // TODO: non-'.' abbreviations
    if(nn>-1 && (nn+1)!=ustrs[i].length()) {
      FB_TRACE("partial",&ustrs[i],FALSE,i);
      // is partial.
      // is it unique?
      int sameAs = -1;
      for(int j=0;j<subCount;j++) {
        if(j==i) continue;
        if(ustrs[i].compare(0,nn+1,ustrs[j],0,nn+1)==0) {
          FB_TRACE("prefix",&ustrs[j],FALSE,nn+1);
          //UBool otherIsPartial = ((nn+1)!=ustrs[j].length());  // true if ustrs[j] doesn't end at nn
          if(partials[j]==0) { // hasn't been processed yet
            partials[j] = kSuppressInReverse | kAddToForward;
            FB_TRACE("suppressing",&ustrs[j],FALSE,j);
          } else if(partials[j] & kSuppressInReverse) {
            sameAs = j; // the other entry is already in the reverse table.
          }
        }
      }
      FB_TRACE("for partial same-",&ustrs[i],FALSE,sameAs);
      FB_TRACE(" == partial #",&ustrs[i],FALSE,partials[i]);
      UnicodeString prefix(ustrs[i], 0, nn+1);
      if(sameAs == -1 && partials[i] == 0) {
        // first one - add the prefix to the reverse table.
        prefix.reverse();
        builder->add(prefix, kPARTIAL, status);
        revCount++;
        FB_TRACE("Added partial",&prefix,FALSE, i);
        FB_TRACE(u_errorName(status),&ustrs[i],FALSE,i);
        partials[i] = kSuppressInReverse | kAddToForward;
      } else {
        FB_TRACE("NOT adding partial",&prefix,FALSE, i);
        FB_TRACE(u_errorName(status),&ustrs[i],FALSE,i);
      }
    }
  }
  for(int i=0;i<subCount;i++) {
    if(partials[i]==0) {
      ustrs[i].reverse();
      builder->add(ustrs[i], kMATCH, status);
      revCount++;
      FB_TRACE(u_errorName(status), &ustrs[i], FALSE, i);
    } else {
      FB_TRACE("Adding fwd",&ustrs[i], FALSE, i);

      // an optimization would be to only add the portion after the '.'
      // for example, for "Ph.D." we store ".hP" in the reverse table. We could just store "D." in the forward,
      // instead of "Ph.D." since we already know the "Ph." part is a match.
      // would need the trie to be able to hold 0-length strings, though.
      builder2->add(ustrs[i], kMATCH, status); // forward
      fwdCount++;
      //ustrs[i].reverse();
      ////if(debug2) u_printf("SUPPRESS- not Added(%d):  /%S/ status=%s\n",partials[i], ustrs[i].getTerminatedBuffer(), u_errorName(status));
    }
  }
  FB_TRACE("AbbrCount",NULL,FALSE, subCount);

  if(revCount>0) {
    backwardsTrie.adoptInstead(builder->build(USTRINGTRIE_BUILD_FAST, status));
    if(U_FAILURE(status)) {
      FB_TRACE(u_errorName(status),NULL,FALSE, -1);
      return NULL;
    }
  }

  if(fwdCount>0) {
    forwardsPartialTrie.adoptInstead(builder2->build(USTRINGTRIE_BUILD_FAST, status));
    if(U_FAILURE(status)) {
      FB_TRACE(u_errorName(status),NULL,FALSE, -1);
      return NULL;
    }
  }

  return new SimpleFilteredSentenceBreakIterator(adopt.orphan(), forwardsPartialTrie.orphan(), backwardsTrie.orphan(), status);
}


// ----------- Base class implementation

FilteredBreakIteratorBuilder::FilteredBreakIteratorBuilder() {
}

FilteredBreakIteratorBuilder::~FilteredBreakIteratorBuilder() {
}

FilteredBreakIteratorBuilder *
FilteredBreakIteratorBuilder::createInstance(const Locale& where, UErrorCode& status) {
  if(U_FAILURE(status)) return NULL;
  LocalPointer<FilteredBreakIteratorBuilder> ret(new SimpleFilteredBreakIteratorBuilder(where, status), status);
  return (U_SUCCESS(status))? ret.orphan(): NULL;
}

FilteredBreakIteratorBuilder *
FilteredBreakIteratorBuilder::createInstance(UErrorCode& status) {
  if(U_FAILURE(status)) return NULL;
  LocalPointer<FilteredBreakIteratorBuilder> ret(new SimpleFilteredBreakIteratorBuilder(status), status);
  return (U_SUCCESS(status))? ret.orphan(): NULL;
}

U_NAMESPACE_END

#endif //#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_FILTERED_BREAK_ITERATION
