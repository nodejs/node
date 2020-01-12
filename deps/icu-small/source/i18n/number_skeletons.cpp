// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "number_decnum.h"
#include "number_skeletons.h"
#include "umutex.h"
#include "ucln_in.h"
#include "patternprops.h"
#include "unicode/ucharstriebuilder.h"
#include "number_utils.h"
#include "number_decimalquantity.h"
#include "unicode/numberformatter.h"
#include "uinvchar.h"
#include "charstr.h"
#include "string_segment.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;
using namespace icu::number::impl::skeleton;

namespace {

icu::UInitOnce gNumberSkeletonsInitOnce = U_INITONCE_INITIALIZER;

char16_t* kSerializedStemTrie = nullptr;

UBool U_CALLCONV cleanupNumberSkeletons() {
    uprv_free(kSerializedStemTrie);
    kSerializedStemTrie = nullptr;
    gNumberSkeletonsInitOnce.reset();
    return TRUE;
}

void U_CALLCONV initNumberSkeletons(UErrorCode& status) {
    ucln_i18n_registerCleanup(UCLN_I18N_NUMBER_SKELETONS, cleanupNumberSkeletons);

    UCharsTrieBuilder b(status);
    if (U_FAILURE(status)) { return; }

    // Section 1:
    b.add(u"compact-short", STEM_COMPACT_SHORT, status);
    b.add(u"compact-long", STEM_COMPACT_LONG, status);
    b.add(u"scientific", STEM_SCIENTIFIC, status);
    b.add(u"engineering", STEM_ENGINEERING, status);
    b.add(u"notation-simple", STEM_NOTATION_SIMPLE, status);
    b.add(u"base-unit", STEM_BASE_UNIT, status);
    b.add(u"percent", STEM_PERCENT, status);
    b.add(u"permille", STEM_PERMILLE, status);
    b.add(u"precision-integer", STEM_PRECISION_INTEGER, status);
    b.add(u"precision-unlimited", STEM_PRECISION_UNLIMITED, status);
    b.add(u"precision-currency-standard", STEM_PRECISION_CURRENCY_STANDARD, status);
    b.add(u"precision-currency-cash", STEM_PRECISION_CURRENCY_CASH, status);
    b.add(u"rounding-mode-ceiling", STEM_ROUNDING_MODE_CEILING, status);
    b.add(u"rounding-mode-floor", STEM_ROUNDING_MODE_FLOOR, status);
    b.add(u"rounding-mode-down", STEM_ROUNDING_MODE_DOWN, status);
    b.add(u"rounding-mode-up", STEM_ROUNDING_MODE_UP, status);
    b.add(u"rounding-mode-half-even", STEM_ROUNDING_MODE_HALF_EVEN, status);
    b.add(u"rounding-mode-half-down", STEM_ROUNDING_MODE_HALF_DOWN, status);
    b.add(u"rounding-mode-half-up", STEM_ROUNDING_MODE_HALF_UP, status);
    b.add(u"rounding-mode-unnecessary", STEM_ROUNDING_MODE_UNNECESSARY, status);
    b.add(u"group-off", STEM_GROUP_OFF, status);
    b.add(u"group-min2", STEM_GROUP_MIN2, status);
    b.add(u"group-auto", STEM_GROUP_AUTO, status);
    b.add(u"group-on-aligned", STEM_GROUP_ON_ALIGNED, status);
    b.add(u"group-thousands", STEM_GROUP_THOUSANDS, status);
    b.add(u"latin", STEM_LATIN, status);
    b.add(u"unit-width-narrow", STEM_UNIT_WIDTH_NARROW, status);
    b.add(u"unit-width-short", STEM_UNIT_WIDTH_SHORT, status);
    b.add(u"unit-width-full-name", STEM_UNIT_WIDTH_FULL_NAME, status);
    b.add(u"unit-width-iso-code", STEM_UNIT_WIDTH_ISO_CODE, status);
    b.add(u"unit-width-hidden", STEM_UNIT_WIDTH_HIDDEN, status);
    b.add(u"sign-auto", STEM_SIGN_AUTO, status);
    b.add(u"sign-always", STEM_SIGN_ALWAYS, status);
    b.add(u"sign-never", STEM_SIGN_NEVER, status);
    b.add(u"sign-accounting", STEM_SIGN_ACCOUNTING, status);
    b.add(u"sign-accounting-always", STEM_SIGN_ACCOUNTING_ALWAYS, status);
    b.add(u"sign-except-zero", STEM_SIGN_EXCEPT_ZERO, status);
    b.add(u"sign-accounting-except-zero", STEM_SIGN_ACCOUNTING_EXCEPT_ZERO, status);
    b.add(u"decimal-auto", STEM_DECIMAL_AUTO, status);
    b.add(u"decimal-always", STEM_DECIMAL_ALWAYS, status);
    if (U_FAILURE(status)) { return; }

    // Section 2:
    b.add(u"precision-increment", STEM_PRECISION_INCREMENT, status);
    b.add(u"measure-unit", STEM_MEASURE_UNIT, status);
    b.add(u"per-measure-unit", STEM_PER_MEASURE_UNIT, status);
    b.add(u"currency", STEM_CURRENCY, status);
    b.add(u"integer-width", STEM_INTEGER_WIDTH, status);
    b.add(u"numbering-system", STEM_NUMBERING_SYSTEM, status);
    b.add(u"scale", STEM_SCALE, status);
    if (U_FAILURE(status)) { return; }

    // Build the CharsTrie
    // TODO: Use SLOW or FAST here?
    UnicodeString result;
    b.buildUnicodeString(USTRINGTRIE_BUILD_FAST, result, status);
    if (U_FAILURE(status)) { return; }

    // Copy the result into the global constant pointer
    size_t numBytes = result.length() * sizeof(char16_t);
    kSerializedStemTrie = static_cast<char16_t*>(uprv_malloc(numBytes));
    uprv_memcpy(kSerializedStemTrie, result.getBuffer(), numBytes);
}


inline void appendMultiple(UnicodeString& sb, UChar32 cp, int32_t count) {
    for (int i = 0; i < count; i++) {
        sb.append(cp);
    }
}


#define CHECK_NULL(seen, field, status) (void)(seen); /* for auto-format line wrapping */ \
UPRV_BLOCK_MACRO_BEGIN { \
    if ((seen).field) { \
        (status) = U_NUMBER_SKELETON_SYNTAX_ERROR; \
        return STATE_NULL; \
    } \
    (seen).field = true; \
} UPRV_BLOCK_MACRO_END


#define SKELETON_UCHAR_TO_CHAR(dest, src, start, end, status) (void)(dest); \
UPRV_BLOCK_MACRO_BEGIN { \
    UErrorCode conversionStatus = U_ZERO_ERROR; \
    (dest).appendInvariantChars({FALSE, (src).getBuffer() + (start), (end) - (start)}, conversionStatus); \
    if (conversionStatus == U_INVARIANT_CONVERSION_ERROR) { \
        /* Don't propagate the invariant conversion error; it is a skeleton syntax error */ \
        (status) = U_NUMBER_SKELETON_SYNTAX_ERROR; \
        return; \
    } else if (U_FAILURE(conversionStatus)) { \
        (status) = conversionStatus; \
        return; \
    } \
} UPRV_BLOCK_MACRO_END


} // anonymous namespace


Notation stem_to_object::notation(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_COMPACT_SHORT:
            return Notation::compactShort();
        case STEM_COMPACT_LONG:
            return Notation::compactLong();
        case STEM_SCIENTIFIC:
            return Notation::scientific();
        case STEM_ENGINEERING:
            return Notation::engineering();
        case STEM_NOTATION_SIMPLE:
            return Notation::simple();
        default:
            UPRV_UNREACHABLE;
    }
}

MeasureUnit stem_to_object::unit(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_BASE_UNIT:
            // Slicing is okay
            return NoUnit::base(); // NOLINT
        case STEM_PERCENT:
            // Slicing is okay
            return NoUnit::percent(); // NOLINT
        case STEM_PERMILLE:
            // Slicing is okay
            return NoUnit::permille(); // NOLINT
        default:
            UPRV_UNREACHABLE;
    }
}

Precision stem_to_object::precision(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_PRECISION_INTEGER:
            return Precision::integer();
        case STEM_PRECISION_UNLIMITED:
            return Precision::unlimited();
        case STEM_PRECISION_CURRENCY_STANDARD:
            return Precision::currency(UCURR_USAGE_STANDARD);
        case STEM_PRECISION_CURRENCY_CASH:
            return Precision::currency(UCURR_USAGE_CASH);
        default:
            UPRV_UNREACHABLE;
    }
}

UNumberFormatRoundingMode stem_to_object::roundingMode(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_ROUNDING_MODE_CEILING:
            return UNUM_ROUND_CEILING;
        case STEM_ROUNDING_MODE_FLOOR:
            return UNUM_ROUND_FLOOR;
        case STEM_ROUNDING_MODE_DOWN:
            return UNUM_ROUND_DOWN;
        case STEM_ROUNDING_MODE_UP:
            return UNUM_ROUND_UP;
        case STEM_ROUNDING_MODE_HALF_EVEN:
            return UNUM_ROUND_HALFEVEN;
        case STEM_ROUNDING_MODE_HALF_DOWN:
            return UNUM_ROUND_HALFDOWN;
        case STEM_ROUNDING_MODE_HALF_UP:
            return UNUM_ROUND_HALFUP;
        case STEM_ROUNDING_MODE_UNNECESSARY:
            return UNUM_ROUND_UNNECESSARY;
        default:
            UPRV_UNREACHABLE;
    }
}

UNumberGroupingStrategy stem_to_object::groupingStrategy(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_GROUP_OFF:
            return UNUM_GROUPING_OFF;
        case STEM_GROUP_MIN2:
            return UNUM_GROUPING_MIN2;
        case STEM_GROUP_AUTO:
            return UNUM_GROUPING_AUTO;
        case STEM_GROUP_ON_ALIGNED:
            return UNUM_GROUPING_ON_ALIGNED;
        case STEM_GROUP_THOUSANDS:
            return UNUM_GROUPING_THOUSANDS;
        default:
            return UNUM_GROUPING_COUNT; // for objects, throw; for enums, return COUNT
    }
}

UNumberUnitWidth stem_to_object::unitWidth(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_UNIT_WIDTH_NARROW:
            return UNUM_UNIT_WIDTH_NARROW;
        case STEM_UNIT_WIDTH_SHORT:
            return UNUM_UNIT_WIDTH_SHORT;
        case STEM_UNIT_WIDTH_FULL_NAME:
            return UNUM_UNIT_WIDTH_FULL_NAME;
        case STEM_UNIT_WIDTH_ISO_CODE:
            return UNUM_UNIT_WIDTH_ISO_CODE;
        case STEM_UNIT_WIDTH_HIDDEN:
            return UNUM_UNIT_WIDTH_HIDDEN;
        default:
            return UNUM_UNIT_WIDTH_COUNT; // for objects, throw; for enums, return COUNT
    }
}

UNumberSignDisplay stem_to_object::signDisplay(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_SIGN_AUTO:
            return UNUM_SIGN_AUTO;
        case STEM_SIGN_ALWAYS:
            return UNUM_SIGN_ALWAYS;
        case STEM_SIGN_NEVER:
            return UNUM_SIGN_NEVER;
        case STEM_SIGN_ACCOUNTING:
            return UNUM_SIGN_ACCOUNTING;
        case STEM_SIGN_ACCOUNTING_ALWAYS:
            return UNUM_SIGN_ACCOUNTING_ALWAYS;
        case STEM_SIGN_EXCEPT_ZERO:
            return UNUM_SIGN_EXCEPT_ZERO;
        case STEM_SIGN_ACCOUNTING_EXCEPT_ZERO:
            return UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO;
        default:
            return UNUM_SIGN_COUNT; // for objects, throw; for enums, return COUNT
    }
}

UNumberDecimalSeparatorDisplay stem_to_object::decimalSeparatorDisplay(skeleton::StemEnum stem) {
    switch (stem) {
        case STEM_DECIMAL_AUTO:
            return UNUM_DECIMAL_SEPARATOR_AUTO;
        case STEM_DECIMAL_ALWAYS:
            return UNUM_DECIMAL_SEPARATOR_ALWAYS;
        default:
            return UNUM_DECIMAL_SEPARATOR_COUNT; // for objects, throw; for enums, return COUNT
    }
}


void enum_to_stem_string::roundingMode(UNumberFormatRoundingMode value, UnicodeString& sb) {
    switch (value) {
        case UNUM_ROUND_CEILING:
            sb.append(u"rounding-mode-ceiling", -1);
            break;
        case UNUM_ROUND_FLOOR:
            sb.append(u"rounding-mode-floor", -1);
            break;
        case UNUM_ROUND_DOWN:
            sb.append(u"rounding-mode-down", -1);
            break;
        case UNUM_ROUND_UP:
            sb.append(u"rounding-mode-up", -1);
            break;
        case UNUM_ROUND_HALFEVEN:
            sb.append(u"rounding-mode-half-even", -1);
            break;
        case UNUM_ROUND_HALFDOWN:
            sb.append(u"rounding-mode-half-down", -1);
            break;
        case UNUM_ROUND_HALFUP:
            sb.append(u"rounding-mode-half-up", -1);
            break;
        case UNUM_ROUND_UNNECESSARY:
            sb.append(u"rounding-mode-unnecessary", -1);
            break;
        default:
            UPRV_UNREACHABLE;
    }
}

void enum_to_stem_string::groupingStrategy(UNumberGroupingStrategy value, UnicodeString& sb) {
    switch (value) {
        case UNUM_GROUPING_OFF:
            sb.append(u"group-off", -1);
            break;
        case UNUM_GROUPING_MIN2:
            sb.append(u"group-min2", -1);
            break;
        case UNUM_GROUPING_AUTO:
            sb.append(u"group-auto", -1);
            break;
        case UNUM_GROUPING_ON_ALIGNED:
            sb.append(u"group-on-aligned", -1);
            break;
        case UNUM_GROUPING_THOUSANDS:
            sb.append(u"group-thousands", -1);
            break;
        default:
            UPRV_UNREACHABLE;
    }
}

void enum_to_stem_string::unitWidth(UNumberUnitWidth value, UnicodeString& sb) {
    switch (value) {
        case UNUM_UNIT_WIDTH_NARROW:
            sb.append(u"unit-width-narrow", -1);
            break;
        case UNUM_UNIT_WIDTH_SHORT:
            sb.append(u"unit-width-short", -1);
            break;
        case UNUM_UNIT_WIDTH_FULL_NAME:
            sb.append(u"unit-width-full-name", -1);
            break;
        case UNUM_UNIT_WIDTH_ISO_CODE:
            sb.append(u"unit-width-iso-code", -1);
            break;
        case UNUM_UNIT_WIDTH_HIDDEN:
            sb.append(u"unit-width-hidden", -1);
            break;
        default:
            UPRV_UNREACHABLE;
    }
}

void enum_to_stem_string::signDisplay(UNumberSignDisplay value, UnicodeString& sb) {
    switch (value) {
        case UNUM_SIGN_AUTO:
            sb.append(u"sign-auto", -1);
            break;
        case UNUM_SIGN_ALWAYS:
            sb.append(u"sign-always", -1);
            break;
        case UNUM_SIGN_NEVER:
            sb.append(u"sign-never", -1);
            break;
        case UNUM_SIGN_ACCOUNTING:
            sb.append(u"sign-accounting", -1);
            break;
        case UNUM_SIGN_ACCOUNTING_ALWAYS:
            sb.append(u"sign-accounting-always", -1);
            break;
        case UNUM_SIGN_EXCEPT_ZERO:
            sb.append(u"sign-except-zero", -1);
            break;
        case UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO:
            sb.append(u"sign-accounting-except-zero", -1);
            break;
        default:
            UPRV_UNREACHABLE;
    }
}

void
enum_to_stem_string::decimalSeparatorDisplay(UNumberDecimalSeparatorDisplay value, UnicodeString& sb) {
    switch (value) {
        case UNUM_DECIMAL_SEPARATOR_AUTO:
            sb.append(u"decimal-auto", -1);
            break;
        case UNUM_DECIMAL_SEPARATOR_ALWAYS:
            sb.append(u"decimal-always", -1);
            break;
        default:
            UPRV_UNREACHABLE;
    }
}


UnlocalizedNumberFormatter skeleton::create(
        const UnicodeString& skeletonString, UParseError* perror, UErrorCode& status) {

    // Initialize perror
    if (perror != nullptr) {
        perror->line = 0;
        perror->offset = -1;
        perror->preContext[0] = 0;
        perror->postContext[0] = 0;
    }

    umtx_initOnce(gNumberSkeletonsInitOnce, &initNumberSkeletons, status);
    if (U_FAILURE(status)) {
        return {};
    }

    int32_t errOffset;
    MacroProps macros = parseSkeleton(skeletonString, errOffset, status);
    if (U_SUCCESS(status)) {
        return NumberFormatter::with().macros(macros);
    }

    if (perror == nullptr) {
        return {};
    }

    // Populate the UParseError with the error location
    perror->offset = errOffset;
    int32_t contextStart = uprv_max(0, errOffset - U_PARSE_CONTEXT_LEN + 1);
    int32_t contextEnd = uprv_min(skeletonString.length(), errOffset + U_PARSE_CONTEXT_LEN - 1);
    skeletonString.extract(contextStart, errOffset - contextStart, perror->preContext, 0);
    perror->preContext[errOffset - contextStart] = 0;
    skeletonString.extract(errOffset, contextEnd - errOffset, perror->postContext, 0);
    perror->postContext[contextEnd - errOffset] = 0;
    return {};
}

UnicodeString skeleton::generate(const MacroProps& macros, UErrorCode& status) {
    umtx_initOnce(gNumberSkeletonsInitOnce, &initNumberSkeletons, status);
    UnicodeString sb;
    GeneratorHelpers::generateSkeleton(macros, sb, status);
    return sb;
}

MacroProps skeleton::parseSkeleton(
        const UnicodeString& skeletonString, int32_t& errOffset, UErrorCode& status) {
    U_ASSERT(U_SUCCESS(status));

    // Add a trailing whitespace to the end of the skeleton string to make code cleaner.
    UnicodeString tempSkeletonString(skeletonString);
    tempSkeletonString.append(u' ');

    SeenMacroProps seen;
    MacroProps macros;
    StringSegment segment(tempSkeletonString, false);
    UCharsTrie stemTrie(kSerializedStemTrie);
    ParseState stem = STATE_NULL;
    int32_t offset = 0;

    // Primary skeleton parse loop:
    while (offset < segment.length()) {
        UChar32 cp = segment.codePointAt(offset);
        bool isTokenSeparator = PatternProps::isWhiteSpace(cp);
        bool isOptionSeparator = (cp == u'/');

        if (!isTokenSeparator && !isOptionSeparator) {
            // Non-separator token; consume it.
            offset += U16_LENGTH(cp);
            if (stem == STATE_NULL) {
                // We are currently consuming a stem.
                // Go to the next state in the stem trie.
                stemTrie.nextForCodePoint(cp);
            }
            continue;
        }

        // We are looking at a token or option separator.
        // If the segment is nonempty, parse it and reset the segment.
        // Otherwise, make sure it is a valid repeating separator.
        if (offset != 0) {
            segment.setLength(offset);
            if (stem == STATE_NULL) {
                // The first separator after the start of a token. Parse it as a stem.
                stem = parseStem(segment, stemTrie, seen, macros, status);
                stemTrie.reset();
            } else {
                // A separator after the first separator of a token. Parse it as an option.
                stem = parseOption(stem, segment, macros, status);
            }
            segment.resetLength();
            if (U_FAILURE(status)) {
                errOffset = segment.getOffset();
                return macros;
            }

            // Consume the segment:
            segment.adjustOffset(offset);
            offset = 0;

        } else if (stem != STATE_NULL) {
            // A separator ('/' or whitespace) following an option separator ('/')
            // segment.setLength(U16_LENGTH(cp)); // for error message
            // throw new SkeletonSyntaxException("Unexpected separator character", segment);
            status = U_NUMBER_SKELETON_SYNTAX_ERROR;
            errOffset = segment.getOffset();
            return macros;

        } else {
            // Two spaces in a row; this is OK.
        }

        // Does the current stem forbid options?
        if (isOptionSeparator && stem == STATE_NULL) {
            // segment.setLength(U16_LENGTH(cp)); // for error message
            // throw new SkeletonSyntaxException("Unexpected option separator", segment);
            status = U_NUMBER_SKELETON_SYNTAX_ERROR;
            errOffset = segment.getOffset();
            return macros;
        }

        // Does the current stem require an option?
        if (isTokenSeparator && stem != STATE_NULL) {
            switch (stem) {
                case STATE_INCREMENT_PRECISION:
                case STATE_MEASURE_UNIT:
                case STATE_PER_MEASURE_UNIT:
                case STATE_CURRENCY_UNIT:
                case STATE_INTEGER_WIDTH:
                case STATE_NUMBERING_SYSTEM:
                case STATE_SCALE:
                    // segment.setLength(U16_LENGTH(cp)); // for error message
                    // throw new SkeletonSyntaxException("Stem requires an option", segment);
                    status = U_NUMBER_SKELETON_SYNTAX_ERROR;
                    errOffset = segment.getOffset();
                    return macros;
                default:
                    break;
            }
            stem = STATE_NULL;
        }

        // Consume the separator:
        segment.adjustOffset(U16_LENGTH(cp));
    }
    U_ASSERT(stem == STATE_NULL);
    return macros;
}

ParseState
skeleton::parseStem(const StringSegment& segment, const UCharsTrie& stemTrie, SeenMacroProps& seen,
                    MacroProps& macros, UErrorCode& status) {
    // First check for "blueprint" stems, which start with a "signal char"
    switch (segment.charAt(0)) {
        case u'.':
        CHECK_NULL(seen, precision, status);
            blueprint_helpers::parseFractionStem(segment, macros, status);
            return STATE_FRACTION_PRECISION;
        case u'@':
        CHECK_NULL(seen, precision, status);
            blueprint_helpers::parseDigitsStem(segment, macros, status);
            return STATE_NULL;
        default:
            break;
    }

    // Now look at the stemsTrie, which is already be pointing at our stem.
    UStringTrieResult stemResult = stemTrie.current();

    if (stemResult != USTRINGTRIE_INTERMEDIATE_VALUE && stemResult != USTRINGTRIE_FINAL_VALUE) {
        // throw new SkeletonSyntaxException("Unknown stem", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return STATE_NULL;
    }

    auto stem = static_cast<StemEnum>(stemTrie.getValue());
    switch (stem) {

        // Stems with meaning on their own, not requiring an option:

        case STEM_COMPACT_SHORT:
        case STEM_COMPACT_LONG:
        case STEM_SCIENTIFIC:
        case STEM_ENGINEERING:
        case STEM_NOTATION_SIMPLE:
        CHECK_NULL(seen, notation, status);
            macros.notation = stem_to_object::notation(stem);
            switch (stem) {
                case STEM_SCIENTIFIC:
                case STEM_ENGINEERING:
                    return STATE_SCIENTIFIC; // allows for scientific options
                default:
                    return STATE_NULL;
            }

        case STEM_BASE_UNIT:
        case STEM_PERCENT:
        case STEM_PERMILLE:
        CHECK_NULL(seen, unit, status);
            macros.unit = stem_to_object::unit(stem);
            return STATE_NULL;

        case STEM_PRECISION_INTEGER:
        case STEM_PRECISION_UNLIMITED:
        case STEM_PRECISION_CURRENCY_STANDARD:
        case STEM_PRECISION_CURRENCY_CASH:
        CHECK_NULL(seen, precision, status);
            macros.precision = stem_to_object::precision(stem);
            switch (stem) {
                case STEM_PRECISION_INTEGER:
                    return STATE_FRACTION_PRECISION; // allows for "precision-integer/@##"
                default:
                    return STATE_NULL;
            }

        case STEM_ROUNDING_MODE_CEILING:
        case STEM_ROUNDING_MODE_FLOOR:
        case STEM_ROUNDING_MODE_DOWN:
        case STEM_ROUNDING_MODE_UP:
        case STEM_ROUNDING_MODE_HALF_EVEN:
        case STEM_ROUNDING_MODE_HALF_DOWN:
        case STEM_ROUNDING_MODE_HALF_UP:
        case STEM_ROUNDING_MODE_UNNECESSARY:
        CHECK_NULL(seen, roundingMode, status);
            macros.roundingMode = stem_to_object::roundingMode(stem);
            return STATE_NULL;

        case STEM_GROUP_OFF:
        case STEM_GROUP_MIN2:
        case STEM_GROUP_AUTO:
        case STEM_GROUP_ON_ALIGNED:
        case STEM_GROUP_THOUSANDS:
        CHECK_NULL(seen, grouper, status);
            macros.grouper = Grouper::forStrategy(stem_to_object::groupingStrategy(stem));
            return STATE_NULL;

        case STEM_LATIN:
        CHECK_NULL(seen, symbols, status);
            macros.symbols.setTo(NumberingSystem::createInstanceByName("latn", status));
            return STATE_NULL;

        case STEM_UNIT_WIDTH_NARROW:
        case STEM_UNIT_WIDTH_SHORT:
        case STEM_UNIT_WIDTH_FULL_NAME:
        case STEM_UNIT_WIDTH_ISO_CODE:
        case STEM_UNIT_WIDTH_HIDDEN:
        CHECK_NULL(seen, unitWidth, status);
            macros.unitWidth = stem_to_object::unitWidth(stem);
            return STATE_NULL;

        case STEM_SIGN_AUTO:
        case STEM_SIGN_ALWAYS:
        case STEM_SIGN_NEVER:
        case STEM_SIGN_ACCOUNTING:
        case STEM_SIGN_ACCOUNTING_ALWAYS:
        case STEM_SIGN_EXCEPT_ZERO:
        case STEM_SIGN_ACCOUNTING_EXCEPT_ZERO:
        CHECK_NULL(seen, sign, status);
            macros.sign = stem_to_object::signDisplay(stem);
            return STATE_NULL;

        case STEM_DECIMAL_AUTO:
        case STEM_DECIMAL_ALWAYS:
        CHECK_NULL(seen, decimal, status);
            macros.decimal = stem_to_object::decimalSeparatorDisplay(stem);
            return STATE_NULL;

            // Stems requiring an option:

        case STEM_PRECISION_INCREMENT:
        CHECK_NULL(seen, precision, status);
            return STATE_INCREMENT_PRECISION;

        case STEM_MEASURE_UNIT:
        CHECK_NULL(seen, unit, status);
            return STATE_MEASURE_UNIT;

        case STEM_PER_MEASURE_UNIT:
        CHECK_NULL(seen, perUnit, status);
            return STATE_PER_MEASURE_UNIT;

        case STEM_CURRENCY:
        CHECK_NULL(seen, unit, status);
            return STATE_CURRENCY_UNIT;

        case STEM_INTEGER_WIDTH:
        CHECK_NULL(seen, integerWidth, status);
            return STATE_INTEGER_WIDTH;

        case STEM_NUMBERING_SYSTEM:
        CHECK_NULL(seen, symbols, status);
            return STATE_NUMBERING_SYSTEM;

        case STEM_SCALE:
        CHECK_NULL(seen, scale, status);
            return STATE_SCALE;

        default:
            UPRV_UNREACHABLE;
    }
}

ParseState skeleton::parseOption(ParseState stem, const StringSegment& segment, MacroProps& macros,
                                 UErrorCode& status) {

    ///// Required options: /////

    switch (stem) {
        case STATE_CURRENCY_UNIT:
            blueprint_helpers::parseCurrencyOption(segment, macros, status);
            return STATE_NULL;
        case STATE_MEASURE_UNIT:
            blueprint_helpers::parseMeasureUnitOption(segment, macros, status);
            return STATE_NULL;
        case STATE_PER_MEASURE_UNIT:
            blueprint_helpers::parseMeasurePerUnitOption(segment, macros, status);
            return STATE_NULL;
        case STATE_INCREMENT_PRECISION:
            blueprint_helpers::parseIncrementOption(segment, macros, status);
            return STATE_NULL;
        case STATE_INTEGER_WIDTH:
            blueprint_helpers::parseIntegerWidthOption(segment, macros, status);
            return STATE_NULL;
        case STATE_NUMBERING_SYSTEM:
            blueprint_helpers::parseNumberingSystemOption(segment, macros, status);
            return STATE_NULL;
        case STATE_SCALE:
            blueprint_helpers::parseScaleOption(segment, macros, status);
            return STATE_NULL;
        default:
            break;
    }

    ///// Non-required options: /////

    // Scientific options
    switch (stem) {
        case STATE_SCIENTIFIC:
            if (blueprint_helpers::parseExponentWidthOption(segment, macros, status)) {
                return STATE_SCIENTIFIC;
            }
            if (U_FAILURE(status)) {
                return {};
            }
            if (blueprint_helpers::parseExponentSignOption(segment, macros, status)) {
                return STATE_SCIENTIFIC;
            }
            if (U_FAILURE(status)) {
                return {};
            }
            break;
        default:
            break;
    }

    // Frac-sig option
    switch (stem) {
        case STATE_FRACTION_PRECISION:
            if (blueprint_helpers::parseFracSigOption(segment, macros, status)) {
                return STATE_NULL;
            }
            if (U_FAILURE(status)) {
                return {};
            }
            break;
        default:
            break;
    }

    // Unknown option
    // throw new SkeletonSyntaxException("Invalid option", segment);
    status = U_NUMBER_SKELETON_SYNTAX_ERROR;
    return STATE_NULL;
}

void GeneratorHelpers::generateSkeleton(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }

    // Supported options
    if (GeneratorHelpers::notation(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::unit(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::perUnit(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::precision(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::roundingMode(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::grouping(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::integerWidth(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::symbols(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::unitWidth(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::sign(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::decimal(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }
    if (GeneratorHelpers::scale(macros, sb, status)) {
        sb.append(u' ');
    }
    if (U_FAILURE(status)) { return; }

    // Unsupported options
    if (!macros.padder.isBogus()) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }
    if (macros.affixProvider != nullptr) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }
    if (macros.rules != nullptr) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }
    if (macros.currencySymbols != nullptr) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }

    // Remove the trailing space
    if (sb.length() > 0) {
        sb.truncate(sb.length() - 1);
    }
}


bool blueprint_helpers::parseExponentWidthOption(const StringSegment& segment, MacroProps& macros,
                                                 UErrorCode&) {
    if (segment.charAt(0) != u'+') {
        return false;
    }
    int32_t offset = 1;
    int32_t minExp = 0;
    for (; offset < segment.length(); offset++) {
        if (segment.charAt(offset) == u'e') {
            minExp++;
        } else {
            break;
        }
    }
    if (offset < segment.length()) {
        return false;
    }
    // Use the public APIs to enforce bounds checking
    macros.notation = static_cast<ScientificNotation&>(macros.notation).withMinExponentDigits(minExp);
    return true;
}

void
blueprint_helpers::generateExponentWidthOption(int32_t minExponentDigits, UnicodeString& sb, UErrorCode&) {
    sb.append(u'+');
    appendMultiple(sb, u'e', minExponentDigits);
}

bool
blueprint_helpers::parseExponentSignOption(const StringSegment& segment, MacroProps& macros, UErrorCode&) {
    // Get the sign display type out of the CharsTrie data structure.
    UCharsTrie tempStemTrie(kSerializedStemTrie);
    UStringTrieResult result = tempStemTrie.next(
            segment.toTempUnicodeString().getBuffer(),
            segment.length());
    if (result != USTRINGTRIE_INTERMEDIATE_VALUE && result != USTRINGTRIE_FINAL_VALUE) {
        return false;
    }
    auto sign = stem_to_object::signDisplay(static_cast<StemEnum>(tempStemTrie.getValue()));
    if (sign == UNUM_SIGN_COUNT) {
        return false;
    }
    macros.notation = static_cast<ScientificNotation&>(macros.notation).withExponentSignDisplay(sign);
    return true;
}

void blueprint_helpers::parseCurrencyOption(const StringSegment& segment, MacroProps& macros,
                                            UErrorCode& status) {
    // Unlike ICU4J, have to check length manually because ICU4C CurrencyUnit does not check it for us
    if (segment.length() != 3) {
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    const UChar* currencyCode = segment.toTempUnicodeString().getBuffer();
    UErrorCode localStatus = U_ZERO_ERROR;
    CurrencyUnit currency(currencyCode, localStatus);
    if (U_FAILURE(localStatus)) {
        // Not 3 ascii chars
        // throw new SkeletonSyntaxException("Invalid currency", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    // Slicing is OK
    macros.unit = currency; // NOLINT
}

void
blueprint_helpers::generateCurrencyOption(const CurrencyUnit& currency, UnicodeString& sb, UErrorCode&) {
    sb.append(currency.getISOCurrency(), -1);
}

void blueprint_helpers::parseMeasureUnitOption(const StringSegment& segment, MacroProps& macros,
                                               UErrorCode& status) {
    const UnicodeString stemString = segment.toTempUnicodeString();

    // NOTE: The category (type) of the unit is guaranteed to be a valid subtag (alphanumeric)
    // http://unicode.org/reports/tr35/#Validity_Data
    int firstHyphen = 0;
    while (firstHyphen < stemString.length() && stemString.charAt(firstHyphen) != '-') {
        firstHyphen++;
    }
    if (firstHyphen == stemString.length()) {
        // throw new SkeletonSyntaxException("Invalid measure unit option", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }

    // Need to do char <-> UChar conversion...
    U_ASSERT(U_SUCCESS(status));
    CharString type;
    SKELETON_UCHAR_TO_CHAR(type, stemString, 0, firstHyphen, status);
    CharString subType;
    SKELETON_UCHAR_TO_CHAR(subType, stemString, firstHyphen + 1, stemString.length(), status);

    // Note: the largest type as of this writing (March 2018) is "volume", which has 24 units.
    static constexpr int32_t CAPACITY = 30;
    MeasureUnit units[CAPACITY];
    UErrorCode localStatus = U_ZERO_ERROR;
    int32_t numUnits = MeasureUnit::getAvailable(type.data(), units, CAPACITY, localStatus);
    if (U_FAILURE(localStatus)) {
        // More than 30 units in this type?
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    for (int32_t i = 0; i < numUnits; i++) {
        auto& unit = units[i];
        if (uprv_strcmp(subType.data(), unit.getSubtype()) == 0) {
            macros.unit = unit;
            return;
        }
    }

    // throw new SkeletonSyntaxException("Unknown measure unit", segment);
    status = U_NUMBER_SKELETON_SYNTAX_ERROR;
}

void blueprint_helpers::generateMeasureUnitOption(const MeasureUnit& measureUnit, UnicodeString& sb,
                                                  UErrorCode&) {
    // Need to do char <-> UChar conversion...
    sb.append(UnicodeString(measureUnit.getType(), -1, US_INV));
    sb.append(u'-');
    sb.append(UnicodeString(measureUnit.getSubtype(), -1, US_INV));
}

void blueprint_helpers::parseMeasurePerUnitOption(const StringSegment& segment, MacroProps& macros,
                                                  UErrorCode& status) {
    // A little bit of a hack: safe the current unit (numerator), call the main measure unit
    // parsing code, put back the numerator unit, and put the new unit into per-unit.
    MeasureUnit numerator = macros.unit;
    parseMeasureUnitOption(segment, macros, status);
    if (U_FAILURE(status)) { return; }
    macros.perUnit = macros.unit;
    macros.unit = numerator;
}

void blueprint_helpers::parseFractionStem(const StringSegment& segment, MacroProps& macros,
                                          UErrorCode& status) {
    U_ASSERT(segment.charAt(0) == u'.');
    int32_t offset = 1;
    int32_t minFrac = 0;
    int32_t maxFrac;
    for (; offset < segment.length(); offset++) {
        if (segment.charAt(offset) == u'0') {
            minFrac++;
        } else {
            break;
        }
    }
    if (offset < segment.length()) {
        if (segment.charAt(offset) == u'+') {
            maxFrac = -1;
            offset++;
        } else {
            maxFrac = minFrac;
            for (; offset < segment.length(); offset++) {
                if (segment.charAt(offset) == u'#') {
                    maxFrac++;
                } else {
                    break;
                }
            }
        }
    } else {
        maxFrac = minFrac;
    }
    if (offset < segment.length()) {
        // throw new SkeletonSyntaxException("Invalid fraction stem", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    // Use the public APIs to enforce bounds checking
    if (maxFrac == -1) {
        macros.precision = Precision::minFraction(minFrac);
    } else {
        macros.precision = Precision::minMaxFraction(minFrac, maxFrac);
    }
}

void
blueprint_helpers::generateFractionStem(int32_t minFrac, int32_t maxFrac, UnicodeString& sb, UErrorCode&) {
    if (minFrac == 0 && maxFrac == 0) {
        sb.append(u"precision-integer", -1);
        return;
    }
    sb.append(u'.');
    appendMultiple(sb, u'0', minFrac);
    if (maxFrac == -1) {
        sb.append(u'+');
    } else {
        appendMultiple(sb, u'#', maxFrac - minFrac);
    }
}

void
blueprint_helpers::parseDigitsStem(const StringSegment& segment, MacroProps& macros, UErrorCode& status) {
    U_ASSERT(segment.charAt(0) == u'@');
    int offset = 0;
    int minSig = 0;
    int maxSig;
    for (; offset < segment.length(); offset++) {
        if (segment.charAt(offset) == u'@') {
            minSig++;
        } else {
            break;
        }
    }
    if (offset < segment.length()) {
        if (segment.charAt(offset) == u'+') {
            maxSig = -1;
            offset++;
        } else {
            maxSig = minSig;
            for (; offset < segment.length(); offset++) {
                if (segment.charAt(offset) == u'#') {
                    maxSig++;
                } else {
                    break;
                }
            }
        }
    } else {
        maxSig = minSig;
    }
    if (offset < segment.length()) {
        // throw new SkeletonSyntaxException("Invalid significant digits stem", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    // Use the public APIs to enforce bounds checking
    if (maxSig == -1) {
        macros.precision = Precision::minSignificantDigits(minSig);
    } else {
        macros.precision = Precision::minMaxSignificantDigits(minSig, maxSig);
    }
}

void
blueprint_helpers::generateDigitsStem(int32_t minSig, int32_t maxSig, UnicodeString& sb, UErrorCode&) {
    appendMultiple(sb, u'@', minSig);
    if (maxSig == -1) {
        sb.append(u'+');
    } else {
        appendMultiple(sb, u'#', maxSig - minSig);
    }
}

bool blueprint_helpers::parseFracSigOption(const StringSegment& segment, MacroProps& macros,
                                           UErrorCode& status) {
    if (segment.charAt(0) != u'@') {
        return false;
    }
    int offset = 0;
    int minSig = 0;
    int maxSig;
    for (; offset < segment.length(); offset++) {
        if (segment.charAt(offset) == u'@') {
            minSig++;
        } else {
            break;
        }
    }
    // For the frac-sig option, there must be minSig or maxSig but not both.
    // Valid: @+, @@+, @@@+
    // Valid: @#, @##, @###
    // Invalid: @, @@, @@@
    // Invalid: @@#, @@##, @@@#
    if (offset < segment.length()) {
        if (segment.charAt(offset) == u'+') {
            maxSig = -1;
            offset++;
        } else if (minSig > 1) {
            // @@#, @@##, @@@#
            // throw new SkeletonSyntaxException("Invalid digits option for fraction rounder", segment);
            status = U_NUMBER_SKELETON_SYNTAX_ERROR;
            return false;
        } else {
            maxSig = minSig;
            for (; offset < segment.length(); offset++) {
                if (segment.charAt(offset) == u'#') {
                    maxSig++;
                } else {
                    break;
                }
            }
        }
    } else {
        // @, @@, @@@
        // throw new SkeletonSyntaxException("Invalid digits option for fraction rounder", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return false;
    }
    if (offset < segment.length()) {
        // throw new SkeletonSyntaxException("Invalid digits option for fraction rounder", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return false;
    }

    auto& oldPrecision = static_cast<const FractionPrecision&>(macros.precision);
    if (maxSig == -1) {
        macros.precision = oldPrecision.withMinDigits(minSig);
    } else {
        macros.precision = oldPrecision.withMaxDigits(maxSig);
    }
    return true;
}

void blueprint_helpers::parseIncrementOption(const StringSegment& segment, MacroProps& macros,
                                             UErrorCode& status) {
    // Need to do char <-> UChar conversion...
    U_ASSERT(U_SUCCESS(status));
    CharString buffer;
    SKELETON_UCHAR_TO_CHAR(buffer, segment.toTempUnicodeString(), 0, segment.length(), status);

    // Utilize DecimalQuantity/decNumber to parse this for us.
    DecimalQuantity dq;
    UErrorCode localStatus = U_ZERO_ERROR;
    dq.setToDecNumber({buffer.data(), buffer.length()}, localStatus);
    if (U_FAILURE(localStatus)) {
        // throw new SkeletonSyntaxException("Invalid rounding increment", segment, e);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    double increment = dq.toDouble();

    // We also need to figure out how many digits. Do a brute force string operation.
    int decimalOffset = 0;
    while (decimalOffset < segment.length() && segment.charAt(decimalOffset) != '.') {
        decimalOffset++;
    }
    if (decimalOffset == segment.length()) {
        macros.precision = Precision::increment(increment);
    } else {
        int32_t fractionLength = segment.length() - decimalOffset - 1;
        macros.precision = Precision::increment(increment).withMinFraction(fractionLength);
    }
}

void blueprint_helpers::generateIncrementOption(double increment, int32_t trailingZeros, UnicodeString& sb,
                                                UErrorCode&) {
    // Utilize DecimalQuantity/double_conversion to format this for us.
    DecimalQuantity dq;
    dq.setToDouble(increment);
    dq.roundToInfinity();
    sb.append(dq.toPlainString());

    // We might need to append extra trailing zeros for min fraction...
    if (trailingZeros > 0) {
        appendMultiple(sb, u'0', trailingZeros);
    }
}

void blueprint_helpers::parseIntegerWidthOption(const StringSegment& segment, MacroProps& macros,
                                                UErrorCode& status) {
    int32_t offset = 0;
    int32_t minInt = 0;
    int32_t maxInt;
    if (segment.charAt(0) == u'+') {
        maxInt = -1;
        offset++;
    } else {
        maxInt = 0;
    }
    for (; offset < segment.length(); offset++) {
        if (maxInt != -1 && segment.charAt(offset) == u'#') {
            maxInt++;
        } else {
            break;
        }
    }
    if (offset < segment.length()) {
        for (; offset < segment.length(); offset++) {
            if (segment.charAt(offset) == u'0') {
                minInt++;
            } else {
                break;
            }
        }
    }
    if (maxInt != -1) {
        maxInt += minInt;
    }
    if (offset < segment.length()) {
        // throw new SkeletonSyntaxException("Invalid integer width stem", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    // Use the public APIs to enforce bounds checking
    if (maxInt == -1) {
        macros.integerWidth = IntegerWidth::zeroFillTo(minInt);
    } else {
        macros.integerWidth = IntegerWidth::zeroFillTo(minInt).truncateAt(maxInt);
    }
}

void blueprint_helpers::generateIntegerWidthOption(int32_t minInt, int32_t maxInt, UnicodeString& sb,
                                                   UErrorCode&) {
    if (maxInt == -1) {
        sb.append(u'+');
    } else {
        appendMultiple(sb, u'#', maxInt - minInt);
    }
    appendMultiple(sb, u'0', minInt);
}

void blueprint_helpers::parseNumberingSystemOption(const StringSegment& segment, MacroProps& macros,
                                                   UErrorCode& status) {
    // Need to do char <-> UChar conversion...
    U_ASSERT(U_SUCCESS(status));
    CharString buffer;
    SKELETON_UCHAR_TO_CHAR(buffer, segment.toTempUnicodeString(), 0, segment.length(), status);

    NumberingSystem* ns = NumberingSystem::createInstanceByName(buffer.data(), status);
    if (ns == nullptr || U_FAILURE(status)) {
        // This is a skeleton syntax error; don't bubble up the low-level NumberingSystem error
        // throw new SkeletonSyntaxException("Unknown numbering system", segment);
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }
    macros.symbols.setTo(ns);
}

void blueprint_helpers::generateNumberingSystemOption(const NumberingSystem& ns, UnicodeString& sb,
                                                      UErrorCode&) {
    // Need to do char <-> UChar conversion...
    sb.append(UnicodeString(ns.getName(), -1, US_INV));
}

void blueprint_helpers::parseScaleOption(const StringSegment& segment, MacroProps& macros,
                                              UErrorCode& status) {
    // Need to do char <-> UChar conversion...
    U_ASSERT(U_SUCCESS(status));
    CharString buffer;
    SKELETON_UCHAR_TO_CHAR(buffer, segment.toTempUnicodeString(), 0, segment.length(), status);

    LocalPointer<DecNum> decnum(new DecNum(), status);
    if (U_FAILURE(status)) { return; }
    decnum->setTo({buffer.data(), buffer.length()}, status);
    if (U_FAILURE(status)) {
        // This is a skeleton syntax error; don't let the low-level decnum error bubble up
        status = U_NUMBER_SKELETON_SYNTAX_ERROR;
        return;
    }

    // NOTE: The constructor will optimize the decnum for us if possible.
    macros.scale = {0, decnum.orphan()};
}

void blueprint_helpers::generateScaleOption(int32_t magnitude, const DecNum* arbitrary, UnicodeString& sb,
                                            UErrorCode& status) {
    // Utilize DecimalQuantity/double_conversion to format this for us.
    DecimalQuantity dq;
    if (arbitrary != nullptr) {
        dq.setToDecNum(*arbitrary, status);
        if (U_FAILURE(status)) { return; }
    } else {
        dq.setToInt(1);
    }
    dq.adjustMagnitude(magnitude);
    dq.roundToInfinity();
    sb.append(dq.toPlainString());
}


bool GeneratorHelpers::notation(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (macros.notation.fType == Notation::NTN_COMPACT) {
        UNumberCompactStyle style = macros.notation.fUnion.compactStyle;
        if (style == UNumberCompactStyle::UNUM_LONG) {
            sb.append(u"compact-long", -1);
            return true;
        } else if (style == UNumberCompactStyle::UNUM_SHORT) {
            sb.append(u"compact-short", -1);
            return true;
        } else {
            // Compact notation generated from custom data (not supported in skeleton)
            // The other compact notations are literals
            status = U_UNSUPPORTED_ERROR;
            return false;
        }
    } else if (macros.notation.fType == Notation::NTN_SCIENTIFIC) {
        const Notation::ScientificSettings& impl = macros.notation.fUnion.scientific;
        if (impl.fEngineeringInterval == 3) {
            sb.append(u"engineering", -1);
        } else {
            sb.append(u"scientific", -1);
        }
        if (impl.fMinExponentDigits > 1) {
            sb.append(u'/');
            blueprint_helpers::generateExponentWidthOption(impl.fMinExponentDigits, sb, status);
            if (U_FAILURE(status)) {
                return false;
            }
        }
        if (impl.fExponentSignDisplay != UNUM_SIGN_AUTO) {
            sb.append(u'/');
            enum_to_stem_string::signDisplay(impl.fExponentSignDisplay, sb);
        }
        return true;
    } else {
        // Default value is not shown in normalized form
        return false;
    }
}

bool GeneratorHelpers::unit(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (utils::unitIsCurrency(macros.unit)) {
        sb.append(u"currency/", -1);
        CurrencyUnit currency(macros.unit, status);
        if (U_FAILURE(status)) {
            return false;
        }
        blueprint_helpers::generateCurrencyOption(currency, sb, status);
        return true;
    } else if (utils::unitIsNoUnit(macros.unit)) {
        if (utils::unitIsPercent(macros.unit)) {
            sb.append(u"percent", -1);
            return true;
        } else if (utils::unitIsPermille(macros.unit)) {
            sb.append(u"permille", -1);
            return true;
        } else {
            // Default value is not shown in normalized form
            return false;
        }
    } else {
        sb.append(u"measure-unit/", -1);
        blueprint_helpers::generateMeasureUnitOption(macros.unit, sb, status);
        return true;
    }
}

bool GeneratorHelpers::perUnit(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    // Per-units are currently expected to be only MeasureUnits.
    if (utils::unitIsNoUnit(macros.perUnit)) {
        if (utils::unitIsPercent(macros.perUnit) || utils::unitIsPermille(macros.perUnit)) {
            status = U_UNSUPPORTED_ERROR;
            return false;
        } else {
            // Default value: ok to ignore
            return false;
        }
    } else if (utils::unitIsCurrency(macros.perUnit)) {
        status = U_UNSUPPORTED_ERROR;
        return false;
    } else {
        sb.append(u"per-measure-unit/", -1);
        blueprint_helpers::generateMeasureUnitOption(macros.perUnit, sb, status);
        return true;
    }
}

bool GeneratorHelpers::precision(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (macros.precision.fType == Precision::RND_NONE) {
        sb.append(u"precision-unlimited", -1);
    } else if (macros.precision.fType == Precision::RND_FRACTION) {
        const Precision::FractionSignificantSettings& impl = macros.precision.fUnion.fracSig;
        blueprint_helpers::generateFractionStem(impl.fMinFrac, impl.fMaxFrac, sb, status);
    } else if (macros.precision.fType == Precision::RND_SIGNIFICANT) {
        const Precision::FractionSignificantSettings& impl = macros.precision.fUnion.fracSig;
        blueprint_helpers::generateDigitsStem(impl.fMinSig, impl.fMaxSig, sb, status);
    } else if (macros.precision.fType == Precision::RND_FRACTION_SIGNIFICANT) {
        const Precision::FractionSignificantSettings& impl = macros.precision.fUnion.fracSig;
        blueprint_helpers::generateFractionStem(impl.fMinFrac, impl.fMaxFrac, sb, status);
        sb.append(u'/');
        if (impl.fMinSig == -1) {
            blueprint_helpers::generateDigitsStem(1, impl.fMaxSig, sb, status);
        } else {
            blueprint_helpers::generateDigitsStem(impl.fMinSig, -1, sb, status);
        }
    } else if (macros.precision.fType == Precision::RND_INCREMENT
            || macros.precision.fType == Precision::RND_INCREMENT_ONE
            || macros.precision.fType == Precision::RND_INCREMENT_FIVE) {
        const Precision::IncrementSettings& impl = macros.precision.fUnion.increment;
        sb.append(u"precision-increment/", -1);
        blueprint_helpers::generateIncrementOption(
                impl.fIncrement,
                impl.fMinFrac - impl.fMaxFrac,
                sb,
                status);
    } else if (macros.precision.fType == Precision::RND_CURRENCY) {
        UCurrencyUsage usage = macros.precision.fUnion.currencyUsage;
        if (usage == UCURR_USAGE_STANDARD) {
            sb.append(u"precision-currency-standard", -1);
        } else {
            sb.append(u"precision-currency-cash", -1);
        }
    } else {
        // Bogus or Error
        return false;
    }

    // NOTE: Always return true for rounding because the default value depends on other options.
    return true;
}

bool GeneratorHelpers::roundingMode(const MacroProps& macros, UnicodeString& sb, UErrorCode&) {
    if (macros.roundingMode == kDefaultMode) {
        return false; // Default
    }
    enum_to_stem_string::roundingMode(macros.roundingMode, sb);
    return true;
}

bool GeneratorHelpers::grouping(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (macros.grouper.isBogus()) {
        return false; // No value
    } else if (macros.grouper.fStrategy == UNUM_GROUPING_COUNT) {
        status = U_UNSUPPORTED_ERROR;
        return false;
    } else if (macros.grouper.fStrategy == UNUM_GROUPING_AUTO) {
        return false; // Default value
    } else {
        enum_to_stem_string::groupingStrategy(macros.grouper.fStrategy, sb);
        return true;
    }
}

bool GeneratorHelpers::integerWidth(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (macros.integerWidth.fHasError || macros.integerWidth.isBogus() ||
        macros.integerWidth == IntegerWidth::standard()) {
        // Error or Default
        return false;
    }
    sb.append(u"integer-width/", -1);
    blueprint_helpers::generateIntegerWidthOption(
            macros.integerWidth.fUnion.minMaxInt.fMinInt,
            macros.integerWidth.fUnion.minMaxInt.fMaxInt,
            sb,
            status);
    return true;
}

bool GeneratorHelpers::symbols(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (macros.symbols.isNumberingSystem()) {
        const NumberingSystem& ns = *macros.symbols.getNumberingSystem();
        if (uprv_strcmp(ns.getName(), "latn") == 0) {
            sb.append(u"latin", -1);
        } else {
            sb.append(u"numbering-system/", -1);
            blueprint_helpers::generateNumberingSystemOption(ns, sb, status);
        }
        return true;
    } else if (macros.symbols.isDecimalFormatSymbols()) {
        status = U_UNSUPPORTED_ERROR;
        return false;
    } else {
        // No custom symbols
        return false;
    }
}

bool GeneratorHelpers::unitWidth(const MacroProps& macros, UnicodeString& sb, UErrorCode&) {
    if (macros.unitWidth == UNUM_UNIT_WIDTH_SHORT || macros.unitWidth == UNUM_UNIT_WIDTH_COUNT) {
        return false; // Default or Bogus
    }
    enum_to_stem_string::unitWidth(macros.unitWidth, sb);
    return true;
}

bool GeneratorHelpers::sign(const MacroProps& macros, UnicodeString& sb, UErrorCode&) {
    if (macros.sign == UNUM_SIGN_AUTO || macros.sign == UNUM_SIGN_COUNT) {
        return false; // Default or Bogus
    }
    enum_to_stem_string::signDisplay(macros.sign, sb);
    return true;
}

bool GeneratorHelpers::decimal(const MacroProps& macros, UnicodeString& sb, UErrorCode&) {
    if (macros.decimal == UNUM_DECIMAL_SEPARATOR_AUTO || macros.decimal == UNUM_DECIMAL_SEPARATOR_COUNT) {
        return false; // Default or Bogus
    }
    enum_to_stem_string::decimalSeparatorDisplay(macros.decimal, sb);
    return true;
}

bool GeneratorHelpers::scale(const MacroProps& macros, UnicodeString& sb, UErrorCode& status) {
    if (!macros.scale.isValid()) {
        return false; // Default or Bogus
    }
    sb.append(u"scale/", -1);
    blueprint_helpers::generateScaleOption(
            macros.scale.fMagnitude,
            macros.scale.fArbitrary,
            sb,
            status);
    return true;
}


#endif /* #if !UCONFIG_NO_FORMATTING */
