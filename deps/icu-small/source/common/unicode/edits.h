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
            array(stackArray), capacity(STACK_CAPACITY), length(0), delta(0),
            errorCode(U_ZERO_ERROR) {}
    /**
     * Destructor.
     * @draft ICU 59
     */
    ~Edits();

    /**
     * Resets the data but may not release memory.
     * @draft ICU 59
     */
    void reset();

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
    UBool hasChanges() const;

    /**
     * Access to the list of edits.
     * @see getCoarseIterator
     * @see getFineIterator
     * @draft ICU 59
     */
    struct U_COMMON_API Iterator U_FINAL : public UMemory {
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
         * @return TRUE if the edit for the source index was found
         * @draft ICU 59
         */
        UBool findSourceIndex(int32_t i, UErrorCode &errorCode);

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
        void updateIndexes();
        UBool noNext();
        UBool next(UBool onlyChanges, UErrorCode &errorCode);

        const uint16_t *array;
        int32_t index, length;
        int32_t remaining;
        UBool onlyChanges_, coarse;

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

private:
    Edits(const Edits &) = delete;
    Edits &operator=(const Edits &) = delete;

    void setLastUnit(int32_t last) { array[length - 1] = (uint16_t)last; }
    int32_t lastUnit() const { return length > 0 ? array[length - 1] : 0xffff; }

    void append(int32_t r);
    UBool growArray();

    static const int32_t STACK_CAPACITY = 100;
    uint16_t *array;
    int32_t capacity;
    int32_t length;
    int32_t delta;
    UErrorCode errorCode;
    uint16_t stackArray[STACK_CAPACITY];
};

#endif  // U_HIDE_DRAFT_API

U_NAMESPACE_END

#endif  // __EDITS_H__
