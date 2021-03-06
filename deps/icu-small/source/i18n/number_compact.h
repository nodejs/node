// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_COMPACT_H__
#define __NUMBER_COMPACT_H__

#include "standardplural.h"
#include "number_types.h"
#include "unicode/unum.h"
#include "uvector.h"
#include "resource.h"
#include "number_patternmodifier.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

static const int32_t COMPACT_MAX_DIGITS = 15;

class CompactData : public MultiplierProducer {
  public:
    CompactData();

    void populate(const Locale &locale, const char *nsName, CompactStyle compactStyle,
                  CompactType compactType, UErrorCode &status);

    int32_t getMultiplier(int32_t magnitude) const U_OVERRIDE;

    const UChar *getPattern(int32_t magnitude, StandardPlural::Form plural) const;

    void getUniquePatterns(UVector &output, UErrorCode &status) const;

  private:
    const UChar *patterns[(COMPACT_MAX_DIGITS + 1) * StandardPlural::COUNT];
    int8_t multipliers[COMPACT_MAX_DIGITS + 1];
    int8_t largestMagnitude;
    UBool isEmpty;

    class CompactDataSink : public ResourceSink {
      public:
        explicit CompactDataSink(CompactData &data) : data(data) {}

        void put(const char *key, ResourceValue &value, UBool /*noFallback*/, UErrorCode &status) U_OVERRIDE;

      private:
        CompactData &data;
    };
};

struct CompactModInfo {
    const ImmutablePatternModifier *mod;
    const UChar* patternString;
};

class CompactHandler : public MicroPropsGenerator, public UMemory {
  public:
    CompactHandler(
            CompactStyle compactStyle,
            const Locale &locale,
            const char *nsName,
            CompactType compactType,
            const PluralRules *rules,
            MutablePatternModifier *buildReference,
            bool safe,
            const MicroPropsGenerator *parent,
            UErrorCode &status);

    ~CompactHandler() U_OVERRIDE;

    void
    processQuantity(DecimalQuantity &quantity, MicroProps &micros, UErrorCode &status) const U_OVERRIDE;

  private:
    const PluralRules *rules;
    const MicroPropsGenerator *parent;
    // Initial capacity of 12 for 0K, 00K, 000K, ...M, ...B, and ...T
    MaybeStackArray<CompactModInfo, 12> precomputedMods;
    int32_t precomputedModsLength = 0;
    CompactData data;
    ParsedPatternInfo unsafePatternInfo;
    MutablePatternModifier* unsafePatternModifier;
    UBool safe;

    /** Used by the safe code path */
    void precomputeAllModifiers(MutablePatternModifier &buildReference, UErrorCode &status);
};


} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__NUMBER_COMPACT_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
