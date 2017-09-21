// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// edits.h
// created: 2016dec30 Markus W. Scherer

#ifndef __EDITS_H__
#define __EDITS_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

/**
 * \file
 * \brief C++ API: C++ class Edits for low-level string transformations on styled text.
 */

U_NAMESPACE_BEGIN

#ifndef U_HIDE_DRAFT_API

/**
 * Records lengths of string edits but not replacement text.
 * Supports replacements, insertions, deletions in linear progression.
 * Does not support moving/reordering of text.
 *
 * An Edits object tracks a separate UErrorCode, but ICU string transformation functions
 * (e.g., case mapping functions) merge any such errors into their API's UErrorCode.
 *
 * @draft ICU 59
 */
class U_COMMON_API Edits U_FINAL : public UMemory {
public:
    /**
     * Constructs an empty object.
     * @draft ICU 59
     */
    Edits() :
            array(stackArray), capacity(STACK_CAPACITY), length(0), delta(0), numChanges(0),
            errorCode_(U_ZERO_ERROR) {}
    /**
     * Copy constructor.
     * @param other source edits
     * @draft ICU 60
     */
    Edits(const Edits &other) :
            array(stackArray), capacity(STACK_CAPACITY), length(other.length),
            delta(other.delta), numChanges(other.numChanges),
            errorCode_(other.errorCode_) {
        copyArray(other);
    }
    /**
     * Move constructor, might leave src empty.
     * This object will have the same contents that the source object had.
     * @param src source edits
     * @draft ICU 60
     */
    Edits(Edits &&src) U_NOEXCEPT :
            array(stackArray), capacity(STACK_CAPACITY), length(src.length),
            delta(src.delta), numChanges(src.numChanges),
            errorCode_(src.errorCode_) {
        moveArray(src);
    }

    /**
     * Destructor.
     * @draft ICU 59
     */
    ~Edits();

    /**
     * Assignment operator.
     * @param other source edits
     * @return *this
     * @draft ICU 60
     */
    Edits &operator=(const Edits &other);

    /**
     * Move assignment operator, might leave src empty.
     * This object will have the same contents that the source object had.
     * The behavior is undefined if *this and src are the same object.
     * @param src source edits
     * @return *this
     * @draft ICU 60
     */
    Edits &operator=(Edits &&src) U_NOEXCEPT;

    /**
     * Resets the data but may not release memory.
     * @draft ICU 59
     */
    void reset() U_NOEXCEPT;

    /**
     * Adds a record for an unchanged segment of text.
     * Normally called from inside ICU string transformation functions, not user code.
     * @draft ICU 59
     */
    void addUnchanged(int32_t unchangedLength);
    /**
     * Adds a record for a text replacement/insertion/deletion.
     * Normally called from inside ICU string transformation functions, not user code.
     * @draft ICU 59
     */
    void addReplace(int32_t oldLength, int32_t newLength);
    /**
     * Sets the UErrorCode if an error occurred while recording edits.
     * Preserves older error codes in the outErrorCode.
     * Normally called from inside ICU string transformation functions, not user code.
     * @param outErrorCode Set to an error code if it does not contain one already
     *                  and an error occurred while recording edits.
     *                  Otherwise unchanged.
     * @return TRUE if U_FAILURE(outErrorCode)
     * @draft ICU 59
     */
    UBool copyErrorTo(UErrorCode &outErrorCode);

    /**
     * How much longer is the new text compared with the old text?
     * @return new length minus old length
     * @draft ICU 59
     */
    int32_t lengthDelta() const { return delta; }
    /**
     * @return TRUE if there are any change edits
     * @draft ICU 59
     */
    UBool hasChanges() const { return numChanges != 0; }

    /**
     * @return the number of change edits
     * @draft ICU 60
     */
    int32_t numberOfChanges() const { return numChanges; }

    /**
     * Access to the list of edits.
     * @see getCoarseIterator
     * @see getFineIterator
     * @draft ICU 59
     */
    struct U_COMMON_API Iterator U_FINAL : public UMemory {
        /**
         * Default constructor, empty iterator.
         * @draft ICU 60
         */
        Iterator() :
                array(nullptr), index(0), length(0),
                remaining(0), onlyChanges_(FALSE), coarse(FALSE),
                dir(0), changed(FALSE), oldLength_(0), newLength_(0),
                srcIndex(0), replIndex(0), destIndex(0) {}
        /**
         * Copy constructor.
         * @draft ICU 59
         */
        Iterator(const Iterator &other) = default;
        /**
         * Assignment operator.
         * @draft ICU 59
         */
        Iterator &operator=(const Iterator &other) = default;

        /**
         * Advances to the next edit.
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return TRUE if there is another edit
         * @draft ICU 59
         */
        UBool next(UErrorCode &errorCode) { return next(onlyChanges_, errorCode); }

        /**
         * Finds the edit that contains the source index.
         * The source index may be found in a non-change
         * even if normal iteration would skip non-changes.
         * Normal iteration can continue from a found edit.
         *
         * The iterator state before this search logically does not matter.
         * (It may affect the performance of the search.)
         *
         * The iterator state after this search is undefined
         * if the source index is out of bounds for the source string.
         *
         * @param i source index
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return TRUE if the edit for the source index was found
         * @draft ICU 59
         */
        UBool findSourceIndex(int32_t i, UErrorCode &errorCode) {
            return findIndex(i, TRUE, errorCode) == 0;
        }

        /**
         * Finds the edit that contains the destination index.
         * The destination index may be found in a non-change
         * even if normal iteration would skip non-changes.
         * Normal iteration can continue from a found edit.
         *
         * The iterator state before this search logically does not matter.
         * (It may affect the performance of the search.)
         *
         * The iterator state after this search is undefined
         * if the source index is out of bounds for the source string.
         *
         * @param i destination index
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return TRUE if the edit for the destination index was found
         * @draft ICU 60
         */
        UBool findDestinationIndex(int32_t i, UErrorCode &errorCode) {
            return findIndex(i, FALSE, errorCode) == 0;
        }

        /**
         * Returns the destination index corresponding to the given source index.
         * If the source index is inside a change edit (not at its start),
         * then the destination index at the end of that edit is returned,
         * since there is no information about index mapping inside a change edit.
         *
         * (This means that indexes to the start and middle of an edit,
         * for example around a grapheme cluster, are mapped to indexes
         * encompassing the entire edit.
         * The alternative, mapping an interior index to the start,
         * would map such an interval to an empty one.)
         *
         * This operation will usually but not always modify this object.
         * The iterator state after this search is undefined.
         *
         * @param i source index
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return destination index; undefined if i is not 0..string length
         * @draft ICU 60
         */
        int32_t destinationIndexFromSourceIndex(int32_t i, UErrorCode &errorCode);

        /**
         * Returns the source index corresponding to the given destination index.
         * If the destination index is inside a change edit (not at its start),
         * then the source index at the end of that edit is returned,
         * since there is no information about index mapping inside a change edit.
         *
         * (This means that indexes to the start and middle of an edit,
         * for example around a grapheme cluster, are mapped to indexes
         * encompassing the entire edit.
         * The alternative, mapping an interior index to the start,
         * would map such an interval to an empty one.)
         *
         * This operation will usually but not always modify this object.
         * The iterator state after this search is undefined.
         *
         * @param i destination index
         * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
         *                  or else the function returns immediately. Check for U_FAILURE()
         *                  on output or use with function chaining. (See User Guide for details.)
         * @return source index; undefined if i is not 0..string length
         * @draft ICU 60
         */
        int32_t sourceIndexFromDestinationIndex(int32_t i, UErrorCode &errorCode);

        /**
         * @return TRUE if this edit replaces oldLength() units with newLength() different ones.
         *         FALSE if oldLength units remain unchanged.
         * @draft ICU 59
         */
        UBool hasChange() const { return changed; }
        /**
         * @return the number of units in the original string which are replaced or remain unchanged.
         * @draft ICU 59
         */
        int32_t oldLength() const { return oldLength_; }
        /**
         * @return the number of units in the modified string, if hasChange() is TRUE.
         *         Same as oldLength if hasChange() is FALSE.
         * @draft ICU 59
         */
        int32_t newLength() const { return newLength_; }

        /**
         * @return the current index into the source string
         * @draft ICU 59
         */
        int32_t sourceIndex() const { return srcIndex; }
        /**
         * @return the current index into the replacement-characters-only string,
         *         not counting unchanged spans
         * @draft ICU 59
         */
        int32_t replacementIndex() const { return replIndex; }
        /**
         * @return the current index into the full destination string
         * @draft ICU 59
         */
        int32_t destinationIndex() const { return destIndex; }

    private:
        friend class Edits;

        Iterator(const uint16_t *a, int32_t len, UBool oc, UBool crs);

        int32_t readLength(int32_t head);
        void updateNextIndexes();
        void updatePreviousIndexes();
        UBool noNext();
        UBool next(UBool onlyChanges, UErrorCode &errorCode);
        UBool previous(UErrorCode &errorCode);
        /** @return -1: error or i<0; 0: found; 1: i>=string length */
        int32_t findIndex(int32_t i, UBool findSource, UErrorCode &errorCode);

        const uint16_t *array;
        int32_t index, length;
        // 0 if we are not within compressed equal-length changes.
        // Otherwise the number of remaining changes, including the current one.
        int32_t remaining;
        UBool onlyChanges_, coarse;

        int8_t dir;  // iteration direction: back(<0), initial(0), forward(>0)
        UBool changed;
        int32_t oldLength_, newLength_;
        int32_t srcIndex, replIndex, destIndex;
    };

    /**
     * Returns an Iterator for coarse-grained changes for simple string updates.
     * Skips non-changes.
     * @return an Iterator that merges adjacent changes.
     * @draft ICU 59
     */
    Iterator getCoarseChangesIterator() const {
        return Iterator(array, length, TRUE, TRUE);
    }

    /**
     * Returns an Iterator for coarse-grained changes and non-changes for simple string updates.
     * @return an Iterator that merges adjacent changes.
     * @draft ICU 59
     */
    Iterator getCoarseIterator() const {
        return Iterator(array, length, FALSE, TRUE);
    }

    /**
     * Returns an Iterator for fine-grained changes for modifying styled text.
     * Skips non-changes.
     * @return an Iterator that separates adjacent changes.
     * @draft ICU 59
     */
    Iterator getFineChangesIterator() const {
        return Iterator(array, length, TRUE, FALSE);
    }

    /**
     * Returns an Iterator for fine-grained changes and non-changes for modifying styled text.
     * @return an Iterator that separates adjacent changes.
     * @draft ICU 59
     */
    Iterator getFineIterator() const {
        return Iterator(array, length, FALSE, FALSE);
    }

    /**
     * Merges the two input Edits and appends the result to this object.
     *
     * Consider two string transformations (for example, normalization and case mapping)
     * where each records Edits in addition to writing an output string.<br>
     * Edits ab reflect how substrings of input string a
     * map to substrings of intermediate string b.<br>
     * Edits bc reflect how substrings of intermediate string b
     * map to substrings of output string c.<br>
     * This function merges ab and bc such that the additional edits
     * recorded in this object reflect how substrings of input string a
     * map to substrings of output string c.
     *
     * If unrelated Edits are passed in where the output string of the first
     * has a different length than the input string of the second,
     * then a U_ILLEGAL_ARGUMENT_ERROR is reported.
     *
     * @param ab reflects how substrings of input string a
     *     map to substrings of intermediate string b.
     * @param bc reflects how substrings of intermediate string b
     *     map to substrings of output string c.
     * @param errorCode ICU error code. Its input value must pass the U_SUCCESS() test,
     *                  or else the function returns immediately. Check for U_FAILURE()
     *                  on output or use with function chaining. (See User Guide for details.)
     * @return *this, with the merged edits appended
     * @draft ICU 60
     */
    Edits &mergeAndAppend(const Edits &ab, const Edits &bc, UErrorCode &errorCode);

private:
    void releaseArray() U_NOEXCEPT;
    Edits &copyArray(const Edits &other);
    Edits &moveArray(Edits &src) U_NOEXCEPT;

    void setLastUnit(int32_t last) { array[length - 1] = (uint16_t)last; }
    int32_t lastUnit() const { return length > 0 ? array[length - 1] : 0xffff; }

    void append(int32_t r);
    UBool growArray();

    static const int32_t STACK_CAPACITY = 100;
    uint16_t *array;
    int32_t capacity;
    int32_t length;
    int32_t delta;
    int32_t numChanges;
    UErrorCode errorCode_;
    uint16_t stackArray[STACK_CAPACITY];
};

#endif  // U_HIDE_DRAFT_API

U_NAMESPACE_END

#endif  // __EDITS_H__
