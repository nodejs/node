/*
***************************************************************************
*   Copyright (C) 1999-2016 International Business Machines Corporation
*   and others. All rights reserved.
***************************************************************************
*/
//
//  file:  rbbi.c    Contains the implementation of the rule based break iterator
//                   runtime engine and the API implementation for
//                   class RuleBasedBreakIterator
//

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "unicode/uchriter.h"
#include "unicode/udata.h"
#include "unicode/uclean.h"
#include "rbbidata.h"
#include "rbbirb.h"
#include "cmemory.h"
#include "cstring.h"
#include "umutex.h"
#include "ucln_cmn.h"
#include "brkeng.h"

#include "uassert.h"
#include "uvector.h"

// if U_LOCAL_SERVICE_HOOK is defined, then localsvc.cpp is expected to be included.
#if U_LOCAL_SERVICE_HOOK
#include "localsvc.h"
#endif

#ifdef RBBI_DEBUG
static UBool fTrace = FALSE;
#endif

U_NAMESPACE_BEGIN

// The state number of the starting state
#define START_STATE 1

// The state-transition value indicating "stop"
#define STOP_STATE  0


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedBreakIterator)


//=======================================================================
// constructors
//=======================================================================

/**
 * Constructs a RuleBasedBreakIterator that uses the already-created
 * tables object that is passed in as a parameter.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status)
{
    init();
    fData = new RBBIDataWrapper(data, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}

/**
 * Same as above but does not adopt memory
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(const RBBIDataHeader* data, enum EDontAdopt, UErrorCode &status)
{
    init();
    fData = new RBBIDataWrapper(data, RBBIDataWrapper::kDontAdopt, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}


//
//  Construct from precompiled binary rules (tables).  This constructor is public API,
//  taking the rules as a (const uint8_t *) to match the type produced by getBinaryRules().
//
RuleBasedBreakIterator::RuleBasedBreakIterator(const uint8_t *compiledRules,
                       uint32_t       ruleLength,
                       UErrorCode     &status) {
    init();
    if (U_FAILURE(status)) {
        return;
    }
    if (compiledRules == NULL || ruleLength < sizeof(RBBIDataHeader)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    const RBBIDataHeader *data = (const RBBIDataHeader *)compiledRules;
    if (data->fLength > ruleLength) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    fData = new RBBIDataWrapper(data, RBBIDataWrapper::kDontAdopt, status);
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}


//-------------------------------------------------------------------------------
//
//   Constructor   from a UDataMemory handle to precompiled break rules
//                 stored in an ICU data file.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(UDataMemory* udm, UErrorCode &status)
{
    init();
    fData = new RBBIDataWrapper(udm, status); // status checked in constructor
    if (U_FAILURE(status)) {return;}
    if(fData == 0) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
}



//-------------------------------------------------------------------------------
//
//   Constructor       from a set of rules supplied as a string.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator( const UnicodeString  &rules,
                                                UParseError          &parseError,
                                                UErrorCode           &status)
{
    init();
    if (U_FAILURE(status)) {return;}
    RuleBasedBreakIterator *bi = (RuleBasedBreakIterator *)
        RBBIRuleBuilder::createRuleBasedBreakIterator(rules, &parseError, status);
    // Note:  This is a bit awkward.  The RBBI ruleBuilder has a factory method that
    //        creates and returns a complete RBBI.  From here, in a constructor, we
    //        can't just return the object created by the builder factory, hence
    //        the assignment of the factory created object to "this".
    if (U_SUCCESS(status)) {
        *this = *bi;
        delete bi;
    }
}


//-------------------------------------------------------------------------------
//
// Default Constructor.      Create an empty shell that can be set up later.
//                           Used when creating a RuleBasedBreakIterator from a set
//                           of rules.
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator() {
    init();
}


//-------------------------------------------------------------------------------
//
//   Copy constructor.  Will produce a break iterator with the same behavior,
//                      and which iterates over the same text, as the one passed in.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(const RuleBasedBreakIterator& other)
: BreakIterator(other)
{
    this->init();
    *this = other;
}


/**
 * Destructor
 */
RuleBasedBreakIterator::~RuleBasedBreakIterator() {
    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // fCharIter was adopted from the outside.
        delete fCharIter;
    }
    fCharIter = NULL;
    delete fSCharIter;
    fCharIter = NULL;
    delete fDCharIter;
    fDCharIter = NULL;

    utext_close(fText);

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (fCachedBreakPositions) {
        uprv_free(fCachedBreakPositions);
        fCachedBreakPositions = NULL;
    }
    if (fLanguageBreakEngines) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = NULL;
    }
    if (fUnhandledBreakEngine) {
        delete fUnhandledBreakEngine;
        fUnhandledBreakEngine = NULL;
    }
}

/**
 * Assignment operator.  Sets this iterator to have the same behavior,
 * and iterate over the same text, as the one passed in.
 */
RuleBasedBreakIterator&
RuleBasedBreakIterator::operator=(const RuleBasedBreakIterator& that) {
    if (this == &that) {
        return *this;
    }
    reset();    // Delete break cache information
    fBreakType = that.fBreakType;
    if (fLanguageBreakEngines != NULL) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = NULL;   // Just rebuild for now
    }
    // TODO: clone fLanguageBreakEngines from "that"
    UErrorCode status = U_ZERO_ERROR;
    fText = utext_clone(fText, that.fText, FALSE, TRUE, &status);

    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        delete fCharIter;
    }
    fCharIter = NULL;

    if (that.fCharIter != NULL ) {
        // This is a little bit tricky - it will intially appear that
        //  this->fCharIter is adopted, even if that->fCharIter was
        //  not adopted.  That's ok.
        fCharIter = that.fCharIter->clone();
    }

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (that.fData != NULL) {
        fData = that.fData->addReference();
    }

    return *this;
}



//-----------------------------------------------------------------------------
//
//    init()      Shared initialization routine.   Used by all the constructors.
//                Initializes all fields, leaving the object in a consistent state.
//
//-----------------------------------------------------------------------------
void RuleBasedBreakIterator::init() {
    UErrorCode  status    = U_ZERO_ERROR;
    fText                 = utext_openUChars(NULL, NULL, 0, &status);
    fCharIter             = NULL;
    fSCharIter            = NULL;
    fDCharIter            = NULL;
    fData                 = NULL;
    fLastRuleStatusIndex  = 0;
    fLastStatusIndexValid = TRUE;
    fDictionaryCharCount  = 0;
    fBreakType            = UBRK_WORD;  // Defaulting BreakType to word gives reasonable
                                        //   dictionary behavior for Break Iterators that are
                                        //   built from rules.  Even better would be the ability to
                                        //   declare the type in the rules.

    fCachedBreakPositions    = NULL;
    fLanguageBreakEngines    = NULL;
    fUnhandledBreakEngine    = NULL;
    fNumCachedBreakPositions = 0;
    fPositionInCache         = 0;

#ifdef RBBI_DEBUG
    static UBool debugInitDone = FALSE;
    if (debugInitDone == FALSE) {
        char *debugEnv = getenv("U_RBBIDEBUG");
        if (debugEnv && uprv_strstr(debugEnv, "trace")) {
            fTrace = TRUE;
        }
        debugInitDone = TRUE;
    }
#endif
}



//-----------------------------------------------------------------------------
//
//    clone - Returns a newly-constructed RuleBasedBreakIterator with the same
//            behavior, and iterating over the same text, as this one.
//            Virtual function: does the right thing with subclasses.
//
//-----------------------------------------------------------------------------
BreakIterator*
RuleBasedBreakIterator::clone(void) const {
    return new RuleBasedBreakIterator(*this);
}

/**
 * Equality operator.  Returns TRUE if both BreakIterators are of the
 * same class, have the same behavior, and iterate over the same text.
 */
UBool
RuleBasedBreakIterator::operator==(const BreakIterator& that) const {
    if (typeid(*this) != typeid(that)) {
        return FALSE;
    }

    const RuleBasedBreakIterator& that2 = (const RuleBasedBreakIterator&) that;

    if (!utext_equals(fText, that2.fText)) {
        // The two break iterators are operating on different text,
        //   or have a different interation position.
        return FALSE;
    };

    // TODO:  need a check for when in a dictionary region at different offsets.

    if (that2.fData == fData ||
        (fData != NULL && that2.fData != NULL && *that2.fData == *fData)) {
            // The two break iterators are using the same rules.
            return TRUE;
        }
    return FALSE;
}

/**
 * Compute a hash code for this BreakIterator
 * @return A hash code
 */
int32_t
RuleBasedBreakIterator::hashCode(void) const {
    int32_t   hash = 0;
    if (fData != NULL) {
        hash = fData->hashCode();
    }
    return hash;
}


void RuleBasedBreakIterator::setText(UText *ut, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    reset();
    fText = utext_clone(fText, ut, FALSE, TRUE, &status);

    // Set up a dummy CharacterIterator to be returned if anyone
    //   calls getText().  With input from UText, there is no reasonable
    //   way to return a characterIterator over the actual input text.
    //   Return one over an empty string instead - this is the closest
    //   we can come to signaling a failure.
    //   (GetText() is obsolete, this failure is sort of OK)
    if (fDCharIter == NULL) {
        static const UChar c = 0;
        fDCharIter = new UCharCharacterIterator(&c, 0);
        if (fDCharIter == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // existing fCharIter was adopted from the outside.  Delete it now.
        delete fCharIter;
    }
    fCharIter = fDCharIter;

    this->first();
}


UText *RuleBasedBreakIterator::getUText(UText *fillIn, UErrorCode &status) const {
    UText *result = utext_clone(fillIn, fText, FALSE, TRUE, &status);
    return result;
}



/**
 * Returns the description used to create this iterator
 */
const UnicodeString&
RuleBasedBreakIterator::getRules() const {
    if (fData != NULL) {
        return fData->getRuleSourceString();
    } else {
        static const UnicodeString *s;
        if (s == NULL) {
            // TODO:  something more elegant here.
            //        perhaps API should return the string by value.
            //        Note:  thread unsafe init & leak are semi-ok, better than
            //               what was before.  Sould be cleaned up, though.
            s = new UnicodeString;
        }
        return *s;
    }
}

//=======================================================================
// BreakIterator overrides
//=======================================================================

/**
 * Return a CharacterIterator over the text being analyzed.
 */
CharacterIterator&
RuleBasedBreakIterator::getText() const {
    return *fCharIter;
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::adoptText(CharacterIterator* newText) {
    // If we are holding a CharacterIterator adopted from a
    //   previous call to this function, delete it now.
    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        delete fCharIter;
    }

    fCharIter = newText;
    UErrorCode status = U_ZERO_ERROR;
    reset();
    if (newText==NULL || newText->startIndex() != 0) {
        // startIndex !=0 wants to be an error, but there's no way to report it.
        // Make the iterator text be an empty string.
        fText = utext_openUChars(fText, NULL, 0, &status);
    } else {
        fText = utext_openCharacterIterator(fText, newText, &status);
    }
    this->first();
}

/**
 * Set the iterator to analyze a new piece of text.  This function resets
 * the current iteration position to the beginning of the text.
 * @param newText An iterator over the text to analyze.
 */
void
RuleBasedBreakIterator::setText(const UnicodeString& newText) {
    UErrorCode status = U_ZERO_ERROR;
    reset();
    fText = utext_openConstUnicodeString(fText, &newText, &status);

    // Set up a character iterator on the string.
    //   Needed in case someone calls getText().
    //  Can not, unfortunately, do this lazily on the (probably never)
    //  call to getText(), because getText is const.
    if (fSCharIter == NULL) {
        fSCharIter = new StringCharacterIterator(newText);
    } else {
        fSCharIter->setText(newText);
    }

    if (fCharIter!=fSCharIter && fCharIter!=fDCharIter) {
        // old fCharIter was adopted from the outside.  Delete it.
        delete fCharIter;
    }
    fCharIter = fSCharIter;

    this->first();
}


/**
 *  Provide a new UText for the input text.  Must reference text with contents identical
 *  to the original.
 *  Intended for use with text data originating in Java (garbage collected) environments
 *  where the data may be moved in memory at arbitrary times.
 */
RuleBasedBreakIterator &RuleBasedBreakIterator::refreshInputText(UText *input, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return *this;
    }
    if (input == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    int64_t pos = utext_getNativeIndex(fText);
    //  Shallow read-only clone of the new UText into the existing input UText
    fText = utext_clone(fText, input, FALSE, TRUE, &status);
    if (U_FAILURE(status)) {
        return *this;
    }
    utext_setNativeIndex(fText, pos);
    if (utext_getNativeIndex(fText) != pos) {
        // Sanity check.  The new input utext is supposed to have the exact same
        // contents as the old.  If we can't set to the same position, it doesn't.
        // The contents underlying the old utext might be invalid at this point,
        // so it's not safe to check directly.
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return *this;
}


/**
 * Sets the current iteration position to the beginning of the text, position zero.
 * @return The new iterator position, which is zero.
 */
int32_t RuleBasedBreakIterator::first(void) {
    reset();
    fLastRuleStatusIndex  = 0;
    fLastStatusIndexValid = TRUE;
    //if (fText == NULL)
    //    return BreakIterator::DONE;

    utext_setNativeIndex(fText, 0);
    return 0;
}

/**
 * Sets the current iteration position to the end of the text.
 * @return The text's past-the-end offset.
 */
int32_t RuleBasedBreakIterator::last(void) {
    reset();
    if (fText == NULL) {
        fLastRuleStatusIndex  = 0;
        fLastStatusIndexValid = TRUE;
        return BreakIterator::DONE;
    }

    fLastStatusIndexValid = FALSE;
    int32_t pos = (int32_t)utext_nativeLength(fText);
    utext_setNativeIndex(fText, pos);
    return pos;
}

/**
 * Advances the iterator either forward or backward the specified number of steps.
 * Negative values move backward, and positive values move forward.  This is
 * equivalent to repeatedly calling next() or previous().
 * @param n The number of steps to move.  The sign indicates the direction
 * (negative is backwards, and positive is forwards).
 * @return The character offset of the boundary position n boundaries away from
 * the current one.
 */
int32_t RuleBasedBreakIterator::next(int32_t n) {
    int32_t result = current();
    while (n > 0) {
        result = next();
        --n;
    }
    while (n < 0) {
        result = previous();
        ++n;
    }
    return result;
}

/**
 * Advances the iterator to the next boundary position.
 * @return The position of the first boundary after this one.
 */
int32_t RuleBasedBreakIterator::next(void) {
    // if we have cached break positions and we're still in the range
    // covered by them, just move one step forward in the cache
    if (fCachedBreakPositions != NULL) {
        if (fPositionInCache < fNumCachedBreakPositions - 1) {
            ++fPositionInCache;
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    int32_t startPos = current();
    fDictionaryCharCount = 0;
    int32_t result = handleNext(fData->fForwardTable);
    if (fDictionaryCharCount > 0) {
        result = checkDictionary(startPos, result, FALSE);
    }
    return result;
}

/**
 * Advances the iterator backwards, to the last boundary preceding this one.
 * @return The position of the last boundary position preceding this one.
 */
int32_t RuleBasedBreakIterator::previous(void) {
    int32_t result;
    int32_t startPos;

    // if we have cached break positions and we're still in the range
    // covered by them, just move one step backward in the cache
    if (fCachedBreakPositions != NULL) {
        if (fPositionInCache > 0) {
            --fPositionInCache;
            // If we're at the beginning of the cache, need to reevaluate the
            // rule status
            if (fPositionInCache <= 0) {
                fLastStatusIndexValid = FALSE;
            }
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    // if we're already sitting at the beginning of the text, return DONE
    if (fText == NULL || (startPos = current()) == 0) {
        fLastRuleStatusIndex  = 0;
        fLastStatusIndexValid = TRUE;
        return BreakIterator::DONE;
    }

    if (fData->fSafeRevTable != NULL || fData->fSafeFwdTable != NULL) {
        result = handlePrevious(fData->fReverseTable);
        if (fDictionaryCharCount > 0) {
            result = checkDictionary(result, startPos, TRUE);
        }
        return result;
    }

    // old rule syntax
    // set things up.  handlePrevious() will back us up to some valid
    // break position before the current position (we back our internal
    // iterator up one step to prevent handlePrevious() from returning
    // the current position), but not necessarily the last one before
    // where we started

    int32_t start = current();

    (void)UTEXT_PREVIOUS32(fText);
    int32_t lastResult    = handlePrevious(fData->fReverseTable);
    if (lastResult == UBRK_DONE) {
        lastResult = 0;
        utext_setNativeIndex(fText, 0);
    }
    result = lastResult;
    int32_t lastTag       = 0;
    UBool   breakTagValid = FALSE;

    // iterate forward from the known break position until we pass our
    // starting point.  The last break position before the starting
    // point is our return value

    for (;;) {
        result         = next();
        if (result == BreakIterator::DONE || result >= start) {
            break;
        }
        lastResult     = result;
        lastTag        = fLastRuleStatusIndex;
        breakTagValid  = TRUE;
    }

    // fLastBreakTag wants to have the value for section of text preceding
    // the result position that we are to return (in lastResult.)  If
    // the backwards rules overshot and the above loop had to do two or more
    // next()s to move up to the desired return position, we will have a valid
    // tag value. But, if handlePrevious() took us to exactly the correct result position,
    // we wont have a tag value for that position, which is only set by handleNext().

    // Set the current iteration position to be the last break position
    // before where we started, and then return that value.
    utext_setNativeIndex(fText, lastResult);
    fLastRuleStatusIndex  = lastTag;       // for use by getRuleStatus()
    fLastStatusIndexValid = breakTagValid;

    // No need to check the dictionary; it will have been handled by
    // next()

    return lastResult;
}

/**
 * Sets the iterator to refer to the first boundary position following
 * the specified position.
 * @offset The position from which to begin searching for a break position.
 * @return The position of the first break after the current position.
 */
int32_t RuleBasedBreakIterator::following(int32_t offset) {
    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    if (fText == NULL || offset >= utext_nativeLength(fText)) {
        last();
        return next();
    }
    else if (offset < 0) {
        return first();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.
    utext_setNativeIndex(fText, offset);
    offset = (int32_t)utext_getNativeIndex(fText);

    // if we have cached break positions and offset is in the range
    // covered by them, use them
    // TODO: could use binary search
    // TODO: what if offset is outside range, but break is not?
    if (fCachedBreakPositions != NULL) {
        if (offset >= fCachedBreakPositions[0]
                && offset < fCachedBreakPositions[fNumCachedBreakPositions - 1]) {
            fPositionInCache = 0;
            // We are guaranteed not to leave the array due to range test above
            while (offset >= fCachedBreakPositions[fPositionInCache]) {
                ++fPositionInCache;
            }
            int32_t pos = fCachedBreakPositions[fPositionInCache];
            utext_setNativeIndex(fText, pos);
            return pos;
        }
        else {
            reset();
        }
    }

    // Set our internal iteration position (temporarily)
    // to the position passed in.  If this is the _beginning_ position,
    // then we can just use next() to get our return value

    int32_t result = 0;

    if (fData->fSafeRevTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        // move forward one codepoint to prepare for moving back to a
        // safe point.
        // this handles offset being between a supplementary character
        // TODO: is this still needed, with move to code point boundary handled above?
        (void)UTEXT_NEXT32(fText);
        // handlePrevious will move most of the time to < 1 boundary away
        handlePrevious(fData->fSafeRevTable);
        int32_t result = next();
        while (result <= offset) {
            result = next();
        }
        return result;
    }
    if (fData->fSafeFwdTable != NULL) {
        // backup plan if forward safe table is not available
        utext_setNativeIndex(fText, offset);
        (void)UTEXT_PREVIOUS32(fText);
        // handle next will give result >= offset
        handleNext(fData->fSafeFwdTable);
        // previous will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = previous();
        while (oldresult > offset) {
            int32_t result = previous();
            if (result <= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        int32_t result = next();
        if (result <= offset) {
            return next();
        }
        return result;
    }
    // otherwise, we have to sync up first.  Use handlePrevious() to back
    // up to a known break position before the specified position (if
    // we can determine that the specified position is a break position,
    // we don't back up at all).  This may or may not be the last break
    // position at or before our starting position.  Advance forward
    // from here until we've passed the starting position.  The position
    // we stop on will be the first break position after the specified one.
    // old rule syntax

    utext_setNativeIndex(fText, offset);
    if (offset==0 ||
        (offset==1  && utext_getNativeIndex(fText)==0)) {
        return next();
    }
    result = previous();

    while (result != BreakIterator::DONE && result <= offset) {
        result = next();
    }

    return result;
}

/**
 * Sets the iterator to refer to the last boundary position before the
 * specified position.
 * @offset The position to begin searching for a break from.
 * @return The position of the last boundary before the starting position.
 */
int32_t RuleBasedBreakIterator::preceding(int32_t offset) {
    // if the offset passed in is already past the end of the text,
    // just return DONE; if it's before the beginning, return the
    // text's starting offset
    if (fText == NULL || offset > utext_nativeLength(fText)) {
        return last();
    }
    else if (offset < 0) {
        return first();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.
    utext_setNativeIndex(fText, offset);
    offset = (int32_t)utext_getNativeIndex(fText);

    // if we have cached break positions and offset is in the range
    // covered by them, use them
    if (fCachedBreakPositions != NULL) {
        // TODO: binary search?
        // TODO: What if offset is outside range, but break is not?
        if (offset > fCachedBreakPositions[0]
                && offset <= fCachedBreakPositions[fNumCachedBreakPositions - 1]) {
            fPositionInCache = 0;
            while (fPositionInCache < fNumCachedBreakPositions
                   && offset > fCachedBreakPositions[fPositionInCache])
                ++fPositionInCache;
            --fPositionInCache;
            // If we're at the beginning of the cache, need to reevaluate the
            // rule status
            if (fPositionInCache <= 0) {
                fLastStatusIndexValid = FALSE;
            }
            utext_setNativeIndex(fText, fCachedBreakPositions[fPositionInCache]);
            return fCachedBreakPositions[fPositionInCache];
        }
        else {
            reset();
        }
    }

    // if we start by updating the current iteration position to the
    // position specified by the caller, we can just use previous()
    // to carry out this operation

    if (fData->fSafeFwdTable != NULL) {
        // new rule syntax
        utext_setNativeIndex(fText, offset);
        int32_t newOffset = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        if (newOffset != offset) {
            // Will come here if specified offset was not a code point boundary AND
            //   the underlying implmentation is using UText, which snaps any non-code-point-boundary
            //   indices to the containing code point.
            // For breakitereator::preceding only, these non-code-point indices need to be moved
            //   up to refer to the following codepoint.
            (void)UTEXT_NEXT32(fText);
            offset = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        }

        // TODO:  (synwee) would it be better to just check for being in the middle of a surrogate pair,
        //        rather than adjusting the position unconditionally?
        //        (Change would interact with safe rules.)
        // TODO:  change RBBI behavior for off-boundary indices to match that of UText?
        //        affects only preceding(), seems cleaner, but is slightly different.
        (void)UTEXT_PREVIOUS32(fText);
        handleNext(fData->fSafeFwdTable);
        int32_t result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        while (result >= offset) {
            result = previous();
        }
        return result;
    }
    if (fData->fSafeRevTable != NULL) {
        // backup plan if forward safe table is not available
        //  TODO:  check whether this path can be discarded
        //         It's probably OK to say that rules must supply both safe tables
        //            if they use safe tables at all.  We have certainly never described
        //            to anyone how to work with just one safe table.
        utext_setNativeIndex(fText, offset);
        (void)UTEXT_NEXT32(fText);

        // handle previous will give result <= offset
        handlePrevious(fData->fSafeRevTable);

        // next will give result 0 or 1 boundary away from offset,
        // most of the time
        // we have to
        int32_t oldresult = next();
        while (oldresult < offset) {
            int32_t result = next();
            if (result >= offset) {
                return oldresult;
            }
            oldresult = result;
        }
        int32_t result = previous();
        if (result >= offset) {
            return previous();
        }
        return result;
    }

    // old rule syntax
    utext_setNativeIndex(fText, offset);
    return previous();
}

/**
 * Returns true if the specfied position is a boundary position.  As a side
 * effect, leaves the iterator pointing to the first boundary position at
 * or after "offset".
 * @param offset the offset to check.
 * @return True if "offset" is a boundary position.
 */
UBool RuleBasedBreakIterator::isBoundary(int32_t offset) {
    // the beginning index of the iterator is always a boundary position by definition
    if (offset == 0) {
        first();       // For side effects on current position, tag values.
        return TRUE;
    }

    if (offset == (int32_t)utext_nativeLength(fText)) {
        last();       // For side effects on current position, tag values.
        return TRUE;
    }

    // out-of-range indexes are never boundary positions
    if (offset < 0) {
        first();       // For side effects on current position, tag values.
        return FALSE;
    }

    if (offset > utext_nativeLength(fText)) {
        last();        // For side effects on current position, tag values.
        return FALSE;
    }

    // otherwise, we can use following() on the position before the specified
    // one and return true if the position we get back is the one the user
    // specified
    utext_previous32From(fText, offset);
    int32_t backOne = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    UBool    result  = following(backOne) == offset;
    return result;
}

/**
 * Returns the current iteration position.
 * @return The current iteration position.
 */
int32_t RuleBasedBreakIterator::current(void) const {
    int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    return pos;
}

//=======================================================================
// implementation
//=======================================================================

//
// RBBIRunMode  -  the state machine runs an extra iteration at the beginning and end
//                 of user text.  A variable with this enum type keeps track of where we
//                 are.  The state machine only fetches user input while in the RUN mode.
//
enum RBBIRunMode {
    RBBI_START,     // state machine processing is before first char of input
    RBBI_RUN,       // state machine processing is in the user text
    RBBI_END        // state machine processing is after end of user text.
};


// Map from look-ahead break states (corresponds to rules) to boundary positions.
// Allows multiple lookahead break rules to be in flight at the same time.
//
// This is a temporary approach for ICU 57. A better fix is to make the look-ahead numbers
// in the state table be sequential, then we can just index an array. And the
// table could also tell us in advance how big that array needs to be.
//
// Before ICU 57 there was just a single simple variable for a look-ahead match that
// was in progress. Two rules at once did not work.

static const int32_t kMaxLookaheads = 8;
struct LookAheadResults {
    int32_t    fUsedSlotLimit;
    int32_t    fPositions[8];
    int16_t    fKeys[8];

    LookAheadResults() : fUsedSlotLimit(0), fPositions(), fKeys() {};

    int32_t getPosition(int16_t key) {
        for (int32_t i=0; i<fUsedSlotLimit; ++i) {
            if (fKeys[i] == key) {
                return fPositions[i];
            }
        }
        U_ASSERT(FALSE);
        return -1;
    }

    void setPosition(int16_t key, int32_t position) {
        int32_t i;
        for (i=0; i<fUsedSlotLimit; ++i) {
            if (fKeys[i] == key) {
                fPositions[i] = position;
                return;
            }
        }
        if (i >= kMaxLookaheads) {
            U_ASSERT(FALSE);
            i = kMaxLookaheads - 1;
        }
        fKeys[i] = key;
        fPositions[i] = position;
        U_ASSERT(fUsedSlotLimit == i);
        fUsedSlotLimit = i + 1;
    }
};


//-----------------------------------------------------------------------------------
//
//  handleNext(stateTable)
//     This method is the actual implementation of the rbbi next() method.
//     This method initializes the state machine to state 1
//     and advances through the text character by character until we reach the end
//     of the text or the state machine transitions to state 0.  We update our return
//     value every time the state machine passes through an accepting state.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handleNext(const RBBIStateTable *statetable) {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;

    RBBIStateTableRow  *row;
    UChar32             c;
    LookAheadResults    lookAheadMatches;
    int32_t             result             = 0;
    int32_t             initialPosition    = 0;
    const char         *tableData          = statetable->fTableData;
    uint32_t            tableRowLen        = statetable->fRowLen;

    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPuts("Handle Next   pos   char  state category");
        }
    #endif

    // No matter what, handleNext alway correctly sets the break tag value.
    fLastStatusIndexValid = TRUE;
    fLastRuleStatusIndex = 0;

    // if we're already at the end of the text, return DONE.
    initialPosition = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    result          = initialPosition;
    c               = UTEXT_NEXT32(fText);
    if (fData == NULL || c==U_SENTINEL) {
        return BreakIterator::DONE;
    }

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            //(statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the end of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                fDictionaryCharCount++;
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

       #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4ld   ", utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //

        // Note: fNextState is defined as uint16_t[2], but we are casting
        // a generated RBBI table to RBBIStateTableRow and some tables
        // actually have more than 2 categories.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            // (statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


        if (row->fAccepting == -1) {
            // Match found, common case.
            if (mode != RBBI_START) {
                result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            }
            fLastRuleStatusIndex = row->fTagIdx;   // Remember the break status (tag) values.
        }

        int16_t completedRule = row->fAccepting;
        if (completedRule > 0) {
            // Lookahead match is completed.
            int32_t lookaheadResult = lookAheadMatches.getPosition(completedRule);
            if (lookaheadResult >= 0) {
                fLastRuleStatusIndex = row->fTagIdx;
                UTEXT_SETNATIVEINDEX(fText, lookaheadResult);
                return lookaheadResult;
            }
        }
        int16_t rule = row->fLookAhead;
        if (rule != 0) {
            // At the position of a '/' in a look-ahead match. Record it.
            int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            lookAheadMatches.setPosition(rule, pos);
        }

        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

        // Advance to the next character.
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = UTEXT_NEXT32(fText);
        } else {
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }


    }

    // The state machine is done.  Check whether it found a match...

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        UTEXT_SETNATIVEINDEX(fText, initialPosition);
        UTEXT_NEXT32(fText);
        result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    }

    // Leave the iterator at our result position.
    UTEXT_SETNATIVEINDEX(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}



//-----------------------------------------------------------------------------------
//
//  handlePrevious()
//
//      Iterate backwards, according to the logic of the reverse rules.
//      This version handles the exact style backwards rules.
//
//      The logic of this function is very similar to handleNext(), above.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handlePrevious(const RBBIStateTable *statetable) {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;
    RBBIStateTableRow  *row;
    UChar32             c;
    LookAheadResults    lookAheadMatches;
    int32_t             result          = 0;
    int32_t             initialPosition = 0;

    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPuts("Handle Previous   pos   char  state category");
        }
    #endif

    // handlePrevious() never gets the rule status.
    // Flag the status as invalid; if the user ever asks for status, we will need
    // to back up, then re-find the break position using handleNext(), which does
    // get the status value.
    fLastStatusIndexValid = FALSE;
    fLastRuleStatusIndex = 0;

    // if we're already at the start of the text, return DONE.
    if (fText == NULL || fData == NULL || UTEXT_GETNATIVEINDEX(fText)==0) {
        return BreakIterator::DONE;
    }

    //  Set up the starting char.
    initialPosition = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    result          = initialPosition;
    c               = UTEXT_PREVIOUS32(fText);

    //  Set the initial state for the state machine
    state = START_STATE;
    row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));
    category = 3;
    mode     = RBBI_RUN;
    if (statetable->fFlags & RBBI_BOF_REQUIRED) {
        category = 2;
        mode     = RBBI_START;
    }


    // loop until we reach the start of the text or transition to state 0
    //
    for (;;) {
        if (c == U_SENTINEL) {
            // Reached end of input string.
            if (mode == RBBI_END) {
                // We have already run the loop one last time with the
                //   character set to the psueudo {eof} value.  Now it is time
                //   to unconditionally bail out.
                if (result == initialPosition) {
                    // Ran off start, no match found.
                    // move one index one (towards the start, since we are doing a previous())
                    UTEXT_SETNATIVEINDEX(fText, initialPosition);
                    (void)UTEXT_PREVIOUS32(fText);   // TODO:  shouldn't be necessary.  We're already at beginning.  Check.
                }
                break;
            }
            // Run the loop one last time with the fake end-of-input character category.
            mode = RBBI_END;
            category = 1;
        }

        //
        // Get the char category.  An incoming category of 1 or 2 means that
        //      we are preset for doing the beginning or end of input, and
        //      that we shouldn't get a category from an actual text input character.
        //
        if (mode == RBBI_RUN) {
            // look up the current character's character category, which tells us
            // which column in the state table to look at.
            // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
            //        not the size of the character going in, which is a UChar32.
            //
            UTRIE_GET16(&fData->fTrie, c, category);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iterators (subclasses).
            //    Chars that need to be handled by a dictionary have a flag bit set
            //    in their category values.
            //
            if ((category & 0x4000) != 0)  {
                fDictionaryCharCount++;
                //  And off the dictionary flag bit.
                category &= ~0x4000;
            }
        }

        #ifdef RBBI_DEBUG
            if (fTrace) {
                RBBIDebugPrintf("             %4d   ", (int32_t)utext_getNativeIndex(fText));
                if (0x20<=c && c<0x7f) {
                    RBBIDebugPrintf("\"%c\"  ", c);
                } else {
                    RBBIDebugPrintf("%5x  ", c);
                }
                RBBIDebugPrintf("%3d  %3d\n", state, category);
            }
        #endif

        // State Transition - move machine to its next state
        //

        // Note: fNextState is defined as uint16_t[2], but we are casting
        // a generated RBBI table to RBBIStateTableRow and some tables
        // actually have more than 2 categories.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            (statetable->fTableData + (statetable->fRowLen * state));

        if (row->fAccepting == -1) {
            // Match found, common case.
            result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
        }

        int16_t completedRule = row->fAccepting;
        if (completedRule > 0) {
            // Lookahead match is completed.
            int32_t lookaheadResult = lookAheadMatches.getPosition(completedRule);
            if (lookaheadResult >= 0) {
                UTEXT_SETNATIVEINDEX(fText, lookaheadResult);
                return lookaheadResult;
            }
        }
        int16_t rule = row->fLookAhead;
        if (rule != 0) {
            // At the position of a '/' in a look-ahead match. Record it.
            int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            lookAheadMatches.setPosition(rule, pos);
        }

        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // We have advanced through the string until it is certain that no
            //   longer match is possible, no matter what characters follow.
            break;
        }

        // Move (backwards) to the next character to process.
        // If this is a beginning-of-input loop iteration, don't advance
        //    the input position.  The next iteration will be processing the
        //    first real input character.
        if (mode == RBBI_RUN) {
            c = UTEXT_PREVIOUS32(fText);
        } else {
            if (mode == RBBI_START) {
                mode = RBBI_RUN;
            }
        }
    }

    // The state machine is done.  Check whether it found a match...

    // If the iterator failed to advance in the match engine, force it ahead by one.
    //   (This really indicates a defect in the break rules.  They should always match
    //    at least one character.)
    if (result == initialPosition) {
        UTEXT_SETNATIVEINDEX(fText, initialPosition);
        UTEXT_PREVIOUS32(fText);
        result = (int32_t)UTEXT_GETNATIVEINDEX(fText);
    }

    // Leave the iterator at our result position.
    UTEXT_SETNATIVEINDEX(fText, result);
    #ifdef RBBI_DEBUG
        if (fTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


void
RuleBasedBreakIterator::reset()
{
    if (fCachedBreakPositions) {
        uprv_free(fCachedBreakPositions);
    }
    fCachedBreakPositions = NULL;
    fNumCachedBreakPositions = 0;
    fDictionaryCharCount = 0;
    fPositionInCache = 0;
}



//-------------------------------------------------------------------------------
//
//   getRuleStatus()   Return the break rule tag associated with the current
//                     iterator position.  If the iterator arrived at its current
//                     position by iterating forwards, the value will have been
//                     cached by the handleNext() function.
//
//                     If no cached status value is available, the status is
//                     found by doing a previous() followed by a next(), which
//                     leaves the iterator where it started, and computes the
//                     status while doing the next().
//
//-------------------------------------------------------------------------------
void RuleBasedBreakIterator::makeRuleStatusValid() {
    if (fLastStatusIndexValid == FALSE) {
        //  No cached status is available.
        if (fText == NULL || current() == 0) {
            //  At start of text, or there is no text.  Status is always zero.
            fLastRuleStatusIndex = 0;
            fLastStatusIndexValid = TRUE;
        } else {
            //  Not at start of text.  Find status the tedious way.
            int32_t pa = current();
            previous();
            if (fNumCachedBreakPositions > 0) {
                reset();                // Blow off the dictionary cache
            }
            int32_t pb = next();
            if (pa != pb) {
                // note: the if (pa != pb) test is here only to eliminate warnings for
                //       unused local variables on gcc.  Logically, it isn't needed.
                U_ASSERT(pa == pb);
            }
        }
    }
    U_ASSERT(fLastRuleStatusIndex >= 0  &&  fLastRuleStatusIndex < fData->fStatusMaxIdx);
}


int32_t  RuleBasedBreakIterator::getRuleStatus() const {
    RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
    nonConstThis->makeRuleStatusValid();

    // fLastRuleStatusIndex indexes to the start of the appropriate status record
    //                                                 (the number of status values.)
    //   This function returns the last (largest) of the array of status values.
    int32_t  idx = fLastRuleStatusIndex + fData->fRuleStatusTable[fLastRuleStatusIndex];
    int32_t  tagVal = fData->fRuleStatusTable[idx];

    return tagVal;
}




int32_t RuleBasedBreakIterator::getRuleStatusVec(
             int32_t *fillInVec, int32_t capacity, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return 0;
    }

    RuleBasedBreakIterator *nonConstThis  = (RuleBasedBreakIterator *)this;
    nonConstThis->makeRuleStatusValid();
    int32_t  numVals = fData->fRuleStatusTable[fLastRuleStatusIndex];
    int32_t  numValsToCopy = numVals;
    if (numVals > capacity) {
        status = U_BUFFER_OVERFLOW_ERROR;
        numValsToCopy = capacity;
    }
    int i;
    for (i=0; i<numValsToCopy; i++) {
        fillInVec[i] = fData->fRuleStatusTable[fLastRuleStatusIndex + i + 1];
    }
    return numVals;
}



//-------------------------------------------------------------------------------
//
//   getBinaryRules        Access to the compiled form of the rules,
//                         for use by build system tools that save the data
//                         for standard iterator types.
//
//-------------------------------------------------------------------------------
const uint8_t  *RuleBasedBreakIterator::getBinaryRules(uint32_t &length) {
    const uint8_t  *retPtr = NULL;
    length = 0;

    if (fData != NULL) {
        retPtr = (const uint8_t *)fData->fHeader;
        length = fData->fHeader->fLength;
    }
    return retPtr;
}


BreakIterator *  RuleBasedBreakIterator::createBufferClone(void * /*stackBuffer*/,
                                   int32_t &bufferSize,
                                   UErrorCode &status)
{
    if (U_FAILURE(status)){
        return NULL;
    }

    if (bufferSize == 0) {
        bufferSize = 1;  // preflighting for deprecated functionality
        return NULL;
    }

    BreakIterator *clonedBI = clone();
    if (clonedBI == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        status = U_SAFECLONE_ALLOCATED_WARNING;
    }
    return (RuleBasedBreakIterator *)clonedBI;
}


//-------------------------------------------------------------------------------
//
//  isDictionaryChar      Return true if the category lookup for this char
//                        indicates that it is in the set of dictionary lookup
//                        chars.
//
//                        This function is intended for use by dictionary based
//                        break iterators.
//
//-------------------------------------------------------------------------------
/*UBool RuleBasedBreakIterator::isDictionaryChar(UChar32   c) {
    if (fData == NULL) {
        return FALSE;
    }
    uint16_t category;
    UTRIE_GET16(&fData->fTrie, c, category);
    return (category & 0x4000) != 0;
}*/


//-------------------------------------------------------------------------------
//
//  checkDictionary       This function handles all processing of characters in
//                        the "dictionary" set. It will determine the appropriate
//                        course of action, and possibly set up a cache in the
//                        process.
//
//-------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::checkDictionary(int32_t startPos,
                            int32_t endPos,
                            UBool reverse) {
    // Reset the old break cache first.
    reset();

    // note: code segment below assumes that dictionary chars are in the
    // startPos-endPos range
    // value returned should be next character in sequence
    if ((endPos - startPos) <= 1) {
        return (reverse ? startPos : endPos);
    }

    // Starting from the starting point, scan towards the proposed result,
    // looking for the first dictionary character (which may be the one
    // we're on, if we're starting in the middle of a range).
    utext_setNativeIndex(fText, reverse ? endPos : startPos);
    if (reverse) {
        UTEXT_PREVIOUS32(fText);
    }

    int32_t rangeStart = startPos;
    int32_t rangeEnd = endPos;

    uint16_t    category;
    int32_t     current;
    UErrorCode  status = U_ZERO_ERROR;
    UStack      breaks(status);
    int32_t     foundBreakCount = 0;
    UChar32     c = utext_current32(fText);

    UTRIE_GET16(&fData->fTrie, c, category);

    // Is the character we're starting on a dictionary character? If so, we
    // need to back up to include the entire run; otherwise the results of
    // the break algorithm will differ depending on where we start. Since
    // the result is cached and there is typically a non-dictionary break
    // within a small number of words, there should be little performance impact.
    if (category & 0x4000) {
        if (reverse) {
            do {
                utext_next32(fText);          // TODO:  recast to work directly with postincrement.
                c = utext_current32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            } while (c != U_SENTINEL && (category & 0x4000));
            // Back up to the last dictionary character
            rangeEnd = (int32_t)UTEXT_GETNATIVEINDEX(fText);
            if (c == U_SENTINEL) {
                // c = fText->last32();
                //   TODO:  why was this if needed?
                c = UTEXT_PREVIOUS32(fText);
            }
            else {
                c = UTEXT_PREVIOUS32(fText);
            }
        }
        else {
            do {
                c = UTEXT_PREVIOUS32(fText);
                UTRIE_GET16(&fData->fTrie, c, category);
            }
            while (c != U_SENTINEL && (category & 0x4000));
            // Back up to the last dictionary character
            if (c == U_SENTINEL) {
                // c = fText->first32();
                c = utext_current32(fText);
            }
            else {
                utext_next32(fText);
                c = utext_current32(fText);
            }
            rangeStart = (int32_t)UTEXT_GETNATIVEINDEX(fText);;
        }
        UTRIE_GET16(&fData->fTrie, c, category);
    }

    // Loop through the text, looking for ranges of dictionary characters.
    // For each span, find the appropriate break engine, and ask it to find
    // any breaks within the span.
    // Note: we always do this in the forward direction, so that the break
    // cache is built in the right order.
    if (reverse) {
        utext_setNativeIndex(fText, rangeStart);
        c = utext_current32(fText);
        UTRIE_GET16(&fData->fTrie, c, category);
    }
    while(U_SUCCESS(status)) {
        while((current = (int32_t)UTEXT_GETNATIVEINDEX(fText)) < rangeEnd && (category & 0x4000) == 0) {
            utext_next32(fText);           // TODO:  tweak for post-increment operation
            c = utext_current32(fText);
            UTRIE_GET16(&fData->fTrie, c, category);
        }
        if (current >= rangeEnd) {
            break;
        }

        // We now have a dictionary character. Get the appropriate language object
        // to deal with it.
        const LanguageBreakEngine *lbe = getLanguageBreakEngine(c);

        // Ask the language object if there are any breaks. It will leave the text
        // pointer on the other side of its range, ready to search for the next one.
        if (lbe != NULL) {
            foundBreakCount += lbe->findBreaks(fText, rangeStart, rangeEnd, FALSE, fBreakType, breaks);
        }

        // Reload the loop variables for the next go-round
        c = utext_current32(fText);
        UTRIE_GET16(&fData->fTrie, c, category);
    }

    // If we found breaks, build a new break cache. The first and last entries must
    // be the original starting and ending position.
    if (foundBreakCount > 0) {
        U_ASSERT(foundBreakCount == breaks.size());
        int32_t totalBreaks = foundBreakCount;
        if (startPos < breaks.elementAti(0)) {
            totalBreaks += 1;
        }
        if (endPos > breaks.peeki()) {
            totalBreaks += 1;
        }
        fCachedBreakPositions = (int32_t *)uprv_malloc(totalBreaks * sizeof(int32_t));
        if (fCachedBreakPositions != NULL) {
            int32_t out = 0;
            fNumCachedBreakPositions = totalBreaks;
            if (startPos < breaks.elementAti(0)) {
                fCachedBreakPositions[out++] = startPos;
            }
            for (int32_t i = 0; i < foundBreakCount; ++i) {
                fCachedBreakPositions[out++] = breaks.elementAti(i);
            }
            if (endPos > fCachedBreakPositions[out-1]) {
                fCachedBreakPositions[out] = endPos;
            }
            // If there are breaks, then by definition, we are replacing the original
            // proposed break by one of the breaks we found. Use following() and
            // preceding() to do the work. They should never recurse in this case.
            if (reverse) {
                return preceding(endPos);
            }
            else {
                return following(startPos);
            }
        }
        // If the allocation failed, just fall through to the "no breaks found" case.
    }

    // If we get here, there were no language-based breaks. Set the text pointer
    // to the original proposed break.
    utext_setNativeIndex(fText, reverse ? startPos : endPos);
    return (reverse ? startPos : endPos);
}

U_NAMESPACE_END


static icu::UStack *gLanguageBreakFactories = NULL;
static icu::UInitOnce gLanguageBreakFactoriesInitOnce = U_INITONCE_INITIALIZER;

/**
 * Release all static memory held by breakiterator.
 */
U_CDECL_BEGIN
static UBool U_CALLCONV breakiterator_cleanup_dict(void) {
    if (gLanguageBreakFactories) {
        delete gLanguageBreakFactories;
        gLanguageBreakFactories = NULL;
    }
    gLanguageBreakFactoriesInitOnce.reset();
    return TRUE;
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV _deleteFactory(void *obj) {
    delete (icu::LanguageBreakFactory *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static void U_CALLCONV initLanguageFactories() {
    UErrorCode status = U_ZERO_ERROR;
    U_ASSERT(gLanguageBreakFactories == NULL);
    gLanguageBreakFactories = new UStack(_deleteFactory, NULL, status);
    if (gLanguageBreakFactories != NULL && U_SUCCESS(status)) {
        ICULanguageBreakFactory *builtIn = new ICULanguageBreakFactory(status);
        gLanguageBreakFactories->push(builtIn, status);
#ifdef U_LOCAL_SERVICE_HOOK
        LanguageBreakFactory *extra = (LanguageBreakFactory *)uprv_svc_hook("languageBreakFactory", &status);
        if (extra != NULL) {
            gLanguageBreakFactories->push(extra, status);
        }
#endif
    }
    ucln_common_registerCleanup(UCLN_COMMON_BREAKITERATOR_DICT, breakiterator_cleanup_dict);
}


static const LanguageBreakEngine*
getLanguageBreakEngineFromFactory(UChar32 c, int32_t breakType)
{
    umtx_initOnce(gLanguageBreakFactoriesInitOnce, &initLanguageFactories);
    if (gLanguageBreakFactories == NULL) {
        return NULL;
    }

    int32_t i = gLanguageBreakFactories->size();
    const LanguageBreakEngine *lbe = NULL;
    while (--i >= 0) {
        LanguageBreakFactory *factory = (LanguageBreakFactory *)(gLanguageBreakFactories->elementAt(i));
        lbe = factory->getEngineFor(c, breakType);
        if (lbe != NULL) {
            break;
        }
    }
    return lbe;
}


//-------------------------------------------------------------------------------
//
//  getLanguageBreakEngine  Find an appropriate LanguageBreakEngine for the
//                          the character c.
//
//-------------------------------------------------------------------------------
const LanguageBreakEngine *
RuleBasedBreakIterator::getLanguageBreakEngine(UChar32 c) {
    const LanguageBreakEngine *lbe = NULL;
    UErrorCode status = U_ZERO_ERROR;

    if (fLanguageBreakEngines == NULL) {
        fLanguageBreakEngines = new UStack(status);
        if (fLanguageBreakEngines == NULL || U_FAILURE(status)) {
            delete fLanguageBreakEngines;
            fLanguageBreakEngines = 0;
            return NULL;
        }
    }

    int32_t i = fLanguageBreakEngines->size();
    while (--i >= 0) {
        lbe = (const LanguageBreakEngine *)(fLanguageBreakEngines->elementAt(i));
        if (lbe->handles(c, fBreakType)) {
            return lbe;
        }
    }

    // No existing dictionary took the character. See if a factory wants to
    // give us a new LanguageBreakEngine for this character.
    lbe = getLanguageBreakEngineFromFactory(c, fBreakType);

    // If we got one, use it and push it on our stack.
    if (lbe != NULL) {
        fLanguageBreakEngines->push((void *)lbe, status);
        // Even if we can't remember it, we can keep looking it up, so
        // return it even if the push fails.
        return lbe;
    }

    // No engine is forthcoming for this character. Add it to the
    // reject set. Create the reject break engine if needed.
    if (fUnhandledBreakEngine == NULL) {
        fUnhandledBreakEngine = new UnhandledEngine(status);
        if (U_SUCCESS(status) && fUnhandledBreakEngine == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        // Put it last so that scripts for which we have an engine get tried
        // first.
        fLanguageBreakEngines->insertElementAt(fUnhandledBreakEngine, 0, status);
        // If we can't insert it, or creation failed, get rid of it
        if (U_FAILURE(status)) {
            delete fUnhandledBreakEngine;
            fUnhandledBreakEngine = 0;
            return NULL;
        }
    }

    // Tell the reject engine about the character; at its discretion, it may
    // add more than just the one character.
    fUnhandledBreakEngine->handleCharacter(c, fBreakType);

    return fUnhandledBreakEngine;
}



/*int32_t RuleBasedBreakIterator::getBreakType() const {
    return fBreakType;
}*/

void RuleBasedBreakIterator::setBreakType(int32_t type) {
    fBreakType = type;
    reset();
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
