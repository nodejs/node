/*
***************************************************************************
*   Copyright (C) 1999-2014 International Business Machines Corporation   *
*   and others. All rights reserved.                                      *
***************************************************************************

**********************************************************************
*   Date        Name        Description
*   10/22/99    alan        Creation.
*   11/11/99    rgillam     Complete port from Java.
**********************************************************************
*/

#ifndef RBBI_H
#define RBBI_H

#include "unicode/utypes.h"

/**
 * \file
 * \brief C++ API: Rule Based Break Iterator
 */

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/udata.h"
#include "unicode/parseerr.h"
#include "unicode/schriter.h"
#include "unicode/uchriter.h"


struct UTrie;

U_NAMESPACE_BEGIN

/** @internal */
struct RBBIDataHeader;
class  RuleBasedBreakIteratorTables;
class  BreakIterator;
class  RBBIDataWrapper;
class  UStack;
class  LanguageBreakEngine;
class  UnhandledEngine;
struct RBBIStateTable;




/**
 *
 * A subclass of BreakIterator whose behavior is specified using a list of rules.
 * <p>Instances of this class are most commonly created by the factory methods of
 *  BreakIterator::createWordInstance(), BreakIterator::createLineInstance(), etc.,
 *  and then used via the abstract API in class BreakIterator</p>
 *
 * <p>See the ICU User Guide for information on Break Iterator Rules.</p>
 *
 * <p>This class is not intended to be subclassed.  (Class DictionaryBasedBreakIterator
 *    is a subclass, but that relationship is effectively internal to the ICU
 *    implementation.  The subclassing interface to RulesBasedBreakIterator is
 *    not part of the ICU API, and may not remain stable.</p>
 *
 */
class U_COMMON_API RuleBasedBreakIterator /*U_FINAL*/ : public BreakIterator {

protected:
    /**
     * The UText through which this BreakIterator accesses the text
     * @internal
     */
    UText  *fText;

    /**
     *   A character iterator that refers to the same text as the UText, above.
     *   Only included for compatibility with old API, which was based on CharacterIterators.
     *   Value may be adopted from outside, or one of fSCharIter or fDCharIter, below.
     */
    CharacterIterator  *fCharIter;

    /**
     *   When the input text is provided by a UnicodeString, this will point to
     *    a characterIterator that wraps that data.  Needed only for the
     *    implementation of getText(), a backwards compatibility issue.
     */
    StringCharacterIterator *fSCharIter;

    /**
     *  When the input text is provided by a UText, this
     *    dummy CharacterIterator over an empty string will
     *    be returned from getText()
     */
    UCharCharacterIterator *fDCharIter;

    /**
     * The rule data for this BreakIterator instance
     * @internal
     */
    RBBIDataWrapper    *fData;

    /** Index of the Rule {tag} values for the most recent match.
     *  @internal
    */
    int32_t             fLastRuleStatusIndex;

    /**
     * Rule tag value valid flag.
     * Some iterator operations don't intrinsically set the correct tag value.
     * This flag lets us lazily compute the value if we are ever asked for it.
     * @internal
     */
    UBool               fLastStatusIndexValid;

    /**
     * Counter for the number of characters encountered with the "dictionary"
     *   flag set.
     * @internal
     */
    uint32_t            fDictionaryCharCount;

    /**
     * When a range of characters is divided up using the dictionary, the break
     * positions that are discovered are stored here, preventing us from having
     * to use either the dictionary or the state table again until the iterator
     * leaves this range of text. Has the most impact for line breaking.
     * @internal
     */
    int32_t*            fCachedBreakPositions;

    /**
     * The number of elements in fCachedBreakPositions
     * @internal
     */
    int32_t             fNumCachedBreakPositions;

    /**
     * if fCachedBreakPositions is not null, this indicates which item in the
     * cache the current iteration position refers to
     * @internal
     */
    int32_t             fPositionInCache;

    /**
     *
     * If present, UStack of LanguageBreakEngine objects that might handle
     * dictionary characters. Searched from top to bottom to find an object to
     * handle a given character.
     * @internal
     */
    UStack              *fLanguageBreakEngines;

    /**
     *
     * If present, the special LanguageBreakEngine used for handling
     * characters that are in the dictionary set, but not handled by any
     * LangugageBreakEngine.
     * @internal
     */
    UnhandledEngine     *fUnhandledBreakEngine;

    /**
     *
     * The type of the break iterator, or -1 if it has not been set.
     * @internal
     */
    int32_t             fBreakType;

protected:
    //=======================================================================
    // constructors
    //=======================================================================

#ifndef U_HIDE_INTERNAL_API
    /**
     * Constant to be used in the constructor
     * RuleBasedBreakIterator(RBBIDataHeader*, EDontAdopt, UErrorCode &);
     * which does not adopt the memory indicated by the RBBIDataHeader*
     * parameter.
     *
     * @internal
     */
    enum EDontAdopt {
        kDontAdopt
    };

    /**
     * Constructor from a flattened set of RBBI data in malloced memory.
     *             RulesBasedBreakIterators built from a custom set of rules
     *             are created via this constructor; the rules are compiled
     *             into memory, then the break iterator is constructed here.
     *
     *             The break iterator adopts the memory, and will
     *             free it when done.
     * @internal
     */
    RuleBasedBreakIterator(RBBIDataHeader* data, UErrorCode &status);

    /**
     * Constructor from a flattened set of RBBI data in memory which need not
     *             be malloced (e.g. it may be a memory-mapped file, etc.).
     *
     *             This version does not adopt the memory, and does not
     *             free it when done.
     * @internal
     */
    RuleBasedBreakIterator(const RBBIDataHeader* data, enum EDontAdopt dontAdopt, UErrorCode &status);
#endif  /* U_HIDE_INTERNAL_API */


    friend class RBBIRuleBuilder;
    /** @internal */
    friend class BreakIterator;



public:

    /** Default constructor.  Creates an empty shell of an iterator, with no
     *  rules or text to iterate over.   Object can subsequently be assigned to.
     *  @stable ICU 2.2
     */
    RuleBasedBreakIterator();

    /**
     * Copy constructor.  Will produce a break iterator with the same behavior,
     * and which iterates over the same text, as the one passed in.
     * @param that The RuleBasedBreakIterator passed to be copied
     * @stable ICU 2.0
     */
    RuleBasedBreakIterator(const RuleBasedBreakIterator& that);

    /**
     * Construct a RuleBasedBreakIterator from a set of rules supplied as a string.
     * @param rules The break rules to be used.
     * @param parseError  In the event of a syntax error in the rules, provides the location
     *                    within the rules of the problem.
     * @param status Information on any errors encountered.
     * @stable ICU 2.2
     */
    RuleBasedBreakIterator( const UnicodeString    &rules,
                             UParseError           &parseError,
                             UErrorCode            &status);

    /**
     * Contruct a RuleBasedBreakIterator from a set of precompiled binary rules.
     * Binary rules are obtained from RulesBasedBreakIterator::getBinaryRules().
     * Construction of a break iterator in this way is substantially faster than
     * constuction from source rules.
     *
     * Ownership of the storage containing the compiled rules remains with the
     * caller of this function.  The compiled rules must not be  modified or
     * deleted during the life of the break iterator.
     *
     * The compiled rules are not compatible across different major versions of ICU.
     * The compiled rules are comaptible only between machines with the same
     * byte ordering (little or big endian) and the same base character set family
     * (ASCII or EBCDIC).
     *
     * @see #getBinaryRules
     * @param compiledRules A pointer to the compiled break rules to be used.
     * @param ruleLength The length of the compiled break rules, in bytes.  This
     *   corresponds to the length value produced by getBinaryRules().
     * @param status Information on any errors encountered, including invalid
     *   binary rules.
     * @stable ICU 4.8
     */
    RuleBasedBreakIterator(const uint8_t *compiledRules,
                           uint32_t       ruleLength,
                           UErrorCode    &status);

    /**
     * This constructor uses the udata interface to create a BreakIterator
     * whose internal tables live in a memory-mapped file.  "image" is an
     * ICU UDataMemory handle for the pre-compiled break iterator tables.
     * @param image handle to the memory image for the break iterator data.
     *        Ownership of the UDataMemory handle passes to the Break Iterator,
     *        which will be responsible for closing it when it is no longer needed.
     * @param status Information on any errors encountered.
     * @see udata_open
     * @see #getBinaryRules
     * @stable ICU 2.8
     */
    RuleBasedBreakIterator(UDataMemory* image, UErrorCode &status);

    /**
     * Destructor
     *  @stable ICU 2.0
     */
    virtual ~RuleBasedBreakIterator();

    /**
     * Assignment operator.  Sets this iterator to have the same behavior,
     * and iterate over the same text, as the one passed in.
     * @param that The RuleBasedBreakItertor passed in
     * @return the newly created RuleBasedBreakIterator
     *  @stable ICU 2.0
     */
    RuleBasedBreakIterator& operator=(const RuleBasedBreakIterator& that);

    /**
     * Equality operator.  Returns TRUE if both BreakIterators are of the
     * same class, have the same behavior, and iterate over the same text.
     * @param that The BreakIterator to be compared for equality
     * @return TRUE if both BreakIterators are of the
     * same class, have the same behavior, and iterate over the same text.
     *  @stable ICU 2.0
     */
    virtual UBool operator==(const BreakIterator& that) const;

    /**
     * Not-equal operator.  If operator== returns TRUE, this returns FALSE,
     * and vice versa.
     * @param that The BreakIterator to be compared for inequality
     * @return TRUE if both BreakIterators are not same.
     *  @stable ICU 2.0
     */
    UBool operator!=(const BreakIterator& that) const;

    /**
     * Returns a newly-constructed RuleBasedBreakIterator with the same
     * behavior, and iterating over the same text, as this one.
     * Differs from the copy constructor in that it is polymorphic, and
     * will correctly clone (copy) a derived class.
     * clone() is thread safe.  Multiple threads may simultaeneously
     * clone the same source break iterator.
     * @return a newly-constructed RuleBasedBreakIterator
     * @stable ICU 2.0
     */
    virtual BreakIterator* clone() const;

    /**
     * Compute a hash code for this BreakIterator
     * @return A hash code
     *  @stable ICU 2.0
     */
    virtual int32_t hashCode(void) const;

    /**
     * Returns the description used to create this iterator
     * @return the description used to create this iterator
     *  @stable ICU 2.0
     */
    virtual const UnicodeString& getRules(void) const;

    //=======================================================================
    // BreakIterator overrides
    //=======================================================================

    /**
     * <p>
     * Return a CharacterIterator over the text being analyzed.
     * The returned character iterator is owned by the break iterator, and must
     * not be deleted by the caller.  Repeated calls to this function may
     * return the same CharacterIterator.
     * </p>
     * <p>
     * The returned character iterator must not be used concurrently with
     * the break iterator.  If concurrent operation is needed, clone the
     * returned character iterator first and operate on the clone.
     * </p>
     * <p>
     * When the break iterator is operating on text supplied via a UText,
     * this function will fail.  Lacking any way to signal failures, it
     * returns an CharacterIterator containing no text.
     * The function getUText() provides similar functionality,
     * is reliable, and is more efficient.
     * </p>
     *
     * TODO:  deprecate this function?
     *
     * @return An iterator over the text being analyzed.
     * @stable ICU 2.0
     */
    virtual  CharacterIterator& getText(void) const;


    /**
      *  Get a UText for the text being analyzed.
      *  The returned UText is a shallow clone of the UText used internally
      *  by the break iterator implementation.  It can safely be used to
      *  access the text without impacting any break iterator operations,
      *  but the underlying text itself must not be altered.
      *
      * @param fillIn A UText to be filled in.  If NULL, a new UText will be
      *           allocated to hold the result.
      * @param status receives any error codes.
      * @return   The current UText for this break iterator.  If an input
      *           UText was provided, it will always be returned.
      * @stable ICU 3.4
      */
     virtual UText *getUText(UText *fillIn, UErrorCode &status) const;

    /**
     * Set the iterator to analyze a new piece of text.  This function resets
     * the current iteration position to the beginning of the text.
     * @param newText An iterator over the text to analyze.  The BreakIterator
     * takes ownership of the character iterator.  The caller MUST NOT delete it!
     *  @stable ICU 2.0
     */
    virtual void adoptText(CharacterIterator* newText);

    /**
     * Set the iterator to analyze a new piece of text.  This function resets
     * the current iteration position to the beginning of the text.
     * @param newText The text to analyze.
     *  @stable ICU 2.0
     */
    virtual void setText(const UnicodeString& newText);

    /**
     * Reset the break iterator to operate over the text represented by
     * the UText.  The iterator position is reset to the start.
     *
     * This function makes a shallow clone of the supplied UText.  This means
     * that the caller is free to immediately close or otherwise reuse the
     * Utext that was passed as a parameter, but that the underlying text itself
     * must not be altered while being referenced by the break iterator.
     *
     * @param text    The UText used to change the text.
     * @param status  Receives any error codes.
     * @stable ICU 3.4
     */
    virtual void  setText(UText *text, UErrorCode &status);

    /**
     * Sets the current iteration position to the beginning of the text, position zero.
     * @return The offset of the beginning of the text, zero.
     *  @stable ICU 2.0
     */
    virtual int32_t first(void);

    /**
     * Sets the current iteration position to the end of the text.
     * @return The text's past-the-end offset.
     *  @stable ICU 2.0
     */
    virtual int32_t last(void);

    /**
     * Advances the iterator either forward or backward the specified number of steps.
     * Negative values move backward, and positive values move forward.  This is
     * equivalent to repeatedly calling next() or previous().
     * @param n The number of steps to move.  The sign indicates the direction
     * (negative is backwards, and positive is forwards).
     * @return The character offset of the boundary position n boundaries away from
     * the current one.
     *  @stable ICU 2.0
     */
    virtual int32_t next(int32_t n);

    /**
     * Advances the iterator to the next boundary position.
     * @return The position of the first boundary after this one.
     *  @stable ICU 2.0
     */
    virtual int32_t next(void);

    /**
     * Moves the iterator backwards, to the last boundary preceding this one.
     * @return The position of the last boundary position preceding this one.
     *  @stable ICU 2.0
     */
    virtual int32_t previous(void);

    /**
     * Sets the iterator to refer to the first boundary position following
     * the specified position.
     * @param offset The position from which to begin searching for a break position.
     * @return The position of the first break after the current position.
     *  @stable ICU 2.0
     */
    virtual int32_t following(int32_t offset);

    /**
     * Sets the iterator to refer to the last boundary position before the
     * specified position.
     * @param offset The position to begin searching for a break from.
     * @return The position of the last boundary before the starting position.
     *  @stable ICU 2.0
     */
    virtual int32_t preceding(int32_t offset);

    /**
     * Returns true if the specfied position is a boundary position.  As a side
     * effect, leaves the iterator pointing to the first boundary position at
     * or after "offset".
     * @param offset the offset to check.
     * @return True if "offset" is a boundary position.
     *  @stable ICU 2.0
     */
    virtual UBool isBoundary(int32_t offset);

    /**
     * Returns the current iteration position.
     * @return The current iteration position.
     * @stable ICU 2.0
     */
    virtual int32_t current(void) const;


    /**
     * Return the status tag from the break rule that determined the most recently
     * returned break position.  For break rules that do not specify a
     * status, a default value of 0 is returned.  If more than one break rule
     * would cause a boundary to be located at some position in the text,
     * the numerically largest of the applicable status values is returned.
     * <p>
     * Of the standard types of ICU break iterators, only word break and
     * line break provide status values.  The values are defined in
     * the header file ubrk.h.  For Word breaks, the status allows distinguishing between words
     * that contain alphabetic letters, "words" that appear to be numbers,
     * punctuation and spaces, words containing ideographic characters, and
     * more.  For Line Break, the status distinguishes between hard (mandatory) breaks
     * and soft (potential) break positions.
     * <p>
     * <code>getRuleStatus()</code> can be called after obtaining a boundary
     * position from <code>next()</code>, <code>previous()</code>, or
     * any other break iterator functions that returns a boundary position.
     * <p>
     * When creating custom break rules, one is free to define whatever
     * status values may be convenient for the application.
     * <p>
     * Note: this function is not thread safe.  It should not have been
     *       declared const, and the const remains only for compatibility
     *       reasons.  (The function is logically const, but not bit-wise const).
     * <p>
     * @return the status from the break rule that determined the most recently
     * returned break position.
     *
     * @see UWordBreak
     * @stable ICU 2.2
     */
    virtual int32_t getRuleStatus() const;

   /**
    * Get the status (tag) values from the break rule(s) that determined the most
    * recently returned break position.
    * <p>
    * The returned status value(s) are stored into an array provided by the caller.
    * The values are stored in sorted (ascending) order.
    * If the capacity of the output array is insufficient to hold the data,
    *  the output will be truncated to the available length, and a
    *  U_BUFFER_OVERFLOW_ERROR will be signaled.
    *
    * @param fillInVec an array to be filled in with the status values.
    * @param capacity  the length of the supplied vector.  A length of zero causes
    *                  the function to return the number of status values, in the
    *                  normal way, without attemtping to store any values.
    * @param status    receives error codes.
    * @return          The number of rule status values from rules that determined
    *                  the most recent boundary returned by the break iterator.
    *                  In the event of a U_BUFFER_OVERFLOW_ERROR, the return value
    *                  is the total number of status values that were available,
    *                  not the reduced number that were actually returned.
    * @see getRuleStatus
    * @stable ICU 3.0
    */
    virtual int32_t getRuleStatusVec(int32_t *fillInVec, int32_t capacity, UErrorCode &status);

    /**
     * Returns a unique class ID POLYMORPHICALLY.  Pure virtual override.
     * This method is to implement a simple version of RTTI, since not all
     * C++ compilers support genuine RTTI.  Polymorphic operator==() and
     * clone() methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

    /**
     * Returns the class ID for this class.  This is useful only for
     * comparing to a return value from getDynamicClassID().  For example:
     *
     *      Base* polymorphic_pointer = createPolymorphicObject();
     *      if (polymorphic_pointer->getDynamicClassID() ==
     *          Derived::getStaticClassID()) ...
     *
     * @return          The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Deprecated functionality. Use clone() instead.
     *
     * Create a clone (copy) of this break iterator in memory provided
     *  by the caller.  The idea is to increase performance by avoiding
     *  a storage allocation.  Use of this functoin is NOT RECOMMENDED.
     *  Performance gains are minimal, and correct buffer management is
     *  tricky.  Use clone() instead.
     *
     * @param stackBuffer  The pointer to the memory into which the cloned object
     *                     should be placed.  If NULL,  allocate heap memory
     *                     for the cloned object.
     * @param BufferSize   The size of the buffer.  If zero, return the required
     *                     buffer size, but do not clone the object.  If the
     *                     size was too small (but not zero), allocate heap
     *                     storage for the cloned object.
     *
     * @param status       Error status.  U_SAFECLONE_ALLOCATED_WARNING will be
     *                     returned if the the provided buffer was too small, and
     *                     the clone was therefore put on the heap.
     *
     * @return  Pointer to the clone object.  This may differ from the stackBuffer
     *          address if the byte alignment of the stack buffer was not suitable
     *          or if the stackBuffer was too small to hold the clone.
     * @deprecated ICU 52. Use clone() instead.
     */
    virtual BreakIterator *  createBufferClone(void *stackBuffer,
                                               int32_t &BufferSize,
                                               UErrorCode &status);


    /**
     * Return the binary form of compiled break rules,
     * which can then be used to create a new break iterator at some
     * time in the future.  Creating a break iterator from pre-compiled rules
     * is much faster than building one from the source form of the
     * break rules.
     *
     * The binary data can only be used with the same version of ICU
     *  and on the same platform type (processor endian-ness)
     *
     * @param length Returns the length of the binary data.  (Out paramter.)
     *
     * @return   A pointer to the binary (compiled) rule data.  The storage
     *           belongs to the RulesBasedBreakIterator object, not the
     *           caller, and must not be modified or deleted.
     * @stable ICU 4.8
     */
    virtual const uint8_t *getBinaryRules(uint32_t &length);

    /**
     *  Set the subject text string upon which the break iterator is operating
     *  without changing any other aspect of the matching state.
     *  The new and previous text strings must have the same content.
     *
     *  This function is intended for use in environments where ICU is operating on
     *  strings that may move around in memory.  It provides a mechanism for notifying
     *  ICU that the string has been relocated, and providing a new UText to access the
     *  string in its new position.
     *
     *  Note that the break iterator implementation never copies the underlying text
     *  of a string being processed, but always operates directly on the original text
     *  provided by the user. Refreshing simply drops the references to the old text
     *  and replaces them with references to the new.
     *
     *  Caution:  this function is normally used only by very specialized,
     *  system-level code.  One example use case is with garbage collection that moves
     *  the text in memory.
     *
     * @param input      The new (moved) text string.
     * @param status     Receives errors detected by this function.
     * @return           *this
     *
     * @stable ICU 49
     */
    virtual RuleBasedBreakIterator &refreshInputText(UText *input, UErrorCode &status);


protected:
    //=======================================================================
    // implementation
    //=======================================================================
    /**
     * Dumps caches and performs other actions associated with a complete change
     * in text or iteration position.
     * @internal
     */
    virtual void reset(void);

#if 0
    /**
      * Return true if the category lookup for this char
      * indicates that it is in the set of dictionary lookup chars.
      * This function is intended for use by dictionary based break iterators.
      * @return true if the category lookup for this char
      * indicates that it is in the set of dictionary lookup chars.
      * @internal
      */
    virtual UBool isDictionaryChar(UChar32);

    /**
      * Get the type of the break iterator.
      * @internal
      */
    virtual int32_t getBreakType() const;
#endif

    /**
      * Set the type of the break iterator.
      * @internal
      */
    virtual void setBreakType(int32_t type);

#ifndef U_HIDE_INTERNAL_API
    /**
      * Common initialization function, used by constructors and bufferClone.
      * @internal
      */
    void init();
#endif  /* U_HIDE_INTERNAL_API */

private:

    /**
     * This method backs the iterator back up to a "safe position" in the text.
     * This is a position that we know, without any context, must be a break position.
     * The various calling methods then iterate forward from this safe position to
     * the appropriate position to return.  (For more information, see the description
     * of buildBackwardsStateTable() in RuleBasedBreakIterator.Builder.)
     * @param statetable state table used of moving backwards
     * @internal
     */
    int32_t handlePrevious(const RBBIStateTable *statetable);

    /**
     * This method is the actual implementation of the next() method.  All iteration
     * vectors through here.  This method initializes the state machine to state 1
     * and advances through the text character by character until we reach the end
     * of the text or the state machine transitions to state 0.  We update our return
     * value every time the state machine passes through a possible end state.
     * @param statetable state table used of moving forwards
     * @internal
     */
    int32_t handleNext(const RBBIStateTable *statetable);

protected:

#ifndef U_HIDE_INTERNAL_API
    /**
     * This is the function that actually implements dictionary-based
     * breaking.  Covering at least the range from startPos to endPos,
     * it checks for dictionary characters, and if it finds them determines
     * the appropriate object to deal with them. It may cache found breaks in
     * fCachedBreakPositions as it goes. It may well also look at text outside
     * the range startPos to endPos.
     * If going forward, endPos is the normal Unicode break result, and
     * if goind in reverse, startPos is the normal Unicode break result
     * @param startPos  The start position of a range of text
     * @param endPos    The end position of a range of text
     * @param reverse   The call is for the reverse direction
     * @internal
     */
    int32_t checkDictionary(int32_t startPos, int32_t endPos, UBool reverse);
#endif  /* U_HIDE_INTERNAL_API */

private:

    /**
     * This function returns the appropriate LanguageBreakEngine for a
     * given character c.
     * @param c         A character in the dictionary set
     * @internal
     */
    const LanguageBreakEngine *getLanguageBreakEngine(UChar32 c);

    /**
     *  @internal
     */
    void makeRuleStatusValid();

};

//------------------------------------------------------------------------------
//
//   Inline Functions Definitions ...
//
//------------------------------------------------------------------------------

inline UBool RuleBasedBreakIterator::operator!=(const BreakIterator& that) const {
    return !operator==(that);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

#endif
