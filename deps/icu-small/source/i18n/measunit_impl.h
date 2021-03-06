// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef __MEASUNIT_IMPL_H__
#define __MEASUNIT_IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/measunit.h"
#include "cmemory.h"
#include "charstr.h"

U_NAMESPACE_BEGIN


static const char16_t kDefaultCurrency[] = u"XXX";
static const char kDefaultCurrency8[] = "XXX";


/**
 * A struct representing a single unit (optional SI prefix and dimensionality).
 */
struct U_I18N_API SingleUnitImpl : public UMemory {
    /**
     * Gets a single unit from the MeasureUnit. If there are multiple single units, sets an error
     * code and returns the base dimensionless unit. Parses if necessary.
     */
    static SingleUnitImpl forMeasureUnit(const MeasureUnit& measureUnit, UErrorCode& status);

    /** Transform this SingleUnitImpl into a MeasureUnit, simplifying if possible. */
    MeasureUnit build(UErrorCode& status) const;

    /**
     * Returns the "simple unit ID", without SI or dimensionality prefix: this
     * instance may represent a square-kilometer, but only "meter" will be
     * returned.
     *
     * The returned pointer points at memory that exists for the duration of the
     * program's running.
     */
    const char *getSimpleUnitID() const;

    /**
     * Compare this SingleUnitImpl to another SingleUnitImpl for the sake of
     * sorting and coalescing.
     *
     * Takes the sign of dimensionality into account, but not the absolute
     * value: per-meter is not considered the same as meter, but meter is
     * considered the same as square-meter.
     *
     * The dimensionless unit generally does not get compared, but if it did, it
     * would sort before other units by virtue of index being < 0 and
     * dimensionality not being negative.
     */
    int32_t compareTo(const SingleUnitImpl& other) const {
        if (dimensionality < 0 && other.dimensionality > 0) {
            // Positive dimensions first
            return 1;
        }
        if (dimensionality > 0 && other.dimensionality < 0) {
            return -1;
        }
        if (index < other.index) {
            return -1;
        }
        if (index > other.index) {
            return 1;
        }
        if (siPrefix < other.siPrefix) {
            return -1;
        }
        if (siPrefix > other.siPrefix) {
            return 1;
        }
        return 0;
    }

    /**
     * Return whether this SingleUnitImpl is compatible with another for the purpose of coalescing.
     *
     * Units with the same base unit and SI prefix should match, except that they must also have
     * the same dimensionality sign, such that we don't merge numerator and denominator.
     */
    bool isCompatibleWith(const SingleUnitImpl& other) const {
        return (compareTo(other) == 0);
    }

    /**
     * Returns true if this unit is the "dimensionless base unit", as produced
     * by the MeasureUnit() default constructor. (This does not include the
     * likes of concentrations or angles.)
     */
    bool isDimensionless() const {
        return index == -1;
    }

    /**
     * Simple unit index, unique for every simple unit, -1 for the dimensionless
     * unit. This is an index into a string list in measunit_extra.cpp.
     *
     * The default value is -1, meaning the dimensionless unit:
     * isDimensionless() will return true, until index is changed.
     */
    int32_t index = -1;

    /**
     * SI prefix.
     *
     * This is ignored for the dimensionless unit.
     */
    UMeasureSIPrefix siPrefix = UMEASURE_SI_PREFIX_ONE;

    /**
     * Dimensionality.
     *
     * This is meaningless for the dimensionless unit.
     */
    int32_t dimensionality = 1;
};

// Export explicit template instantiations of MaybeStackArray, MemoryPool and
// MaybeStackVector. This is required when building DLLs for Windows. (See
// datefmt.h, collationiterator.h, erarules.h and others for similar examples.)
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN
template class U_I18N_API MaybeStackArray<SingleUnitImpl*, 8>;
template class U_I18N_API MemoryPool<SingleUnitImpl, 8>;
template class U_I18N_API MaybeStackVector<SingleUnitImpl, 8>;
#endif

/**
 * Internal representation of measurement units. Capable of representing all complexities of units,
 * including mixed and compound units.
 */
struct U_I18N_API MeasureUnitImpl : public UMemory {
    MeasureUnitImpl() = default;
    MeasureUnitImpl(MeasureUnitImpl &&other) = default;
    MeasureUnitImpl(const MeasureUnitImpl &other, UErrorCode &status);
    MeasureUnitImpl(const SingleUnitImpl &singleUnit, UErrorCode &status);

    MeasureUnitImpl &operator=(MeasureUnitImpl &&other) noexcept = default;

    /** Extract the MeasureUnitImpl from a MeasureUnit. */
    static inline const MeasureUnitImpl* get(const MeasureUnit& measureUnit) {
        return measureUnit.fImpl;
    }

    /**
     * Parse a unit identifier into a MeasureUnitImpl.
     *
     * @param identifier The unit identifier string.
     * @param status Set if the identifier string is not valid.
     * @return A newly parsed value object. Behaviour of this unit is
     * unspecified if an error is returned via status.
     */
    static MeasureUnitImpl forIdentifier(StringPiece identifier, UErrorCode& status);

    /**
     * Extract the MeasureUnitImpl from a MeasureUnit, or parse if it is not present.
     *
     * @param measureUnit The source MeasureUnit.
     * @param memory A place to write the new MeasureUnitImpl if parsing is required.
     * @param status Set if an error occurs.
     * @return A reference to either measureUnit.fImpl or memory.
     */
    static const MeasureUnitImpl& forMeasureUnit(
        const MeasureUnit& measureUnit, MeasureUnitImpl& memory, UErrorCode& status);

    /**
     * Extract the MeasureUnitImpl from a MeasureUnit, or parse if it is not present.
     *
     * @param measureUnit The source MeasureUnit.
     * @param status Set if an error occurs.
     * @return A value object, either newly parsed or copied from measureUnit.
     */
    static MeasureUnitImpl forMeasureUnitMaybeCopy(
        const MeasureUnit& measureUnit, UErrorCode& status);

    /**
     * Used for currency units.
     */
    static inline MeasureUnitImpl forCurrencyCode(StringPiece currencyCode) {
        MeasureUnitImpl result;
        UErrorCode localStatus = U_ZERO_ERROR;
        result.identifier.append(currencyCode, localStatus);
        // localStatus is not expected to fail since currencyCode should be 3 chars long
        return result;
    }

    /** Transform this MeasureUnitImpl into a MeasureUnit, simplifying if possible. */
    MeasureUnit build(UErrorCode& status) &&;

    /**
     * Create a copy of this MeasureUnitImpl. Don't use copy constructor to make this explicit.
     */
    MeasureUnitImpl copy(UErrorCode& status) const;

    /**
     * Extracts the list of all the individual units inside the `MeasureUnitImpl`.
     *      For example:
     *          -   if the `MeasureUnitImpl` is `foot-per-hour`
     *                  it will return a list of 1 {`foot-per-hour`}
     *          -   if the `MeasureUnitImpl` is `foot-and-inch`
     *                  it will return a list of 2 { `foot`, `inch`}
     */
    MaybeStackVector<MeasureUnitImpl> extractIndividualUnits(UErrorCode &status) const;

    /** Mutates this MeasureUnitImpl to take the reciprocal. */
    void takeReciprocal(UErrorCode& status);

    /**
     * Mutates this MeasureUnitImpl to append a single unit.
     *
     * @return true if a new item was added. If unit is the dimensionless unit,
     * it is never added: the return value will always be false.
     */
    bool append(const SingleUnitImpl& singleUnit, UErrorCode& status);

    /** The complexity, either SINGLE, COMPOUND, or MIXED. */
    UMeasureUnitComplexity complexity = UMEASURE_UNIT_SINGLE;

    /**
     * The list of simple units. These may be summed or multiplied, based on the
     * value of the complexity field.
     *
     * The "dimensionless" unit (SingleUnitImpl default constructor) must not be
     * added to this list.
     */
    MaybeStackVector<SingleUnitImpl> units;

    /**
     * The full unit identifier.  Owned by the MeasureUnitImpl.  Empty if not computed.
     */
    CharString identifier;
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif //__MEASUNIT_IMPL_H__
