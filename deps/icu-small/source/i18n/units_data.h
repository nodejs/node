// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __UNITS_DATA_H__
#define __UNITS_DATA_H__

#include <limits>

#include "charstr.h"
#include "cmemory.h"
#include "fixedstring.h"
#include "unicode/stringpiece.h"
#include "unicode/uobject.h"

U_NAMESPACE_BEGIN
namespace units {

/**
 * Encapsulates "convertUnits" information from units resources, specifying how
 * to convert from one unit to another.
 *
 * Information in this class is still in the form of strings: symbolic constants
 * need to be interpreted. Rationale: symbols can cancel out for higher
 * precision conversion - going from feet to inches should cancel out the
 * `ft_to_m` constant.
 */
class ConversionRateInfo : public UMemory {
  public:
    ConversionRateInfo() {}
    ConversionRateInfo(StringPiece sourceUnit, StringPiece baseUnit, StringPiece factor,
                       StringPiece offset, UErrorCode &status)
        : sourceUnit(sourceUnit), baseUnit(baseUnit), factor(factor), offset(offset),
          specialMappingName(), systems() {
        if (this->sourceUnit.isEmpty() != sourceUnit.empty() ||
            this->baseUnit.isEmpty() != baseUnit.empty() ||
            this->factor.isEmpty() != factor.empty() ||
            this->offset.isEmpty() != offset.empty()) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    FixedString sourceUnit;
    FixedString baseUnit;
    FixedString factor;
    FixedString offset;
    FixedString specialMappingName; // the name of a special mapping used instead of factor + optional offset.
    FixedString systems;
};

/**
 * Returns ConversionRateInfo for all supported conversions.
 *
 * @param result Receives the set of conversion rates.
 * @param status Receives status.
 */
void U_I18N_API getAllConversionRates(MaybeStackVector<ConversionRateInfo> &result, UErrorCode &status);

/**
 * Contains all the supported conversion rates.
 */
class ConversionRates {
  public:
    /**
     * Constructor
     *
     * @param status Receives status.
     */
    ConversionRates(UErrorCode &status) { getAllConversionRates(conversionInfo_, status); }

    /**
     * Returns a pointer to the conversion rate info that match the `source`.
     *
     * @param source Contains the source.
     * @param status Receives status.
     */
    const ConversionRateInfo *extractConversionInfo(StringPiece source, UErrorCode &status) const;

  private:
    MaybeStackVector<ConversionRateInfo> conversionInfo_;
};

// Encapsulates unitPreferenceData information from units resources, specifying
// a sequence of output unit preferences.
struct UnitPreference : public UMemory {
    // Set geq to 1.0 by default
    UnitPreference() : geq(1.0) {}
    FixedString unit;
    double geq;
    UnicodeString skeleton;

    UnitPreference(const UnitPreference &other) {
        this->unit = other.unit;
        this->geq = other.geq;
        this->skeleton = other.skeleton;
    }
};

/**
 * Metadata about the preferences in UnitPreferences::unitPrefs_.
 *
 * This class owns all of its data.
 *
 * UnitPreferenceMetadata lives in the anonymous namespace, because it should
 * only be useful to internal code and unit testing code.
 */
class UnitPreferenceMetadata : public UMemory {
  public:
    UnitPreferenceMetadata() {}
    // Constructor, makes copies of the parameters passed to it.
    UnitPreferenceMetadata(StringPiece category, StringPiece usage, StringPiece region,
                           int32_t prefsOffset, int32_t prefsCount, UErrorCode &status);

    // Unit category (e.g. "length", "mass", "electric-capacitance").
    CharString category;
    // Usage (e.g. "road", "vehicle-fuel", "blood-glucose"). Every category
    // should have an entry for "default" usage. TODO(hugovdm): add a test for
    // this.
    CharString usage;
    // Region code (e.g. "US", "CZ", "001"). Every usage should have an entry
    // for the "001" region ("world"). TODO(hugovdm): add a test for this.
    CharString region;
    // Offset into the UnitPreferences::unitPrefs_ list where the relevant
    // preferences are found.
    int32_t prefsOffset;
    // The number of preferences that form this set.
    int32_t prefsCount;

    int32_t compareTo(const UnitPreferenceMetadata &other) const;
    int32_t compareTo(const UnitPreferenceMetadata &other, bool *foundCategory, bool *foundUsage,
                      bool *foundRegion) const;
};

/**
 * Unit Preferences information for various locales and usages.
 */
class U_I18N_API_CLASS UnitPreferences {
  public:
    /**
     * Constructor, loads all the preference data.
     *
     * @param status Receives status.
     */
    U_I18N_API UnitPreferences(UErrorCode& status);

    /**
     * Returns the set of unit preferences in the particular category that best
     * matches the specified usage and region.
     *
     * If region can't be found, falls back to global (001). If usage can't be
     * found, falls back to "default".
     *
     * @param category The category within which to look up usage and region.
     * (TODO(hugovdm): improve docs on how to find the category, once the lookup
     * function is added.)
     * @param usage The usage parameter. (TODO(hugovdm): improve this
     * documentation. Add reference to some list of usages we support.) If the
     * given usage is not found, the method automatically falls back to
     * "default".
     * @param region The region whose preferences are desired. If there are no
     * specific preferences for the requested region, the method automatically
     * falls back to region "001" ("world").
     * @param outPreferences A pointer into an array of preferences: essentially
     * an array slice in combination with preferenceCount.
     * @param preferenceCount The number of unit preferences that belong to the
     * result set.
     * @param status Receives status.
     */
    U_I18N_API MaybeStackVector<UnitPreference> getPreferencesFor(StringPiece category,
                                                                  StringPiece usage,
                                                                  const Locale& locale,
                                                                  UErrorCode& status) const;

  protected:
    // Metadata about the sets of preferences, this is the index for looking up
    // preferences in the unitPrefs_ list.
    MaybeStackVector<UnitPreferenceMetadata> metadata_;
    // All the preferences as a flat list: which usage and region preferences
    // are associated with is stored in `metadata_`.
    MaybeStackVector<UnitPreference> unitPrefs_;
};

} // namespace units
U_NAMESPACE_END

#endif //__UNITS_DATA_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
