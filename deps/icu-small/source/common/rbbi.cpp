// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
***************************************************************************
*   Copyright (C) 1999-2016 International Business Machines Corporation
*   and others. All rights reserved.
***************************************************************************
*/
//
//  file:  rbbi.cpp  Contains the implementation of the rule based break iterator
//                   runtime engine and the API implementation for
//                   class RuleBasedBreakIterator
//

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include <cinttypes>

#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "unicode/uchriter.h"
#include "unicode/uclean.h"
#include "unicode/udata.h"

#include "brkeng.h"
#include "ucln_cmn.h"
#include "cmemory.h"
#include "cstring.h"
#include "localsvc.h"
#include "rbbidata.h"
#include "rbbi_cache.h"
#include "rbbirb.h"
#include "uassert.h"
#include "umutex.h"
#include "uvectr32.h"

#ifdef RBBI_DEBUG
static UBool gTrace = FALSE;
#endif

U_NAMESPACE_BEGIN

// The state number of the starting state
constexpr int32_t START_STATE = 1;

// The state-transition value indicating "stop"
constexpr int32_t STOP_STATE = 0;


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(RuleBasedBreakIterator)


//=======================================================================
// constructors
//=======================================================================

/**
 * Constructs a RuleBasedBreakIterator that uses the already-created
 * tables object that is passed in as a parameter.
 */
RuleBasedBreakIterator::RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status)
 : fSCharIter(UnicodeString())
{
    init(status);
    fData = new RBBIDataWrapper(data, status); // status checked in constructor
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
                       UErrorCode     &status)
 : fSCharIter(UnicodeString())
{
    init(status);
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
 : fSCharIter(UnicodeString())
{
    init(status);
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
 : fSCharIter(UnicodeString())
{
    init(status);
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
RuleBasedBreakIterator::RuleBasedBreakIterator()
 : fSCharIter(UnicodeString())
{
    UErrorCode status = U_ZERO_ERROR;
    init(status);
}


//-------------------------------------------------------------------------------
//
//   Copy constructor.  Will produce a break iterator with the same behavior,
//                      and which iterates over the same text, as the one passed in.
//
//-------------------------------------------------------------------------------
RuleBasedBreakIterator::RuleBasedBreakIterator(const RuleBasedBreakIterator& other)
: BreakIterator(other),
  fSCharIter(UnicodeString())
{
    UErrorCode status = U_ZERO_ERROR;
    this->init(status);
    *this = other;
}


/**
 * Destructor
 */
RuleBasedBreakIterator::~RuleBasedBreakIterator() {
    if (fCharIter != &fSCharIter) {
        // fCharIter was adopted from the outside.
        delete fCharIter;
    }
    fCharIter = NULL;

    utext_close(&fText);

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    delete fBreakCache;
    fBreakCache = NULL;

    delete fDictionaryCache;
    fDictionaryCache = NULL;

    delete fLanguageBreakEngines;
    fLanguageBreakEngines = NULL;

    delete fUnhandledBreakEngine;
    fUnhandledBreakEngine = NULL;
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
    BreakIterator::operator=(that);

    if (fLanguageBreakEngines != NULL) {
        delete fLanguageBreakEngines;
        fLanguageBreakEngines = NULL;   // Just rebuild for now
    }
    // TODO: clone fLanguageBreakEngines from "that"
    UErrorCode status = U_ZERO_ERROR;
    utext_clone(&fText, &that.fText, FALSE, TRUE, &status);

    if (fCharIter != &fSCharIter) {
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

    if (that.fCharIter != NULL && that.fCharIter != &that.fSCharIter) {
        // This is a little bit tricky - it will intially appear that
        //  this->fCharIter is adopted, even if that->fCharIter was
        //  not adopted.  That's ok.
        fCharIter = that.fCharIter->clone();
    }
    fSCharIter = that.fSCharIter;
    if (fCharIter == NULL) {
        fCharIter = &fSCharIter;
    }

    if (fData != NULL) {
        fData->removeReference();
        fData = NULL;
    }
    if (that.fData != NULL) {
        fData = that.fData->addReference();
    }

    fPosition = that.fPosition;
    fRuleStatusIndex = that.fRuleStatusIndex;
    fDone = that.fDone;

    // TODO: both the dictionary and the main cache need to be copied.
    //       Current position could be within a dictionary range. Trying to continue
    //       the iteration without the caches present would go to the rules, with
    //       the assumption that the current position is on a rule boundary.
    fBreakCache->reset(fPosition, fRuleStatusIndex);
    fDictionaryCache->reset();

    return *this;
}



//-----------------------------------------------------------------------------
//
//    init()      Shared initialization routine.   Used by all the constructors.
//                Initializes all fields, leaving the object in a consistent state.
//
//-----------------------------------------------------------------------------
void RuleBasedBreakIterator::init(UErrorCode &status) {
    fCharIter             = NULL;
    fData                 = NULL;
    fPosition             = 0;
    fRuleStatusIndex      = 0;
    fDone                 = false;
    fDictionaryCharCount  = 0;
    fLanguageBreakEngines = NULL;
    fUnhandledBreakEngine = NULL;
    fBreakCache           = NULL;
    fDictionaryCache      = NULL;

    // Note: IBM xlC is unable to assign or initialize member fText from UTEXT_INITIALIZER.
    // fText                 = UTEXT_INITIALIZER;
    static const UText initializedUText = UTEXT_INITIALIZER;
    uprv_memcpy(&fText, &initializedUText, sizeof(UText));

   if (U_FAILURE(status)) {
        return;
    }

    utext_openUChars(&fText, NULL, 0, &status);
    fDictionaryCache = new DictionaryCache(this, status);
    fBreakCache      = new BreakCache(this, status);
    if (U_SUCCESS(status) && (fDictionaryCache == NULL || fBreakCache == NULL)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }

#ifdef RBBI_DEBUG
    static UBool debugInitDone = FALSE;
    if (debugInitDone == FALSE) {
        char *debugEnv = getenv("U_RBBIDEBUG");
        if (debugEnv && uprv_strstr(debugEnv, "trace")) {
            gTrace = TRUE;
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
    if (this == &that) {
        return TRUE;
    }

    // The base class BreakIterator carries no state that participates in equality,
    // and does not implement an equality function that would otherwise be
    // checked at this point.

    const RuleBasedBreakIterator& that2 = (const RuleBasedBreakIterator&) that;

    if (!utext_equals(&fText, &that2.fText)) {
        // The two break iterators are operating on different text,
        //   or have a different iteration position.
        //   Note that fText's position is always the same as the break iterator's position.
        return FALSE;
    };

    if (!(fPosition == that2.fPosition &&
            fRuleStatusIndex == that2.fRuleStatusIndex &&
            fDone == that2.fDone)) {
        return FALSE;
    }

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
    fBreakCache->reset();
    fDictionaryCache->reset();
    utext_clone(&fText, ut, FALSE, TRUE, &status);

    // Set up a dummy CharacterIterator to be returned if anyone
    //   calls getText().  With input from UText, there is no reasonable
    //   way to return a characterIterator over the actual input text.
    //   Return one over an empty string instead - this is the closest
    //   we can come to signaling a failure.
    //   (GetText() is obsolete, this failure is sort of OK)
    fSCharIter.setText(UnicodeString());

    if (fCharIter != &fSCharIter) {
        // existing fCharIter was adopted from the outside.  Delete it now.
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

    this->first();
}


UText *RuleBasedBreakIterator::getUText(UText *fillIn, UErrorCode &status) const {
    UText *result = utext_clone(fillIn, &fText, FALSE, TRUE, &status);
    return result;
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
    if (fCharIter != &fSCharIter) {
        delete fCharIter;
    }

    fCharIter = newText;
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->reset();
    fDictionaryCache->reset();
    if (newText==NULL || newText->startIndex() != 0) {
        // startIndex !=0 wants to be an error, but there's no way to report it.
        // Make the iterator text be an empty string.
        utext_openUChars(&fText, NULL, 0, &status);
    } else {
        utext_openCharacterIterator(&fText, newText, &status);
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
    fBreakCache->reset();
    fDictionaryCache->reset();
    utext_openConstUnicodeString(&fText, &newText, &status);

    // Set up a character iterator on the string.
    //   Needed in case someone calls getText().
    //  Can not, unfortunately, do this lazily on the (probably never)
    //  call to getText(), because getText is const.
    fSCharIter.setText(newText);

    if (fCharIter != &fSCharIter) {
        // old fCharIter was adopted from the outside.  Delete it.
        delete fCharIter;
    }
    fCharIter = &fSCharIter;

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
    int64_t pos = utext_getNativeIndex(&fText);
    //  Shallow read-only clone of the new UText into the existing input UText
    utext_clone(&fText, input, FALSE, TRUE, &status);
    if (U_FAILURE(status)) {
        return *this;
    }
    utext_setNativeIndex(&fText, pos);
    if (utext_getNativeIndex(&fText) != pos) {
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
    UErrorCode status = U_ZERO_ERROR;
    if (!fBreakCache->seek(0)) {
        fBreakCache->populateNear(0, status);
    }
    fBreakCache->current();
    U_ASSERT(fPosition == 0);
    return 0;
}

/**
 * Sets the current iteration position to the end of the text.
 * @return The text's past-the-end offset.
 */
int32_t RuleBasedBreakIterator::last(void) {
    int32_t endPos = (int32_t)utext_nativeLength(&fText);
    UBool endShouldBeBoundary = isBoundary(endPos);      // Has side effect of setting iterator position.
    (void)endShouldBeBoundary;
    U_ASSERT(endShouldBeBoundary);
    U_ASSERT(fPosition == endPos);
    return endPos;
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
    int32_t result = 0;
    if (n > 0) {
        for (; n > 0 && result != UBRK_DONE; --n) {
            result = next();
        }
    } else if (n < 0) {
        for (; n < 0 && result != UBRK_DONE; ++n) {
            result = previous();
        }
    } else {
        result = current();
    }
    return result;
}

/**
 * Advances the iterator to the next boundary position.
 * @return The position of the first boundary after this one.
 */
int32_t RuleBasedBreakIterator::next(void) {
    fBreakCache->next();
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Move the iterator backwards, to the boundary preceding the current one.
 *
 *         Starts from the current position within fText.
 *         Starting position need not be on a boundary.
 *
 * @return The position of the boundary position immediately preceding the starting position.
 */
int32_t RuleBasedBreakIterator::previous(void) {
    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->previous(status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Sets the iterator to refer to the first boundary position following
 * the specified position.
 * @param startPos The position from which to begin searching for a break position.
 * @return The position of the first break after the current position.
 */
int32_t RuleBasedBreakIterator::following(int32_t startPos) {
    // if the supplied position is before the beginning, return the
    // text's starting offset
    if (startPos < 0) {
        return first();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8. Or it may be beyond the end of the text.
    utext_setNativeIndex(&fText, startPos);
    startPos = (int32_t)utext_getNativeIndex(&fText);

    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->following(startPos, status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Sets the iterator to refer to the last boundary position before the
 * specified position.
 * @param offset The position to begin searching for a break from.
 * @return The position of the last boundary before the starting position.
 */
int32_t RuleBasedBreakIterator::preceding(int32_t offset) {
    if (offset > utext_nativeLength(&fText)) {
        return last();
    }

    // Move requested offset to a code point start. It might be on a trail surrogate,
    // or on a trail byte if the input is UTF-8.

    utext_setNativeIndex(&fText, offset);
    int32_t adjustedOffset = static_cast<int32_t>(utext_getNativeIndex(&fText));

    UErrorCode status = U_ZERO_ERROR;
    fBreakCache->preceding(adjustedOffset, status);
    return fDone ? UBRK_DONE : fPosition;
}

/**
 * Returns true if the specfied position is a boundary position.  As a side
 * effect, leaves the iterator pointing to the first boundary position at
 * or after "offset".
 *
 * @param offset the offset to check.
 * @return True if "offset" is a boundary position.
 */
UBool RuleBasedBreakIterator::isBoundary(int32_t offset) {
    // out-of-range indexes are never boundary positions
    if (offset < 0) {
        first();       // For side effects on current position, tag values.
        return FALSE;
    }

    // Adjust offset to be on a code point boundary and not beyond the end of the text.
    // Note that isBoundary() is always false for offsets that are not on code point boundaries.
    // But we still need the side effect of leaving iteration at the following boundary.

    utext_setNativeIndex(&fText, offset);
    int32_t adjustedOffset = static_cast<int32_t>(utext_getNativeIndex(&fText));

    bool result = false;
    UErrorCode status = U_ZERO_ERROR;
    if (fBreakCache->seek(adjustedOffset) || fBreakCache->populateNear(adjustedOffset, status)) {
        result = (fBreakCache->current() == offset);
    }

    if (result && adjustedOffset < offset && utext_char32At(&fText, offset) == U_SENTINEL) {
        // Original offset is beyond the end of the text. Return FALSE, it's not a boundary,
        // but the iteration position remains set to the end of the text, which is a boundary.
        return FALSE;
    }
    if (!result) {
        // Not on a boundary. isBoundary() must leave iterator on the following boundary.
        // Cache->seek(), above, left us on the preceding boundary, so advance one.
        next();
    }
    return result;
}


/**
 * Returns the current iteration position.
 * @return The current iteration position.
 */
int32_t RuleBasedBreakIterator::current(void) const {
    return fPosition;
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

    LookAheadResults() : fUsedSlotLimit(0), fPositions(), fKeys() {}

    int32_t getPosition(int16_t key) {
        for (int32_t i=0; i<fUsedSlotLimit; ++i) {
            if (fKeys[i] == key) {
                return fPositions[i];
            }
        }
        UPRV_UNREACHABLE;
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
            UPRV_UNREACHABLE;
        }
        fKeys[i] = key;
        fPositions[i] = position;
        U_ASSERT(fUsedSlotLimit == i);
        fUsedSlotLimit = i + 1;
    }
};


//-----------------------------------------------------------------------------------
//
//  handleNext()
//     Run the state machine to find a boundary
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handleNext() {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIRunMode         mode;

    RBBIStateTableRow  *row;
    UChar32             c;
    LookAheadResults    lookAheadMatches;
    int32_t             result             = 0;
    int32_t             initialPosition    = 0;
    const RBBIStateTable *statetable       = fData->fForwardTable;
    const char         *tableData          = statetable->fTableData;
    uint32_t            tableRowLen        = statetable->fRowLen;
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPuts("Handle Next   pos   char  state category");
        }
    #endif

    // handleNext alway sets the break tag value.
    // Set the default for it.
    fRuleStatusIndex = 0;

    fDictionaryCharCount = 0;

    // if we're already at the end of the text, return DONE.
    initialPosition = fPosition;
    UTEXT_SETNATIVEINDEX(&fText, initialPosition);
    result          = initialPosition;
    c               = UTEXT_NEXT32(&fText);
    if (c==U_SENTINEL) {
        fDone = TRUE;
        return UBRK_DONE;
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
            category = UTRIE2_GET16(fData->fTrie, c);

            // Check the dictionary bit in the character's category.
            //    Counter is only used by dictionary based iteration.
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
            if (gTrace) {
                RBBIDebugPrintf("             %4" PRId64 "   ", utext_getNativeIndex(&fText));
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

        // fNextState is a variable-length array.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            // (statetable->fTableData + (statetable->fRowLen * state));
            (tableData + tableRowLen * state);


        if (row->fAccepting == -1) {
            // Match found, common case.
            if (mode != RBBI_START) {
                result = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
            }
            fRuleStatusIndex = row->fTagIdx;   // Remember the break status (tag) values.
        }

        int16_t completedRule = row->fAccepting;
        if (completedRule > 0) {
            // Lookahead match is completed.
            int32_t lookaheadResult = lookAheadMatches.getPosition(completedRule);
            if (lookaheadResult >= 0) {
                fRuleStatusIndex = row->fTagIdx;
                fPosition = lookaheadResult;
                return lookaheadResult;
            }
        }
        int16_t rule = row->fLookAhead;
        if (rule != 0) {
            // At the position of a '/' in a look-ahead match. Record it.
            int32_t  pos = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
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
            c = UTEXT_NEXT32(&fText);
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
        utext_setNativeIndex(&fText, initialPosition);
        utext_next32(&fText);
        result = (int32_t)utext_getNativeIndex(&fText);
        fRuleStatusIndex = 0;
    }

    // Leave the iterator at our result position.
    fPosition = result;
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}


//-----------------------------------------------------------------------------------
//
//  handleSafePrevious()
//
//      Iterate backwards using the safe reverse rules.
//      The logic of this function is similar to handleNext(), but simpler
//      because the safe table does not require as many options.
//
//-----------------------------------------------------------------------------------
int32_t RuleBasedBreakIterator::handleSafePrevious(int32_t fromPosition) {
    int32_t             state;
    uint16_t            category        = 0;
    RBBIStateTableRow  *row;
    UChar32             c;
    int32_t             result          = 0;

    const RBBIStateTable *stateTable = fData->fReverseTable;
    UTEXT_SETNATIVEINDEX(&fText, fromPosition);
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPuts("Handle Previous   pos   char  state category");
        }
    #endif

    // if we're already at the start of the text, return DONE.
    if (fData == NULL || UTEXT_GETNATIVEINDEX(&fText)==0) {
        return BreakIterator::DONE;
    }

    //  Set the initial state for the state machine
    c = UTEXT_PREVIOUS32(&fText);
    state = START_STATE;
    row = (RBBIStateTableRow *)
            (stateTable->fTableData + (stateTable->fRowLen * state));

    // loop until we reach the start of the text or transition to state 0
    //
    for (; c != U_SENTINEL; c = UTEXT_PREVIOUS32(&fText)) {

        // look up the current character's character category, which tells us
        // which column in the state table to look at.
        // Note:  the 16 in UTRIE_GET16 refers to the size of the data being returned,
        //        not the size of the character going in, which is a UChar32.
        //
        //  And off the dictionary flag bit. For reverse iteration it is not used.
        category = UTRIE2_GET16(fData->fTrie, c);
        category &= ~0x4000;

        #ifdef RBBI_DEBUG
            if (gTrace) {
                RBBIDebugPrintf("             %4d   ", (int32_t)utext_getNativeIndex(&fText));
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
        // fNextState is a variable-length array.
        U_ASSERT(category<fData->fHeader->fCatCount);
        state = row->fNextState[category];  /*Not accessing beyond memory*/
        row = (RBBIStateTableRow *)
            (stateTable->fTableData + (stateTable->fRowLen * state));

        if (state == STOP_STATE) {
            // This is the normal exit from the lookup state machine.
            // Transistion to state zero means we have found a safe point.
            break;
        }
    }

    // The state machine is done.  Check whether it found a match...
    result = (int32_t)UTEXT_GETNATIVEINDEX(&fText);
    #ifdef RBBI_DEBUG
        if (gTrace) {
            RBBIDebugPrintf("result = %d\n\n", result);
        }
    #endif
    return result;
}

//-------------------------------------------------------------------------------
//
//   getRuleStatus()   Return the break rule tag associated with the current
//                     iterator position.  If the iterator arrived at its current
//                     position by iterating forwards, the value will have been
//                     cached by the handleNext() function.
//
//-------------------------------------------------------------------------------

int32_t  RuleBasedBreakIterator::getRuleStatus() const {

    // fLastRuleStatusIndex indexes to the start of the appropriate status record
    //                                                 (the number of status values.)
    //   This function returns the last (largest) of the array of status values.
    int32_t  idx = fRuleStatusIndex + fData->fRuleStatusTable[fRuleStatusIndex];
    int32_t  tagVal = fData->fRuleStatusTable[idx];

    return tagVal;
}


int32_t RuleBasedBreakIterator::getRuleStatusVec(
             int32_t *fillInVec, int32_t capacity, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }

    int32_t  numVals = fData->fRuleStatusTable[fRuleStatusIndex];
    int32_t  numValsToCopy = numVals;
    if (numVals > capacity) {
        status = U_BUFFER_OVERFLOW_ERROR;
        numValsToCopy = capacity;
    }
    int i;
    for (i=0; i<numValsToCopy; i++) {
        fillInVec[i] = fData->fRuleStatusTable[fRuleStatusIndex + i + 1];
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

U_NAMESPACE_END


static icu::UStack *gLanguageBreakFactories = nullptr;
static const icu::UnicodeString *gEmptyString = nullptr;
static icu::UInitOnce gLanguageBreakFactoriesInitOnce = U_INITONCE_INITIALIZER;
static icu::UInitOnce gRBBIInitOnce = U_INITONCE_INITIALIZER;

/**
 * Release all static memory held by breakiterator.
 */
U_CDECL_BEGIN
static UBool U_CALLCONV rbbi_cleanup(void) {
    delete gLanguageBreakFactories;
    gLanguageBreakFactories = nullptr;
    delete gEmptyString;
    gEmptyString = nullptr;
    gLanguageBreakFactoriesInitOnce.reset();
    gRBBIInitOnce.reset();
    return TRUE;
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV _deleteFactory(void *obj) {
    delete (icu::LanguageBreakFactory *) obj;
}
U_CDECL_END
U_NAMESPACE_BEGIN

static void U_CALLCONV rbbiInit() {
    gEmptyString = new UnicodeString();
    ucln_common_registerCleanup(UCLN_COMMON_RBBI, rbbi_cleanup);
}

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
    ucln_common_registerCleanup(UCLN_COMMON_RBBI, rbbi_cleanup);
}


static const LanguageBreakEngine*
getLanguageBreakEngineFromFactory(UChar32 c)
{
    umtx_initOnce(gLanguageBreakFactoriesInitOnce, &initLanguageFactories);
    if (gLanguageBreakFactories == NULL) {
        return NULL;
    }

    int32_t i = gLanguageBreakFactories->size();
    const LanguageBreakEngine *lbe = NULL;
    while (--i >= 0) {
        LanguageBreakFactory *factory = (LanguageBreakFactory *)(gLanguageBreakFactories->elementAt(i));
        lbe = factory->getEngineFor(c);
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
        if (lbe->handles(c)) {
            return lbe;
        }
    }

    // No existing dictionary took the character. See if a factory wants to
    // give us a new LanguageBreakEngine for this character.
    lbe = getLanguageBreakEngineFromFactory(c);

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
            return nullptr;
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
    fUnhandledBreakEngine->handleCharacter(c);

    return fUnhandledBreakEngine;
}

void RuleBasedBreakIterator::dumpCache() {
    fBreakCache->dumpCache();
}

void RuleBasedBreakIterator::dumpTables() {
    fData->printData();
}

/**
 * Returns the description used to create this iterator
 */

const UnicodeString&
RuleBasedBreakIterator::getRules() const {
    if (fData != NULL) {
        return fData->getRuleSourceString();
    } else {
        umtx_initOnce(gRBBIInitOnce, &rbbiInit);
        return *gEmptyString;
    }
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
