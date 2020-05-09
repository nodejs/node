// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Extra functions for MeasureUnit not needed for all clients.
// Separate .o file so that it can be removed for modularity.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include <cstdlib>
#include "cstring.h"
#include "measunit_impl.h"
#include "uarrsort.h"
#include "uassert.h"
#include "ucln_in.h"
#include "umutex.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/measunit.h"
#include "unicode/ucharstrie.h"
#include "unicode/ucharstriebuilder.h"

#include "cstr.h"

U_NAMESPACE_BEGIN


namespace {

// TODO: Propose a new error code for this?
constexpr UErrorCode kUnitIdentifierSyntaxError = U_ILLEGAL_ARGUMENT_ERROR;

// Trie value offset for SI Prefixes. This is big enough to ensure we only
// insert positive integers into the trie.
constexpr int32_t kSIPrefixOffset = 64;

// Trie value offset for compound parts, e.g. "-per-", "-", "-and-".
constexpr int32_t kCompoundPartOffset = 128;

enum CompoundPart {
    // Represents "-per-"
    COMPOUND_PART_PER = kCompoundPartOffset,
    // Represents "-"
    COMPOUND_PART_TIMES,
    // Represents "-and-"
    COMPOUND_PART_AND,
};

// Trie value offset for "per-".
constexpr int32_t kInitialCompoundPartOffset = 192;

enum InitialCompoundPart {
    // Represents "per-", the only compound part that can appear at the start of
    // an identifier.
    INITIAL_COMPOUND_PART_PER = kInitialCompoundPartOffset,
};

// Trie value offset for powers like "square-", "cubic-", "p2-" etc.
constexpr int32_t kPowerPartOffset = 256;

enum PowerPart {
    POWER_PART_P2 = kPowerPartOffset + 2,
    POWER_PART_P3,
    POWER_PART_P4,
    POWER_PART_P5,
    POWER_PART_P6,
    POWER_PART_P7,
    POWER_PART_P8,
    POWER_PART_P9,
    POWER_PART_P10,
    POWER_PART_P11,
    POWER_PART_P12,
    POWER_PART_P13,
    POWER_PART_P14,
    POWER_PART_P15,
};

// Trie value offset for simple units, e.g. "gram", "nautical-mile",
// "fluid-ounce-imperial".
constexpr int32_t kSimpleUnitOffset = 512;

const struct SIPrefixStrings {
    const char* const string;
    UMeasureSIPrefix value;
} gSIPrefixStrings[] = {
    { "yotta", UMEASURE_SI_PREFIX_YOTTA },
    { "zetta", UMEASURE_SI_PREFIX_ZETTA },
    { "exa", UMEASURE_SI_PREFIX_EXA },
    { "peta", UMEASURE_SI_PREFIX_PETA },
    { "tera", UMEASURE_SI_PREFIX_TERA },
    { "giga", UMEASURE_SI_PREFIX_GIGA },
    { "mega", UMEASURE_SI_PREFIX_MEGA },
    { "kilo", UMEASURE_SI_PREFIX_KILO },
    { "hecto", UMEASURE_SI_PREFIX_HECTO },
    { "deka", UMEASURE_SI_PREFIX_DEKA },
    { "deci", UMEASURE_SI_PREFIX_DECI },
    { "centi", UMEASURE_SI_PREFIX_CENTI },
    { "milli", UMEASURE_SI_PREFIX_MILLI },
    { "micro", UMEASURE_SI_PREFIX_MICRO },
    { "nano", UMEASURE_SI_PREFIX_NANO },
    { "pico", UMEASURE_SI_PREFIX_PICO },
    { "femto", UMEASURE_SI_PREFIX_FEMTO },
    { "atto", UMEASURE_SI_PREFIX_ATTO },
    { "zepto", UMEASURE_SI_PREFIX_ZEPTO },
    { "yocto", UMEASURE_SI_PREFIX_YOCTO },
};

// TODO(ICU-21059): Get this list from data
const char16_t* const gSimpleUnits[] = {
    u"candela",
    u"carat",
    u"gram",
    u"ounce",
    u"ounce-troy",
    u"pound",
    u"kilogram",
    u"stone",
    u"ton",
    u"metric-ton",
    u"earth-mass",
    u"solar-mass",
    u"point",
    u"inch",
    u"foot",
    u"yard",
    u"meter",
    u"fathom",
    u"furlong",
    u"mile",
    u"nautical-mile",
    u"mile-scandinavian",
    u"100-kilometer",
    u"earth-radius",
    u"solar-radius",
    u"astronomical-unit",
    u"light-year",
    u"parsec",
    u"second",
    u"minute",
    u"hour",
    u"day",
    u"day-person",
    u"week",
    u"week-person",
    u"month",
    u"month-person",
    u"year",
    u"year-person",
    u"decade",
    u"century",
    u"ampere",
    u"fahrenheit",
    u"kelvin",
    u"celsius",
    u"arc-second",
    u"arc-minute",
    u"degree",
    u"radian",
    u"revolution",
    u"item",
    u"mole",
    u"permillion",
    u"permyriad",
    u"permille",
    u"percent",
    u"karat",
    u"portion",
    u"bit",
    u"byte",
    u"dot",
    u"pixel",
    u"em",
    u"hertz",
    u"newton",
    u"pound-force",
    u"pascal",
    u"bar",
    u"atmosphere",
    u"ofhg",
    u"electronvolt",
    u"dalton",
    u"joule",
    u"calorie",
    u"british-thermal-unit",
    u"foodcalorie",
    u"therm-us",
    u"watt",
    u"horsepower",
    u"solar-luminosity",
    u"volt",
    u"ohm",
    u"dunam",
    u"acre",
    u"hectare",
    u"teaspoon",
    u"tablespoon",
    u"fluid-ounce-imperial",
    u"fluid-ounce",
    u"cup",
    u"cup-metric",
    u"pint",
    u"pint-metric",
    u"quart",
    u"liter",
    u"gallon",
    u"gallon-imperial",
    u"bushel",
    u"barrel",
    u"knot",
    u"g-force",
    u"lux",
};

icu::UInitOnce gUnitExtrasInitOnce = U_INITONCE_INITIALIZER;

char16_t* kSerializedUnitExtrasStemTrie = nullptr;

UBool U_CALLCONV cleanupUnitExtras() {
    uprv_free(kSerializedUnitExtrasStemTrie);
    kSerializedUnitExtrasStemTrie = nullptr;
    gUnitExtrasInitOnce.reset();
    return TRUE;
}

void U_CALLCONV initUnitExtras(UErrorCode& status) {
    ucln_i18n_registerCleanup(UCLN_I18N_UNIT_EXTRAS, cleanupUnitExtras);

    UCharsTrieBuilder b(status);
    if (U_FAILURE(status)) { return; }

    // Add SI prefixes
    for (const auto& siPrefixInfo : gSIPrefixStrings) {
        UnicodeString uSIPrefix(siPrefixInfo.string, -1, US_INV);
        b.add(uSIPrefix, siPrefixInfo.value + kSIPrefixOffset, status);
    }
    if (U_FAILURE(status)) { return; }

    // Add syntax parts (compound, power prefixes)
    b.add(u"-per-", COMPOUND_PART_PER, status);
    b.add(u"-", COMPOUND_PART_TIMES, status);
    b.add(u"-and-", COMPOUND_PART_AND, status);
    b.add(u"per-", INITIAL_COMPOUND_PART_PER, status);
    b.add(u"square-", POWER_PART_P2, status);
    b.add(u"cubic-", POWER_PART_P3, status);
    b.add(u"p2-", POWER_PART_P2, status);
    b.add(u"p3-", POWER_PART_P3, status);
    b.add(u"p4-", POWER_PART_P4, status);
    b.add(u"p5-", POWER_PART_P5, status);
    b.add(u"p6-", POWER_PART_P6, status);
    b.add(u"p7-", POWER_PART_P7, status);
    b.add(u"p8-", POWER_PART_P8, status);
    b.add(u"p9-", POWER_PART_P9, status);
    b.add(u"p10-", POWER_PART_P10, status);
    b.add(u"p11-", POWER_PART_P11, status);
    b.add(u"p12-", POWER_PART_P12, status);
    b.add(u"p13-", POWER_PART_P13, status);
    b.add(u"p14-", POWER_PART_P14, status);
    b.add(u"p15-", POWER_PART_P15, status);
    if (U_FAILURE(status)) { return; }

    // Add sanctioned simple units by offset
    int32_t simpleUnitOffset = kSimpleUnitOffset;
    for (auto simpleUnit : gSimpleUnits) {
        b.add(simpleUnit, simpleUnitOffset++, status);
    }

    // Build the CharsTrie
    // TODO: Use SLOW or FAST here?
    UnicodeString result;
    b.buildUnicodeString(USTRINGTRIE_BUILD_FAST, result, status);
    if (U_FAILURE(status)) { return; }

    // Copy the result into the global constant pointer
    size_t numBytes = result.length() * sizeof(char16_t);
    kSerializedUnitExtrasStemTrie = static_cast<char16_t*>(uprv_malloc(numBytes));
    uprv_memcpy(kSerializedUnitExtrasStemTrie, result.getBuffer(), numBytes);
}

class Token {
public:
    Token(int32_t match) : fMatch(match) {}

    enum Type {
        TYPE_UNDEFINED,
        TYPE_SI_PREFIX,
        // Token type for "-per-", "-", and "-and-".
        TYPE_COMPOUND_PART,
        // Token type for "per-".
        TYPE_INITIAL_COMPOUND_PART,
        TYPE_POWER_PART,
        TYPE_SIMPLE_UNIT,
    };

    // Calling getType() is invalid, resulting in an assertion failure, if Token
    // value isn't positive.
    Type getType() const {
        U_ASSERT(fMatch > 0);
        if (fMatch < kCompoundPartOffset) {
            return TYPE_SI_PREFIX;
        }
        if (fMatch < kInitialCompoundPartOffset) {
            return TYPE_COMPOUND_PART;
        }
        if (fMatch < kPowerPartOffset) {
            return TYPE_INITIAL_COMPOUND_PART;
        }
        if (fMatch < kSimpleUnitOffset) {
            return TYPE_POWER_PART;
        }
        return TYPE_SIMPLE_UNIT;
    }

    UMeasureSIPrefix getSIPrefix() const {
        U_ASSERT(getType() == TYPE_SI_PREFIX);
        return static_cast<UMeasureSIPrefix>(fMatch - kSIPrefixOffset);
    }

    // Valid only for tokens with type TYPE_COMPOUND_PART.
    int32_t getMatch() const {
        U_ASSERT(getType() == TYPE_COMPOUND_PART);
        return fMatch;
    }

    int32_t getInitialCompoundPart() const {
        // Even if there is only one InitialCompoundPart value, we have this
        // function for the simplicity of code consistency.
        U_ASSERT(getType() == TYPE_INITIAL_COMPOUND_PART);
        // Defensive: if this assert fails, code using this function also needs
        // to change.
        U_ASSERT(fMatch == INITIAL_COMPOUND_PART_PER);
        return fMatch;
    }

    int8_t getPower() const {
        U_ASSERT(getType() == TYPE_POWER_PART);
        return static_cast<int8_t>(fMatch - kPowerPartOffset);
    }

    int32_t getSimpleUnitIndex() const {
        U_ASSERT(getType() == TYPE_SIMPLE_UNIT);
        return fMatch - kSimpleUnitOffset;
    }

private:
    int32_t fMatch;
};

class Parser {
public:
    /**
     * Factory function for parsing the given identifier.
     *
     * @param source The identifier to parse. This function does not make a copy
     * of source: the underlying string that source points at, must outlive the
     * parser.
     * @param status ICU error code.
     */
    static Parser from(StringPiece source, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return Parser();
        }
        umtx_initOnce(gUnitExtrasInitOnce, &initUnitExtras, status);
        if (U_FAILURE(status)) {
            return Parser();
        }
        return Parser(source);
    }

    MeasureUnitImpl parse(UErrorCode& status) {
        MeasureUnitImpl result;
        parseImpl(result, status);
        return result;
    }

private:
    // Tracks parser progress: the offset into fSource.
    int32_t fIndex = 0;

    // Since we're not owning this memory, whatever is passed to the constructor
    // should live longer than this Parser - and the parser shouldn't return any
    // references to that string.
    StringPiece fSource;
    UCharsTrie fTrie;

    // Set to true when we've seen a "-per-" or a "per-", after which all units
    // are in the denominator. Until we find an "-and-", at which point the
    // identifier is invalid pending TODO(CLDR-13700).
    bool fAfterPer = false;

    Parser() : fSource(""), fTrie(u"") {}

    Parser(StringPiece source)
        : fSource(source), fTrie(kSerializedUnitExtrasStemTrie) {}

    inline bool hasNext() const {
        return fIndex < fSource.length();
    }

    // Returns the next Token parsed from fSource, advancing fIndex to the end
    // of that token in fSource. In case of U_FAILURE(status), the token
    // returned will cause an abort if getType() is called on it.
    Token nextToken(UErrorCode& status) {
        fTrie.reset();
        int32_t match = -1;
        // Saves the position in the fSource string for the end of the most
        // recent matching token.
        int32_t previ = -1;
        // Find the longest token that matches a value in the trie:
        while (fIndex < fSource.length()) {
            auto result = fTrie.next(fSource.data()[fIndex++]);
            if (result == USTRINGTRIE_NO_MATCH) {
                break;
            } else if (result == USTRINGTRIE_NO_VALUE) {
                continue;
            }
            U_ASSERT(USTRINGTRIE_HAS_VALUE(result));
            match = fTrie.getValue();
            previ = fIndex;
            if (result == USTRINGTRIE_FINAL_VALUE) {
                break;
            }
            U_ASSERT(result == USTRINGTRIE_INTERMEDIATE_VALUE);
            // continue;
        }

        if (match < 0) {
            status = kUnitIdentifierSyntaxError;
        } else {
            fIndex = previ;
        }
        return Token(match);
    }

    /**
     * Returns the next "single unit" via result.
     *
     * If a "-per-" was parsed, the result will have appropriate negative
     * dimensionality.
     *
     * Returns an error if we parse both compound units and "-and-", since mixed
     * compound units are not yet supported - TODO(CLDR-13700).
     *
     * @param result Will be overwritten by the result, if status shows success.
     * @param sawAnd If an "-and-" was parsed prior to finding the "single
     * unit", sawAnd is set to true. If not, it is left as is.
     * @param status ICU error code.
     */
    void nextSingleUnit(SingleUnitImpl& result, bool& sawAnd, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return;
        }

        // state:
        // 0 = no tokens seen yet (will accept power, SI prefix, or simple unit)
        // 1 = power token seen (will not accept another power token)
        // 2 = SI prefix token seen (will not accept a power or SI prefix token)
        int32_t state = 0;

        bool atStart = fIndex == 0;
        Token token = nextToken(status);
        if (U_FAILURE(status)) { return; }

        if (atStart) {
            // Identifiers optionally start with "per-".
            if (token.getType() == Token::TYPE_INITIAL_COMPOUND_PART) {
                U_ASSERT(token.getInitialCompoundPart() == INITIAL_COMPOUND_PART_PER);
                fAfterPer = true;
                result.dimensionality = -1;

                token = nextToken(status);
                if (U_FAILURE(status)) { return; }
            }
        } else {
            // All other SingleUnit's are separated from previous SingleUnit's
            // via a compound part:
            if (token.getType() != Token::TYPE_COMPOUND_PART) {
                status = kUnitIdentifierSyntaxError;
                return;
            }

            switch (token.getMatch()) {
            case COMPOUND_PART_PER:
                if (sawAnd) {
                    // Mixed compound units not yet supported,
                    // TODO(CLDR-13700).
                    status = kUnitIdentifierSyntaxError;
                    return;
                }
                fAfterPer = true;
                result.dimensionality = -1;
                break;

            case COMPOUND_PART_TIMES:
                if (fAfterPer) {
                    result.dimensionality = -1;
                }
                break;

            case COMPOUND_PART_AND:
                if (fAfterPer) {
                    // Can't start with "-and-", and mixed compound units
                    // not yet supported, TODO(CLDR-13700).
                    status = kUnitIdentifierSyntaxError;
                    return;
                }
                sawAnd = true;
                break;
            }

            token = nextToken(status);
            if (U_FAILURE(status)) { return; }
        }

        // Read tokens until we have a complete SingleUnit or we reach the end.
        while (true) {
            switch (token.getType()) {
                case Token::TYPE_POWER_PART:
                    if (state > 0) {
                        status = kUnitIdentifierSyntaxError;
                        return;
                    }
                    result.dimensionality *= token.getPower();
                    state = 1;
                    break;

                case Token::TYPE_SI_PREFIX:
                    if (state > 1) {
                        status = kUnitIdentifierSyntaxError;
                        return;
                    }
                    result.siPrefix = token.getSIPrefix();
                    state = 2;
                    break;

                case Token::TYPE_SIMPLE_UNIT:
                    result.index = token.getSimpleUnitIndex();
                    return;

                default:
                    status = kUnitIdentifierSyntaxError;
                    return;
            }

            if (!hasNext()) {
                // We ran out of tokens before finding a complete single unit.
                status = kUnitIdentifierSyntaxError;
                return;
            }
            token = nextToken(status);
            if (U_FAILURE(status)) {
                return;
            }
        }
    }

    /// @param result is modified, not overridden. Caller must pass in a
    /// default-constructed (empty) MeasureUnitImpl instance.
    void parseImpl(MeasureUnitImpl& result, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return;
        }
        if (fSource.empty()) {
            // The dimenionless unit: nothing to parse. leave result as is.
            return;
        }
        int32_t unitNum = 0;
        while (hasNext()) {
            bool sawAnd = false;
            SingleUnitImpl singleUnit;
            nextSingleUnit(singleUnit, sawAnd, status);
            if (U_FAILURE(status)) {
                return;
            }
            U_ASSERT(!singleUnit.isDimensionless());
            bool added = result.append(singleUnit, status);
            if (sawAnd && !added) {
                // Two similar units are not allowed in a mixed unit
                status = kUnitIdentifierSyntaxError;
                return;
            }
            if ((++unitNum) >= 2) {
                // nextSingleUnit fails appropriately for "per" and "and" in the
                // same identifier. It doesn't fail for other compound units
                // (COMPOUND_PART_TIMES). Consequently we take care of that
                // here.
                UMeasureUnitComplexity complexity =
                    sawAnd ? UMEASURE_UNIT_MIXED : UMEASURE_UNIT_COMPOUND;
                if (unitNum == 2) {
                    U_ASSERT(result.complexity == UMEASURE_UNIT_SINGLE);
                    result.complexity = complexity;
                } else if (result.complexity != complexity) {
                    // Can't have mixed compound units
                    status = kUnitIdentifierSyntaxError;
                    return;
                }
            }
        }
    }
};

int32_t U_CALLCONV
compareSingleUnits(const void* /*context*/, const void* left, const void* right) {
    auto realLeft = static_cast<const SingleUnitImpl* const*>(left);
    auto realRight = static_cast<const SingleUnitImpl* const*>(right);
    return (*realLeft)->compareTo(**realRight);
}

/**
 * Generate the identifier string for a single unit in place.
 *
 * Does not support the dimensionless SingleUnitImpl: calling serializeSingle
 * with the dimensionless unit results in an U_INTERNAL_PROGRAM_ERROR.
 *
 * @param first If singleUnit is part of a compound unit, and not its first
 * single unit, set this to false. Otherwise: set to true.
 */
void serializeSingle(const SingleUnitImpl& singleUnit, bool first, CharString& output, UErrorCode& status) {
    if (first && singleUnit.dimensionality < 0) {
        // Essentially the "unary per". For compound units with a numerator, the
        // caller takes care of the "binary per".
        output.append("per-", status);
    }

    if (singleUnit.isDimensionless()) {
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    int8_t posPower = std::abs(singleUnit.dimensionality);
    if (posPower == 0) {
        status = U_INTERNAL_PROGRAM_ERROR;
    } else if (posPower == 1) {
        // no-op
    } else if (posPower == 2) {
        output.append("square-", status);
    } else if (posPower == 3) {
        output.append("cubic-", status);
    } else if (posPower < 10) {
        output.append('p', status);
        output.append(posPower + '0', status);
        output.append('-', status);
    } else if (posPower <= 15) {
        output.append("p1", status);
        output.append('0' + (posPower % 10), status);
        output.append('-', status);
    } else {
        status = kUnitIdentifierSyntaxError;
    }
    if (U_FAILURE(status)) {
        return;
    }

    if (singleUnit.siPrefix != UMEASURE_SI_PREFIX_ONE) {
        for (const auto& siPrefixInfo : gSIPrefixStrings) {
            if (siPrefixInfo.value == singleUnit.siPrefix) {
                output.append(siPrefixInfo.string, status);
                break;
            }
        }
    }
    if (U_FAILURE(status)) {
        return;
    }

    output.appendInvariantChars(gSimpleUnits[singleUnit.index], status);
}

/**
 * Normalize a MeasureUnitImpl and generate the identifier string in place.
 */
void serialize(MeasureUnitImpl& impl, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(impl.identifier.isEmpty());
    if (impl.units.length() == 0) {
        // Dimensionless, constructed by the default constructor: no appending
        // to impl.identifier, we wish it to contain the zero-length string.
        return;
    }
    if (impl.complexity == UMEASURE_UNIT_COMPOUND) {
        // Note: don't sort a MIXED unit
        uprv_sortArray(
            impl.units.getAlias(),
            impl.units.length(),
            sizeof(impl.units[0]),
            compareSingleUnits,
            nullptr,
            false,
            &status);
        if (U_FAILURE(status)) {
            return;
        }
    }
    serializeSingle(*impl.units[0], true, impl.identifier, status);
    if (impl.units.length() == 1) {
        return;
    }
    for (int32_t i = 1; i < impl.units.length(); i++) {
        const SingleUnitImpl& prev = *impl.units[i-1];
        const SingleUnitImpl& curr = *impl.units[i];
        if (impl.complexity == UMEASURE_UNIT_MIXED) {
            impl.identifier.append("-and-", status);
            serializeSingle(curr, true, impl.identifier, status);
        } else {
            if (prev.dimensionality > 0 && curr.dimensionality < 0) {
                impl.identifier.append("-per-", status);
            } else {
                impl.identifier.append('-', status);
            }
            serializeSingle(curr, false, impl.identifier, status);
        }
    }

}

/**
 * Appends a SingleUnitImpl to a MeasureUnitImpl.
 *
 * @return true if a new item was added. If unit is the dimensionless unit, it
 * is never added: the return value will always be false.
 */
bool appendImpl(MeasureUnitImpl& impl, const SingleUnitImpl& unit, UErrorCode& status) {
    if (unit.isDimensionless()) {
        // We don't append dimensionless units.
        return false;
    }
    // Find a similar unit that already exists, to attempt to coalesce
    SingleUnitImpl* oldUnit = nullptr;
    for (int32_t i = 0; i < impl.units.length(); i++) {
        auto* candidate = impl.units[i];
        if (candidate->isCompatibleWith(unit)) {
            oldUnit = candidate;
        }
    }
    if (oldUnit) {
        // Both dimensionalities will be positive, or both will be negative, by
        // virtue of isCompatibleWith().
        oldUnit->dimensionality += unit.dimensionality;
    } else {
        SingleUnitImpl* destination = impl.units.emplaceBack();
        if (!destination) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return false;
        }
        *destination = unit;
    }
    return (oldUnit == nullptr);
}

} // namespace


SingleUnitImpl SingleUnitImpl::forMeasureUnit(const MeasureUnit& measureUnit, UErrorCode& status) {
    MeasureUnitImpl temp;
    const MeasureUnitImpl& impl = MeasureUnitImpl::forMeasureUnit(measureUnit, temp, status);
    if (U_FAILURE(status)) {
        return {};
    }
    if (impl.units.length() == 0) {
        return {};
    }
    if (impl.units.length() == 1) {
        return *impl.units[0];
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return {};
}

MeasureUnit SingleUnitImpl::build(UErrorCode& status) const {
    MeasureUnitImpl temp;
    temp.append(*this, status);
    return std::move(temp).build(status);
}


MeasureUnitImpl MeasureUnitImpl::forIdentifier(StringPiece identifier, UErrorCode& status) {
    return Parser::from(identifier, status).parse(status);
}

const MeasureUnitImpl& MeasureUnitImpl::forMeasureUnit(
        const MeasureUnit& measureUnit, MeasureUnitImpl& memory, UErrorCode& status) {
    if (measureUnit.fImpl) {
        return *measureUnit.fImpl;
    } else {
        memory = Parser::from(measureUnit.getIdentifier(), status).parse(status);
        return memory;
    }
}

MeasureUnitImpl MeasureUnitImpl::forMeasureUnitMaybeCopy(
        const MeasureUnit& measureUnit, UErrorCode& status) {
    if (measureUnit.fImpl) {
        return measureUnit.fImpl->copy(status);
    } else {
        return Parser::from(measureUnit.getIdentifier(), status).parse(status);
    }
}

void MeasureUnitImpl::takeReciprocal(UErrorCode& /*status*/) {
    identifier.clear();
    for (int32_t i = 0; i < units.length(); i++) {
        units[i]->dimensionality *= -1;
    }
}

bool MeasureUnitImpl::append(const SingleUnitImpl& singleUnit, UErrorCode& status) {
    identifier.clear();
    return appendImpl(*this, singleUnit, status);
}

MeasureUnit MeasureUnitImpl::build(UErrorCode& status) && {
    serialize(*this, status);
    return MeasureUnit(std::move(*this));
}


MeasureUnit MeasureUnit::forIdentifier(StringPiece identifier, UErrorCode& status) {
    return Parser::from(identifier, status).parse(status).build(status);
}

UMeasureUnitComplexity MeasureUnit::getComplexity(UErrorCode& status) const {
    MeasureUnitImpl temp;
    return MeasureUnitImpl::forMeasureUnit(*this, temp, status).complexity;
}

UMeasureSIPrefix MeasureUnit::getSIPrefix(UErrorCode& status) const {
    return SingleUnitImpl::forMeasureUnit(*this, status).siPrefix;
}

MeasureUnit MeasureUnit::withSIPrefix(UMeasureSIPrefix prefix, UErrorCode& status) const {
    SingleUnitImpl singleUnit = SingleUnitImpl::forMeasureUnit(*this, status);
    singleUnit.siPrefix = prefix;
    return singleUnit.build(status);
}

int32_t MeasureUnit::getDimensionality(UErrorCode& status) const {
    SingleUnitImpl singleUnit = SingleUnitImpl::forMeasureUnit(*this, status);
    if (U_FAILURE(status)) { return 0; }
    if (singleUnit.isDimensionless()) {
        return 0;
    }
    return singleUnit.dimensionality;
}

MeasureUnit MeasureUnit::withDimensionality(int32_t dimensionality, UErrorCode& status) const {
    SingleUnitImpl singleUnit = SingleUnitImpl::forMeasureUnit(*this, status);
    singleUnit.dimensionality = dimensionality;
    return singleUnit.build(status);
}

MeasureUnit MeasureUnit::reciprocal(UErrorCode& status) const {
    MeasureUnitImpl impl = MeasureUnitImpl::forMeasureUnitMaybeCopy(*this, status);
    impl.takeReciprocal(status);
    return std::move(impl).build(status);
}

MeasureUnit MeasureUnit::product(const MeasureUnit& other, UErrorCode& status) const {
    MeasureUnitImpl impl = MeasureUnitImpl::forMeasureUnitMaybeCopy(*this, status);
    MeasureUnitImpl temp;
    const MeasureUnitImpl& otherImpl = MeasureUnitImpl::forMeasureUnit(other, temp, status);
    if (impl.complexity == UMEASURE_UNIT_MIXED || otherImpl.complexity == UMEASURE_UNIT_MIXED) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return {};
    }
    for (int32_t i = 0; i < otherImpl.units.length(); i++) {
        impl.append(*otherImpl.units[i], status);
    }
    if (impl.units.length() > 1) {
        impl.complexity = UMEASURE_UNIT_COMPOUND;
    }
    return std::move(impl).build(status);
}

LocalArray<MeasureUnit> MeasureUnit::splitToSingleUnits(int32_t& outCount, UErrorCode& status) const {
    MeasureUnitImpl temp;
    const MeasureUnitImpl& impl = MeasureUnitImpl::forMeasureUnit(*this, temp, status);
    outCount = impl.units.length();
    MeasureUnit* arr = new MeasureUnit[outCount];
    for (int32_t i = 0; i < outCount; i++) {
        arr[i] = impl.units[i]->build(status);
    }
    return LocalArray<MeasureUnit>(arr, status);
}


U_NAMESPACE_END

#endif /* !UNCONFIG_NO_FORMATTING */
