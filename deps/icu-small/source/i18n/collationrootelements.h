// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationrootelements.h
*
* created on: 2013mar01
* created by: Markus W. Scherer
*/

#ifndef __COLLATIONROOTELEMENTS_H__
#define __COLLATIONROOTELEMENTS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/uobject.h"
#include "collation.h"

U_NAMESPACE_BEGIN

/**
 * Container and access methods for collation elements and weights
 * that occur in the root collator.
 * Needed for finding boundaries for building a tailoring.
 *
 * This class takes and returns 16-bit secondary and tertiary weights.
 */
class U_I18N_API CollationRootElements : public UMemory {
public:
    CollationRootElements(const uint32_t *rootElements, int32_t rootElementsLength)
            : elements(rootElements), length(rootElementsLength) {}

    /**
     * Higher than any root primary.
     */
    static const uint32_t PRIMARY_SENTINEL = 0xffffff00;

    /**
     * Flag in a root element, set if the element contains secondary & tertiary weights,
     * rather than a primary.
     */
    static const uint32_t SEC_TER_DELTA_FLAG = 0x80;
    /**
     * Mask for getting the primary range step value from a primary-range-end element.
     */
    static const uint8_t PRIMARY_STEP_MASK = 0x7f;

    enum {
        /**
         * Index of the first CE with a non-zero tertiary weight.
         * Same as the start of the compact root elements table.
         */
        IX_FIRST_TERTIARY_INDEX,
        /**
         * Index of the first CE with a non-zero secondary weight.
         */
        IX_FIRST_SECONDARY_INDEX,
        /**
         * Index of the first CE with a non-zero primary weight.
         */
        IX_FIRST_PRIMARY_INDEX,
        /**
         * Must match Collation::COMMON_SEC_AND_TER_CE.
         */
        IX_COMMON_SEC_AND_TER_CE,
        /**
         * Secondary & tertiary boundaries.
         * Bits 31..24: [fixed last secondary common byte 45]
         * Bits 23..16: [fixed first ignorable secondary byte 80]
         * Bits 15.. 8: reserved, 0
         * Bits  7.. 0: [fixed first ignorable tertiary byte 3C]
         */
        IX_SEC_TER_BOUNDARIES,
        /**
         * The current number of indexes.
         * Currently the same as elements[IX_FIRST_TERTIARY_INDEX].
         */
        IX_COUNT
    };

    /**
     * Returns the boundary between tertiary weights of primary/secondary CEs
     * and those of tertiary CEs.
     * This is the upper limit for tertiaries of primary/secondary CEs.
     * This minus one is the lower limit for tertiaries of tertiary CEs.
     */
    uint32_t getTertiaryBoundary() const {
        return (elements[IX_SEC_TER_BOUNDARIES] << 8) & 0xff00;
    }

    /**
     * Returns the first assigned tertiary CE.
     */
    uint32_t getFirstTertiaryCE() const {
        return elements[elements[IX_FIRST_TERTIARY_INDEX]] & ~SEC_TER_DELTA_FLAG;
    }

    /**
     * Returns the last assigned tertiary CE.
     */
    uint32_t getLastTertiaryCE() const {
        return elements[elements[IX_FIRST_SECONDARY_INDEX] - 1] & ~SEC_TER_DELTA_FLAG;
    }

    /**
     * Returns the last common secondary weight.
     * This is the lower limit for secondaries of primary CEs.
     */
    uint32_t getLastCommonSecondary() const {
        return (elements[IX_SEC_TER_BOUNDARIES] >> 16) & 0xff00;
    }

    /**
     * Returns the boundary between secondary weights of primary CEs
     * and those of secondary CEs.
     * This is the upper limit for secondaries of primary CEs.
     * This minus one is the lower limit for secondaries of secondary CEs.
     */
    uint32_t getSecondaryBoundary() const {
        return (elements[IX_SEC_TER_BOUNDARIES] >> 8) & 0xff00;
    }

    /**
     * Returns the first assigned secondary CE.
     */
    uint32_t getFirstSecondaryCE() const {
        return elements[elements[IX_FIRST_SECONDARY_INDEX]] & ~SEC_TER_DELTA_FLAG;
    }

    /**
     * Returns the last assigned secondary CE.
     */
    uint32_t getLastSecondaryCE() const {
        return elements[elements[IX_FIRST_PRIMARY_INDEX] - 1] & ~SEC_TER_DELTA_FLAG;
    }

    /**
     * Returns the first assigned primary weight.
     */
    uint32_t getFirstPrimary() const {
        return elements[elements[IX_FIRST_PRIMARY_INDEX]];  // step=0: cannot be a range end
    }

    /**
     * Returns the first assigned primary CE.
     */
    int64_t getFirstPrimaryCE() const {
        return Collation::makeCE(getFirstPrimary());
    }

    /**
     * Returns the last root CE with a primary weight before p.
     * Intended only for reordering group boundaries.
     */
    int64_t lastCEWithPrimaryBefore(uint32_t p) const;

    /**
     * Returns the first root CE with a primary weight of at least p.
     * Intended only for reordering group boundaries.
     */
    int64_t firstCEWithPrimaryAtLeast(uint32_t p) const;

    /**
     * Returns the primary weight before p.
     * p must be greater than the first root primary.
     */
    uint32_t getPrimaryBefore(uint32_t p, UBool isCompressible) const;

    /** Returns the secondary weight before [p, s]. */
    uint32_t getSecondaryBefore(uint32_t p, uint32_t s) const;

    /** Returns the tertiary weight before [p, s, t]. */
    uint32_t getTertiaryBefore(uint32_t p, uint32_t s, uint32_t t) const;

    /**
     * Finds the index of the input primary.
     * p must occur as a root primary, and must not be 0.
     */
    int32_t findPrimary(uint32_t p) const;

    /**
     * Returns the primary weight after p where index=findPrimary(p).
     * p must be at least the first root primary.
     */
    uint32_t getPrimaryAfter(uint32_t p, int32_t index, UBool isCompressible) const;
    /**
     * Returns the secondary weight after [p, s] where index=findPrimary(p)
     * except use index=0 for p=0.
     *
     * Must return a weight for every root [p, s] as well as for every weight
     * returned by getSecondaryBefore(). If p!=0 then s can be BEFORE_WEIGHT16.
     *
     * Exception: [0, 0] is handled by the CollationBuilder:
     * Both its lower and upper boundaries are special.
     */
    uint32_t getSecondaryAfter(int32_t index, uint32_t s) const;
    /**
     * Returns the tertiary weight after [p, s, t] where index=findPrimary(p)
     * except use index=0 for p=0.
     *
     * Must return a weight for every root [p, s, t] as well as for every weight
     * returned by getTertiaryBefore(). If s!=0 then t can be BEFORE_WEIGHT16.
     *
     * Exception: [0, 0, 0] is handled by the CollationBuilder:
     * Both its lower and upper boundaries are special.
     */
    uint32_t getTertiaryAfter(int32_t index, uint32_t s, uint32_t t) const;

private:
    /**
     * Returns the first secondary & tertiary weights for p where index=findPrimary(p)+1.
     */
    uint32_t getFirstSecTerForPrimary(int32_t index) const;

    /**
     * Finds the largest index i where elements[i]<=p.
     * Requires first primary<=p<0xffffff00 (PRIMARY_SENTINEL).
     * Does not require that p is a root collator primary.
     */
    int32_t findP(uint32_t p) const;

    static inline UBool isEndOfPrimaryRange(uint32_t q) {
        return (q & SEC_TER_DELTA_FLAG) == 0 && (q & PRIMARY_STEP_MASK) != 0;
    }

    /**
     * Data structure:
     *
     * The first few entries are indexes, up to elements[IX_FIRST_TERTIARY_INDEX].
     * See the comments on the IX_ constants.
     *
     * All other elements are a compact form of the root collator CEs
     * in mostly collation order.
     *
     * A sequence of one or more root CEs with the same primary weight is stored as
     * one element with the primary weight, with the SEC_TER_DELTA_FLAG flag not set,
     * followed by elements with only the secondary/tertiary weights,
     * each with that flag set.
     * If the lowest secondary/tertiary combination is Collation::COMMON_SEC_AND_TER_CE,
     * then the element for that combination is omitted.
     *
     * Note: If the first actual secondary/tertiary combination is higher than
     * Collation::COMMON_SEC_AND_TER_CE (which is unusual),
     * the runtime code will assume anyway that Collation::COMMON_SEC_AND_TER_CE is present.
     *
     * A range of only-primary CEs with a consistent "step" increment
     * from each primary to the next may be stored as a range.
     * Only the first and last primary are stored, and the last has the step
     * value in the low bits (PRIMARY_STEP_MASK).
     *
     * An range-end element may also either start a new range or be followed by
     * elements with secondary/tertiary deltas.
     *
     * A primary element that is not a range end has zero step bits.
     *
     * There is no element for the completely ignorable CE (all weights 0).
     *
     * Before elements[IX_FIRST_PRIMARY_INDEX], all elements are secondary/tertiary deltas,
     * for all of the ignorable root CEs.
     *
     * There are no elements for unassigned-implicit primary CEs.
     * All primaries stored here are at most 3 bytes long.
     */
    const uint32_t *elements;
    int32_t length;
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
#endif  // __COLLATIONROOTELEMENTS_H__
