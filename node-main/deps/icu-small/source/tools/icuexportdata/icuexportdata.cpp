// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include "unicode/localpointer.h"
#include "unicode/umachine.h"
#include "unicode/unistr.h"
#include "unicode/urename.h"
#include "unicode/uset.h"
#include <vector>
#include <algorithm>
#include "toolutil.h"
#include "uoptions.h"
#include "cmemory.h"
#include "charstr.h"
#include "cstring.h"
#include "unicode/uchar.h"
#include "unicode/errorcode.h"
#include "unicode/uniset.h"
#include "unicode/uscript.h"
#include "unicode/putil.h"
#include "unicode/umutablecptrie.h"
#include "unicode/ucharstriebuilder.h"
#include "ucase.h"
#include "unicode/normalizer2.h"
#include "normalizer2impl.h"
#include "writesrc.h"

U_NAMESPACE_USE

/*
 * Global - verbosity
 */
UBool VERBOSE = false;
UBool QUIET = false;

UBool haveCopyright = true;
UCPTrieType trieType = UCPTRIE_TYPE_SMALL;
const char* destdir = "";

// Mask constants for modified values in the Script CodePointTrie, values are logically 12-bits.
int16_t DATAEXPORT_SCRIPT_X_WITH_COMMON    = 0x0400;
int16_t DATAEXPORT_SCRIPT_X_WITH_INHERITED = 0x0800;
int16_t DATAEXPORT_SCRIPT_X_WITH_OTHER     = 0x0c00;

// TODO(ICU-21821): Replace this with a call to a library function
// This is an array of all code points with explicit scx values, and can be generated the quick and dirty
// way with this script:
//
// # <ScriptExtensions.txt python script.py
//
// import sys
// for line in sys.stdin:
//   line = line.strip()
//   if len(line) == 0 or line.startswith("#"):
//     continue
//   entry = line.split(" ")[0]
//   # Either it is a range
//   if ".." in entry:
//     split = entry.split("..")
//     start = int(split[0], 16)
//     end = int(split[1], 16)
//     # +
//     for ch in range(start, end + 1):
//       print("0x%04x, " % ch, end="")
//   # or a single code point
//   else:
//     print("0x%s, " % entry.lower(), end="")

int32_t scxCodePoints[] = {
    0x00b7, 0x02bc, 0x02c7, 0x02c9, 0x02ca, 0x02cb, 0x02cd, 0x02d7, 0x02d9, 0x0300, 0x0301, 0x0302,
    0x0303, 0x0304, 0x0305, 0x0306, 0x0307, 0x0308, 0x0309, 0x030a, 0x030b, 0x030c, 0x030d, 0x030e,
    0x0310, 0x0311, 0x0313, 0x0320, 0x0323, 0x0324, 0x0325, 0x032d, 0x032e, 0x0330, 0x0331, 0x0342,
    0x0345, 0x0358, 0x035e, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369, 0x036a, 0x036b,
    0x036c, 0x036d, 0x036e, 0x036f, 0x0374, 0x0375, 0x0483, 0x0484, 0x0485, 0x0486, 0x0487, 0x0589,
    0x060c, 0x061b, 0x061c, 0x061f, 0x0640, 0x064b, 0x064c, 0x064d, 0x064e, 0x064f, 0x0650, 0x0651,
    0x0652, 0x0653, 0x0654, 0x0655, 0x0660, 0x0661, 0x0662, 0x0663, 0x0664, 0x0665, 0x0666, 0x0667,
    0x0668, 0x0669, 0x0670, 0x06d4, 0x0951, 0x0952, 0x0964, 0x0965, 0x0966, 0x0967, 0x0968, 0x0969,
    0x096a, 0x096b, 0x096c, 0x096d, 0x096e, 0x096f, 0x09e6, 0x09e7, 0x09e8, 0x09e9, 0x09ea, 0x09eb,
    0x09ec, 0x09ed, 0x09ee, 0x09ef, 0x0a66, 0x0a67, 0x0a68, 0x0a69, 0x0a6a, 0x0a6b, 0x0a6c, 0x0a6d,
    0x0a6e, 0x0a6f, 0x0ae6, 0x0ae7, 0x0ae8, 0x0ae9, 0x0aea, 0x0aeb, 0x0aec, 0x0aed, 0x0aee, 0x0aef,
    0x0be6, 0x0be7, 0x0be8, 0x0be9, 0x0bea, 0x0beb, 0x0bec, 0x0bed, 0x0bee, 0x0bef, 0x0bf0, 0x0bf1,
    0x0bf2, 0x0bf3, 0x0ce6, 0x0ce7, 0x0ce8, 0x0ce9, 0x0cea, 0x0ceb, 0x0cec, 0x0ced, 0x0cee, 0x0cef,
    0x1040, 0x1041, 0x1042, 0x1043, 0x1044, 0x1045, 0x1046, 0x1047, 0x1048, 0x1049, 0x10fb, 0x16eb,
    0x16ec, 0x16ed, 0x1735, 0x1736, 0x1802, 0x1803, 0x1805, 0x1cd0, 0x1cd1, 0x1cd2, 0x1cd3, 0x1cd4,
    0x1cd5, 0x1cd6, 0x1cd7, 0x1cd8, 0x1cd9, 0x1cda, 0x1cdb, 0x1cdc, 0x1cdd, 0x1cde, 0x1cdf, 0x1ce0,
    0x1ce1, 0x1ce2, 0x1ce3, 0x1ce4, 0x1ce5, 0x1ce6, 0x1ce7, 0x1ce8, 0x1ce9, 0x1cea, 0x1ceb, 0x1cec,
    0x1ced, 0x1cee, 0x1cef, 0x1cf0, 0x1cf1, 0x1cf2, 0x1cf3, 0x1cf4, 0x1cf5, 0x1cf6, 0x1cf7, 0x1cf8,
    0x1cf9, 0x1cfa, 0x1dc0, 0x1dc1, 0x1df8, 0x1dfa, 0x202f, 0x204f, 0x205a, 0x205d, 0x20f0, 0x2e17,
    0x2e30, 0x2e31, 0x2e3c, 0x2e41, 0x2e43, 0x2ff0, 0x2ff1, 0x2ff2, 0x2ff3, 0x2ff4, 0x2ff5, 0x2ff6,
    0x2ff7, 0x2ff8, 0x2ff9, 0x2ffa, 0x2ffb, 0x2ffc, 0x2ffd, 0x2ffe, 0x2fff, 0x3001, 0x3002, 0x3003,
    0x3006, 0x3008, 0x3009, 0x300a, 0x300b, 0x300c, 0x300d, 0x300e, 0x300f, 0x3010, 0x3011, 0x3013,
    0x3014, 0x3015, 0x3016, 0x3017, 0x3018, 0x3019, 0x301a, 0x301b, 0x301c, 0x301d, 0x301e, 0x301f,
    0x302a, 0x302b, 0x302c, 0x302d, 0x3030, 0x3031, 0x3032, 0x3033, 0x3034, 0x3035, 0x3037, 0x303c,
    0x303d, 0x303e, 0x303f, 0x3099, 0x309a, 0x309b, 0x309c, 0x30a0, 0x30fb, 0x30fc, 0x3190, 0x3191,
    0x3192, 0x3193, 0x3194, 0x3195, 0x3196, 0x3197, 0x3198, 0x3199, 0x319a, 0x319b, 0x319c, 0x319d,
    0x319e, 0x319f, 0x31c0, 0x31c1, 0x31c2, 0x31c3, 0x31c4, 0x31c5, 0x31c6, 0x31c7, 0x31c8, 0x31c9,
    0x31ca, 0x31cb, 0x31cc, 0x31cd, 0x31ce, 0x31cf, 0x31d0, 0x31d1, 0x31d2, 0x31d3, 0x31d4, 0x31d5,
    0x31d6, 0x31d7, 0x31d8, 0x31d9, 0x31da, 0x31db, 0x31dc, 0x31dd, 0x31de, 0x31df, 0x31e0, 0x31e1,
    0x31e2, 0x31e3, 0x31e4, 0x31e5, 0x31ef, 0x3220, 0x3221, 0x3222, 0x3223, 0x3224, 0x3225, 0x3226,
    0x3227, 0x3228, 0x3229, 0x322a, 0x322b, 0x322c, 0x322d, 0x322e, 0x322f, 0x3230, 0x3231, 0x3232,
    0x3233, 0x3234, 0x3235, 0x3236, 0x3237, 0x3238, 0x3239, 0x323a, 0x323b, 0x323c, 0x323d, 0x323e,
    0x323f, 0x3240, 0x3241, 0x3242, 0x3243, 0x3244, 0x3245, 0x3246, 0x3247, 0x3280, 0x3281, 0x3282,
    0x3283, 0x3284, 0x3285, 0x3286, 0x3287, 0x3288, 0x3289, 0x328a, 0x328b, 0x328c, 0x328d, 0x328e,
    0x328f, 0x3290, 0x3291, 0x3292, 0x3293, 0x3294, 0x3295, 0x3296, 0x3297, 0x3298, 0x3299, 0x329a,
    0x329b, 0x329c, 0x329d, 0x329e, 0x329f, 0x32a0, 0x32a1, 0x32a2, 0x32a3, 0x32a4, 0x32a5, 0x32a6,
    0x32a7, 0x32a8, 0x32a9, 0x32aa, 0x32ab, 0x32ac, 0x32ad, 0x32ae, 0x32af, 0x32b0, 0x32c0, 0x32c1,
    0x32c2, 0x32c3, 0x32c4, 0x32c5, 0x32c6, 0x32c7, 0x32c8, 0x32c9, 0x32ca, 0x32cb, 0x32ff, 0x3358,
    0x3359, 0x335a, 0x335b, 0x335c, 0x335d, 0x335e, 0x335f, 0x3360, 0x3361, 0x3362, 0x3363, 0x3364,
    0x3365, 0x3366, 0x3367, 0x3368, 0x3369, 0x336a, 0x336b, 0x336c, 0x336d, 0x336e, 0x336f, 0x3370,
    0x337b, 0x337c, 0x337d, 0x337e, 0x337f, 0x33e0, 0x33e1, 0x33e2, 0x33e3, 0x33e4, 0x33e5, 0x33e6,
    0x33e7, 0x33e8, 0x33e9, 0x33ea, 0x33eb, 0x33ec, 0x33ed, 0x33ee, 0x33ef, 0x33f0, 0x33f1, 0x33f2,
    0x33f3, 0x33f4, 0x33f5, 0x33f6, 0x33f7, 0x33f8, 0x33f9, 0x33fa, 0x33fb, 0x33fc, 0x33fd, 0x33fe,
    0xa66f, 0xa700, 0xa701, 0xa702, 0xa703, 0xa704, 0xa705, 0xa706, 0xa707, 0xa830, 0xa831, 0xa832,
    0xa833, 0xa834, 0xa835, 0xa836, 0xa837, 0xa838, 0xa839, 0xa8f1, 0xa8f3, 0xa92e, 0xa9cf, 0xfd3e,
    0xfd3f, 0xfdf2, 0xfdfd, 0xfe45, 0xfe46, 0xff61, 0xff62, 0xff63, 0xff64, 0xff65, 0xff70, 0xff9e,
    0xff9f, 0x10100, 0x10101, 0x10102, 0x10107, 0x10108, 0x10109, 0x1010a, 0x1010b, 0x1010c, 0x1010d,
    0x1010e, 0x1010f, 0x10110, 0x10111, 0x10112, 0x10113, 0x10114, 0x10115, 0x10116, 0x10117, 0x10118,
    0x10119, 0x1011a, 0x1011b, 0x1011c, 0x1011d, 0x1011e, 0x1011f, 0x10120, 0x10121, 0x10122, 0x10123,
    0x10124, 0x10125, 0x10126, 0x10127, 0x10128, 0x10129, 0x1012a, 0x1012b, 0x1012c, 0x1012d, 0x1012e,
    0x1012f, 0x10130, 0x10131, 0x10132, 0x10133, 0x10137, 0x10138, 0x10139, 0x1013a, 0x1013b, 0x1013c,
    0x1013d, 0x1013e, 0x1013f, 0x102e0, 0x102e1, 0x102e2, 0x102e3, 0x102e4, 0x102e5, 0x102e6, 0x102e7,
    0x102e8, 0x102e9, 0x102ea, 0x102eb, 0x102ec, 0x102ed, 0x102ee, 0x102ef, 0x102f0, 0x102f1, 0x102f2,
    0x102f3, 0x102f4, 0x102f5, 0x102f6, 0x102f7, 0x102f8, 0x102f9, 0x102fa, 0x102fb, 0x10af2, 0x11301,
    0x11303, 0x1133b, 0x1133c, 0x11fd0, 0x11fd1, 0x11fd3, 0x1bca0, 0x1bca1, 0x1bca2, 0x1bca3, 0x1d360,
    0x1d361, 0x1d362, 0x1d363, 0x1d364, 0x1d365, 0x1d366, 0x1d367, 0x1d368, 0x1d369, 0x1d36a, 0x1d36b,
    0x1d36c, 0x1d36d, 0x1d36e, 0x1d36f, 0x1d370, 0x1d371, 0x1f250, 0x1f251,
};

void handleError(ErrorCode& status, int line, const char* context) {
    if (status.isFailure()) {
        std::cerr << "Error[" << line << "]: " << context << ": " << status.errorName() << std::endl;
        exit(status.reset());
    }
}

class PropertyValueNameGetter : public ValueNameGetter {
public:
    PropertyValueNameGetter(UProperty prop) : property(prop) {}
    ~PropertyValueNameGetter() override;
    const char *getName(uint32_t value) override {
        return u_getPropertyValueName(property, value, U_SHORT_PROPERTY_NAME);
    }

private:
    UProperty property;
};

PropertyValueNameGetter::~PropertyValueNameGetter() {}

// Dump an aliases = [...] key for properties with aliases
void dumpPropertyAliases(UProperty uproperty, FILE* f) {
    int i = U_LONG_PROPERTY_NAME + 1;

    while(true) {
        // The API works by having extra names after U_LONG_PROPERTY_NAME, sequentially,
        // and returning null after that
        const char* alias = u_getPropertyName(uproperty, static_cast<UPropertyNameChoice>(i));
        if (!alias) {
            break;
        }
        if (i == U_LONG_PROPERTY_NAME + 1) {
            fprintf(f, "aliases = [\"%s\"", alias);
        } else {
            fprintf(f, ", \"%s\"", alias);
        }
        i++;
    }
    if (i != U_LONG_PROPERTY_NAME + 1) {
        fprintf(f, "]\n");
    }
}

void dumpBinaryProperty(UProperty uproperty, FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpBinaryProperty");
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    const USet* uset = u_getBinaryPropertySet(uproperty, status);
    handleError(status, __LINE__, fullPropName);

    fputs("[[binary_property]]\n", f);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) fprintf(f, "short_name = \"%s\"\n", shortPropName);
    fprintf(f, "uproperty_discr = 0x%X\n", uproperty);
    dumpPropertyAliases(uproperty, f);
    usrc_writeUnicodeSet(f, uset, UPRV_TARGET_SYNTAX_TOML);
}

// If the value exists, dump an indented entry of the format
// `"  {discr = <discriminant>, long = <longname>, short = <shortname>, aliases = [<aliases>]},"`
void dumpValueEntry(UProperty uproperty, int v, bool is_mask, FILE* f) {
    const char* fullValueName = u_getPropertyValueName(uproperty, v, U_LONG_PROPERTY_NAME);
    const char* shortValueName = u_getPropertyValueName(uproperty, v, U_SHORT_PROPERTY_NAME);
    if (!fullValueName) {
        return;
    }
    if (is_mask) {
        fprintf(f, "  {discr = 0x%X", v);
    } else {
        fprintf(f, "  {discr = %i", v);
    }
    fprintf(f, ", long = \"%s\"", fullValueName);
    if (shortValueName) {
        fprintf(f, ", short = \"%s\"", shortValueName);
    }
    int i = U_LONG_PROPERTY_NAME + 1;
    while(true) {
        // The API works by having extra names after U_LONG_PROPERTY_NAME, sequentially,
        // and returning null after that
        const char* alias = u_getPropertyValueName(uproperty, v, static_cast<UPropertyNameChoice>(i));
        if (!alias) {
            break;
        }
        if (i == U_LONG_PROPERTY_NAME + 1) {
            fprintf(f, ", aliases = [\"%s\"", alias);
        } else {
            fprintf(f, ", \"%s\"", alias);
        }
        i++;
    }
    if (i != U_LONG_PROPERTY_NAME + 1) {
        fprintf(f, "]");
    }
    fprintf(f, "},\n");
}

void dumpEnumeratedProperty(UProperty uproperty, FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpEnumeratedProperty");
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    const UCPMap* umap = u_getIntPropertyMap(uproperty, status);
    handleError(status, __LINE__, fullPropName);

    fputs("[[enum_property]]\n", f);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) fprintf(f, "short_name = \"%s\"\n", shortPropName);
    fprintf(f, "uproperty_discr = 0x%X\n", uproperty);
    dumpPropertyAliases(uproperty, f);

    int32_t minValue = u_getIntPropertyMinValue(uproperty);
    U_ASSERT(minValue >= 0);
    int32_t maxValue = u_getIntPropertyMaxValue(uproperty);
    U_ASSERT(maxValue >= 0);

    fprintf(f, "values = [\n");
    for (int v = minValue; v <= maxValue; v++) {
        dumpValueEntry(uproperty, v, false, f);
    }
    fprintf(f, "]\n");

    PropertyValueNameGetter valueNameGetter(uproperty);
    usrc_writeUCPMap(f, umap, &valueNameGetter, UPRV_TARGET_SYNTAX_TOML);
    fputs("\n", f);


    UCPTrieValueWidth width = UCPTRIE_VALUE_BITS_32;
    if (maxValue <= 0xff) {
        width = UCPTRIE_VALUE_BITS_8;
    } else if (maxValue <= 0xffff) {
        width = UCPTRIE_VALUE_BITS_16;
    }
    LocalUMutableCPTriePointer builder(umutablecptrie_fromUCPMap(umap, status));
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        width,
        status));
    handleError(status, __LINE__, fullPropName);

    fputs("[enum_property.code_point_trie]\n", f);
    usrc_writeUCPTrie(f, shortPropName, utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
}

/*
* Export Bidi_Mirroring_Glyph values (code points) in a similar way to how enumerated
* properties are dumped to file.
* Note: the data will store 0 for code points without a value defined for
* Bidi_Mirroring_Glyph.
*/
void dumpBidiMirroringGlyph(FILE* f) {
    UProperty uproperty = UCHAR_BIDI_MIRRORING_GLYPH;
    IcuToolErrorCode status("icuexportdata: dumpBidiMirroringGlyph");
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    handleError(status, __LINE__, fullPropName);

    // Store 21-bit code point as is
    UCPTrieValueWidth width = UCPTRIE_VALUE_BITS_32;

    // note: unlike dumpEnumeratedProperty, which can get inversion map data using
    // u_getIntPropertyMap(uproperty), the only reliable way to get Bidi_Mirroring_Glyph
    // is to use u_charMirror(cp) over the code point space.
    LocalUMutableCPTriePointer builder(umutablecptrie_open(0, 0, status));
    for(UChar32 c = UCHAR_MIN_VALUE; c <= UCHAR_MAX_VALUE; c++) {
        UChar32 mirroringGlyph = u_charMirror(c);
        // The trie builder code throws an error when it cannot compress the data sufficiently.
        // Therefore, when the value is undefined for a code point, keep a 0 in the trie
        // instead of the ICU API behavior of returning the code point value. Using 0
        // results in a relatively significant space savings by not including redundant data.
        if (c != mirroringGlyph) {
            umutablecptrie_set(builder.getAlias(), c, mirroringGlyph, status);
        }
    }

    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        width,
        status));
    handleError(status, __LINE__, fullPropName);

    // currently a trie and inversion map are the same (as relied upon in characterproperties.cpp)
    const UCPMap* umap = reinterpret_cast<UCPMap *>(utrie.getAlias());

    fputs("[[enum_property]]\n", f);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) {
        fprintf(f, "short_name = \"%s\"\n", shortPropName);
    }
    fprintf(f, "uproperty_discr = 0x%X\n", uproperty);
    dumpPropertyAliases(uproperty, f);

    usrc_writeUCPMap(f, umap, nullptr, UPRV_TARGET_SYNTAX_TOML);
    fputs("\n", f);

    fputs("[enum_property.code_point_trie]\n", f);
    usrc_writeUCPTrie(f, shortPropName, utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
}

// After printing property value `v`, print `mask` if and only if `mask` comes immediately
// after the property in the listing
void maybeDumpMaskValue(UProperty uproperty, uint32_t v, uint32_t mask, FILE* f) {
    if (U_MASK(v) < mask && U_MASK(v + 1) > mask)
        dumpValueEntry(uproperty, mask, true, f);
}

void dumpGeneralCategoryMask(FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpGeneralCategoryMask");
    UProperty uproperty = UCHAR_GENERAL_CATEGORY_MASK;

    fputs("[[mask_property]]\n", f);
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) fprintf(f, "short_name = \"%s\"\n", shortPropName);
    fprintf(f, "uproperty_discr = 0x%X\n", uproperty);
    dumpPropertyAliases(uproperty, f);


    fprintf(f, "mask_for = \"General_Category\"\n");
    int32_t minValue = u_getIntPropertyMinValue(UCHAR_GENERAL_CATEGORY);
    U_ASSERT(minValue >= 0);
    int32_t maxValue = u_getIntPropertyMaxValue(UCHAR_GENERAL_CATEGORY);
    U_ASSERT(maxValue >= 0);

    fprintf(f, "values = [\n");
    for (int32_t v = minValue; v <= maxValue; v++) {
        dumpValueEntry(uproperty, U_MASK(v), true, f);

        // We want to dump these masks "in order", which means they
        // should come immediately after every property they contain
        maybeDumpMaskValue(uproperty, v, U_GC_L_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_LC_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_M_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_N_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_Z_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_C_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_P_MASK, f);
        maybeDumpMaskValue(uproperty, v, U_GC_S_MASK, f);
    }
    fprintf(f, "]\n");
}

void dumpScriptExtensions(FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpScriptExtensions");

    fputs("[[script_extensions]]\n", f);
    const char* scxFullPropName = u_getPropertyName(UCHAR_SCRIPT_EXTENSIONS, U_LONG_PROPERTY_NAME);
    const char* scxShortPropName = u_getPropertyName(UCHAR_SCRIPT_EXTENSIONS, U_SHORT_PROPERTY_NAME);
    fprintf(f, "long_name = \"%s\"\n", scxFullPropName);
    if (scxShortPropName) fprintf(f, "short_name = \"%s\"\n", scxShortPropName);
    fprintf(f, "uproperty_discr = 0x%X\n", UCHAR_SCRIPT_EXTENSIONS);
    dumpPropertyAliases(UCHAR_SCRIPT_EXTENSIONS, f);

    // We want to use 16 bits for our exported trie of sc/scx data because we
    // need 12 bits to match the 12 bits of data stored for sc/scx in the trie
    // in the uprops.icu data file.
    UCPTrieValueWidth scWidth = UCPTRIE_VALUE_BITS_16;

    // Create a mutable UCPTrie builder populated with Script property values data.
    const UCPMap* scInvMap = u_getIntPropertyMap(UCHAR_SCRIPT, status);
    handleError(status, __LINE__, scxFullPropName);
    LocalUMutableCPTriePointer builder(umutablecptrie_fromUCPMap(scInvMap, status));
    handleError(status, __LINE__, scxFullPropName);

    // The values for the output scx companion array.
    // Invariant is that all subvectors are distinct.
    std::vector< std::vector<uint16_t> > outputDedupVec;

    // The sc/scx companion array is an array of arrays (of script codes)
    fputs("script_code_array = [\n", f);
    for(const UChar32 cp : scxCodePoints) {
        // Get the Script value
        uint32_t scVal = umutablecptrie_get(builder.getAlias(), cp);
        // Get the Script_Extensions value (array of Script codes)
        const int32_t SCX_ARRAY_CAPACITY = 32;
        UScriptCode scxValArray[SCX_ARRAY_CAPACITY];
        int32_t numScripts = uscript_getScriptExtensions(cp, scxValArray, SCX_ARRAY_CAPACITY, status);
        handleError(status, __LINE__, scxFullPropName);

        // Convert the scx array into a vector
        std::vector<uint16_t> scxValVec;
        for(int i = 0; i < numScripts; i++) {
            scxValVec.push_back(scxValArray[i]);
        }
        // Ensure that it is sorted
        std::sort(scxValVec.begin(), scxValVec.end());
        // Copy the Script value into the first position of the scx array only
        // if we have the "other" case (Script value is not Common nor Inherited).
        // This offers faster access when users want only the Script value.
        if (scVal != USCRIPT_COMMON && scVal != USCRIPT_INHERITED) {
            scxValVec.insert(scxValVec.begin(), scVal);
        }

        // See if there is already an scx value array matching the newly built one.
        // If there is, then use its index.
        // If not, then append the new value array.
        bool isScxValUnique = true;
        size_t outputIndex = 0;
        for (outputIndex = 0; outputIndex < outputDedupVec.size(); outputIndex++) {
            if (outputDedupVec[outputIndex] == scxValVec) {
                isScxValUnique = false;
                break;
            }
        }

        if (isScxValUnique) {
            outputDedupVec.push_back(scxValVec);
            usrc_writeArray(f, "  [", scxValVec.data(), 16, scxValVec.size(), "    ", "],\n");
        }

        // We must update the value in the UCPTrie for the code point to contain:
        // 9..0 the Script code in the lower 10 bits when 11..10 is 0, else it is
        //   the index into the companion array
        // 11..10 the same higher-order 2 bits in the trie in uprops.icu indicating whether
        //   3: other
        //   2: Script=Inherited
        //   1: Script=Common
        //   0: Script=value in 9..0 (N/A because we are in this loop to create the companion array for non-0 cases)
        uint16_t mask = 0;
        if (scVal == USCRIPT_COMMON) {
            mask = DATAEXPORT_SCRIPT_X_WITH_COMMON;
        } else if (scVal == USCRIPT_INHERITED) {
            mask = DATAEXPORT_SCRIPT_X_WITH_INHERITED;
        } else {
            mask = DATAEXPORT_SCRIPT_X_WITH_OTHER;
        }

        // The new trie value is the index into the new array with the high order bits set
        uint32_t newScVal = outputIndex | mask;

        // Update the code point in the mutable trie builder with the trie value
        umutablecptrie_set(builder.getAlias(), cp, newScVal, status);
        handleError(status, __LINE__, scxFullPropName);
    }
    fputs("]\n\n", f);  // Print the TOML close delimiter for the outer array.

    // Convert from mutable trie builder to immutable trie.
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        scWidth,
        status));
    handleError(status, __LINE__, scxFullPropName);

    fputs("[script_extensions.code_point_trie]\n", f);
    usrc_writeUCPTrie(f, scxShortPropName, utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
}

FILE* prepareOutputFile(const char* basename) {
    IcuToolErrorCode status("icuexportdata");
    CharString outFileName;
    if (destdir != nullptr && *destdir != 0) {
        outFileName.append(destdir, status).ensureEndsWithFileSeparator(status);
    }
    outFileName.append(basename, status);
    outFileName.append(".toml", status);
    handleError(status, __LINE__, basename);

    FILE* f = fopen(outFileName.data(), "w");
    if (f == nullptr) {
        std::cerr << "Unable to open file: " << outFileName.data() << std::endl;
        exit(U_FILE_ACCESS_ERROR);
    }
    if (!QUIET) {
        std::cout << "Writing to: " << outFileName.data() << std::endl;
    }

    if (haveCopyright) {
        usrc_writeCopyrightHeader(f, "#", 2021);
    }
    usrc_writeFileNameGeneratedBy(f, "#", basename, "icuexportdata.cpp");

    return f;
}

#if !UCONFIG_NO_NORMALIZATION

class PendingDescriptor {
public:
    UChar32 scalar;
    uint32_t descriptorOrFlags;
    // If false, we use the above fields only. If true, descriptor only
    // contains the two highest-bit flags and the rest is computed later
    // from the fields below.
    UBool complex;
    UBool supplementary;
    UBool onlyNonStartersInTrail;
    uint32_t len;
    uint32_t offset;

    PendingDescriptor(UChar32 scalar, uint32_t descriptor);
    PendingDescriptor(UChar32 scalar, uint32_t flags, UBool supplementary, UBool onlyNonStartersInTrail, uint32_t len, uint32_t offset);
};

PendingDescriptor::PendingDescriptor(UChar32 scalar, uint32_t descriptor)
 : scalar(scalar), descriptorOrFlags(descriptor), complex(false), supplementary(false), onlyNonStartersInTrail(false), len(0), offset(0) {}

PendingDescriptor::PendingDescriptor(UChar32 scalar, uint32_t flags, UBool supplementary, UBool onlyNonStartersInTrail, uint32_t len, uint32_t offset)
 : scalar(scalar), descriptorOrFlags(flags), complex(true), supplementary(supplementary), onlyNonStartersInTrail(onlyNonStartersInTrail), len(len), offset(offset) {}

void writeCanonicalCompositions(USet* backwardCombiningStarters) {
    IcuToolErrorCode status("icuexportdata: computeCanonicalCompositions");
    const char* basename = "compositions";
    FILE* f = prepareOutputFile(basename);

    LocalPointer<UCharsTrieBuilder> backwardBuilder(new UCharsTrieBuilder(status), status);

    const int32_t DECOMPOSITION_BUFFER_SIZE = 20;
    UChar32 utf32[DECOMPOSITION_BUFFER_SIZE];

    const Normalizer2* nfc = Normalizer2::getNFCInstance(status);
    for (UChar32 c = 0; c <= 0x10FFFF; ++c) {
        if (c >= 0xD800 && c < 0xE000) {
            // Surrogate
            continue;
        }
        UnicodeString decomposition;
        if (!nfc->getRawDecomposition(c, decomposition)) {
            continue;
        }
        int32_t len = decomposition.toUTF32(utf32, DECOMPOSITION_BUFFER_SIZE, status);
        if (len != 2) {
            continue;
        }
        UChar32 starter = utf32[0];
        UChar32 second = utf32[1];
        UChar32 composite = nfc->composePair(starter, second);
        if (composite < 0) {
            continue;
        }
        if (c != composite) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, basename);
        }
        if (!u_getCombiningClass(second)) {
            uset_add(backwardCombiningStarters, second);
        }
        if (composite >= 0xAC00 && composite <= 0xD7A3) {
            // Hangul syllable
            continue;
        }

        UnicodeString backward;
        backward.append(second);
        backward.append(starter);
        backwardBuilder->add(backward, static_cast<int32_t>(composite), status);
    }
    UnicodeString canonicalCompositionTrie;
    backwardBuilder->buildUnicodeString(USTRINGTRIE_BUILD_SMALL, canonicalCompositionTrie, status);

    usrc_writeArray(f, "compositions = [\n  ", canonicalCompositionTrie.getBuffer(), 16, canonicalCompositionTrie.length(), "  ", "\n]\n");
    fclose(f);
    handleError(status, __LINE__, basename);
}

void writeDecompositionTables(const char* basename, const uint16_t* ptr16, size_t len16, const uint32_t* ptr32, size_t len32) {
    FILE* f = prepareOutputFile(basename);
    usrc_writeArray(f, "scalars16 = [\n  ", ptr16, 16, len16, "  ", "\n]\n");
    usrc_writeArray(f, "scalars32 = [\n  ", ptr32, 32, len32, "  ", "\n]\n");
    fclose(f);
}

void pendingInsertionsToTrie(const char* basename, UMutableCPTrie* trie, const std::vector<PendingDescriptor>& pendingTrieInsertions, uint32_t baseSize16, uint32_t baseSize32, uint32_t supplementSize16) {
    IcuToolErrorCode status("icuexportdata: pendingInsertionsToTrie");
    // Iterate backwards to insert lower code points in the trie first in case it matters
    // for trie block allocation.
    for (int32_t i = pendingTrieInsertions.size() - 1; i >= 0; --i) {
        const PendingDescriptor& pending = pendingTrieInsertions[i];
        if (pending.complex) {
            uint32_t additional = 0;
            uint32_t offset = pending.offset;
            uint32_t len = pending.len;
            if (!pending.supplementary) {
                len -= 2;
                if (offset >= baseSize16) {
                    // This is a offset to supplementary 16-bit data. We have
                    // 16-bit base data and 32-bit base data before. However,
                    // the 16-bit base data length is already part of offset.
                    additional = baseSize32;
                }
            } else {
                len -= 1;
                if (offset >= baseSize32) {
                    // This is an offset to supplementary 32-bit data. We have 16-bit
                    // base data, 32-bit base data, and 16-bit supplementary data before.
                    // However, the 32-bit base data length is already part
                    // of offset.
                    additional = baseSize16 + supplementSize16;
                } else {
                    // This is an offset to 32-bit base data. We have 16-bit
                    // base data before.
                    additional = baseSize16;
                }
            }
            // +1 to make offset always non-zero
            offset += 1;
            if (offset + additional > 0xFFF) {
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }
            if (len > 7) {
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }
            umutablecptrie_set(trie, pending.scalar, pending.descriptorOrFlags | (uint32_t(pending.onlyNonStartersInTrail) << 4) | len | (offset + additional) << 16, status);
        } else {
            umutablecptrie_set(trie, pending.scalar, pending.descriptorOrFlags, status);
        }
    }
}

/// Marker that the decomposition does not round trip via NFC.
const uint32_t NON_ROUND_TRIP_MASK = (1 << 30);

/// Marker that the first character of the decomposition can combine
/// backwards.
const uint32_t BACKWARD_COMBINING_MASK = (1 << 31);

void writeDecompositionData(const char* basename, uint32_t baseSize16, uint32_t baseSize32, uint32_t supplementSize16, USet* uset, USet* reference, const std::vector<PendingDescriptor>& pendingTrieInsertions, const std::vector<PendingDescriptor>& nfdPendingTrieInsertions, char16_t passthroughCap) {
    IcuToolErrorCode status("icuexportdata: writeDecompositionData");
    FILE* f = prepareOutputFile(basename);

    // Zero is a magic number that means the character decomposes to itself.
    LocalUMutableCPTriePointer builder(umutablecptrie_open(0, 0, status));

    if (uprv_strcmp(basename, "uts46d") != 0) {
        // Make surrogates decompose to U+FFFD. Don't do this for UTS 46, since this
        // optimization is only used by the UTF-16 slice mode, and UTS 46 is not
        // supported in slice modes (which do not support ignorables).
        // Mark these as potentially backward-combining, to make lead surrogates
        // for non-BMP characters that are backward-combining count as
        // backward-combining just in case, though the backward-combiningness
        // is not actually being looked at today.
        umutablecptrie_setRange(builder.getAlias(), 0xD800, 0xDFFF, NON_ROUND_TRIP_MASK | BACKWARD_COMBINING_MASK | 0xFFFD, status);
    }

    // Add a marker value for Hangul syllables
    umutablecptrie_setRange(builder.getAlias(), 0xAC00, 0xD7A3, 1, status);

    // First put the NFD data in the trie, to be partially overwritten in the NFKD and UTS 46 cases.
    // This is easier that changing the logic that computes the pending insertions.
    pendingInsertionsToTrie(basename, builder.getAlias(), nfdPendingTrieInsertions, baseSize16, baseSize32, supplementSize16);
    pendingInsertionsToTrie(basename, builder.getAlias(), pendingTrieInsertions, baseSize16, baseSize32, supplementSize16);
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        UCPTRIE_VALUE_BITS_32,
        status));
    handleError(status, __LINE__, basename);

    // The ICU4X side has changed enough this whole block of expectation checking might be more appropriate to remove.
    if (reference) {
        if (uset_contains(reference, 0xFF9E) || uset_contains(reference, 0xFF9F) || !uset_contains(reference, 0x0345)) {
            // NFD expectations don't hold. The set must not contain the half-width
            // kana voicing marks and must contain iota subscript.
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, basename);
        }

        USet* halfWidthVoicing = uset_openEmpty();
        uset_add(halfWidthVoicing, 0xFF9E);
        uset_add(halfWidthVoicing, 0xFF9F);

        USet* iotaSubscript = uset_openEmpty();
        uset_add(iotaSubscript, 0x0345);

        USet* halfWidthCheck = uset_cloneAsThawed(uset);
        uset_removeAll(halfWidthCheck, reference);
        if (!uset_equals(halfWidthCheck, halfWidthVoicing) && !uset_isEmpty(halfWidthCheck)) {
            // The result was neither empty nor contained exactly
            // the two half-width voicing marks. The ICU4X
            // normalizer doesn't know how to deal with this case.
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, basename);
        }
        uset_close(halfWidthCheck);

        USet* iotaCheck = uset_cloneAsThawed(reference);
        uset_removeAll(iotaCheck, uset);
        if (!(uset_equals(iotaCheck, iotaSubscript)) && !uset_isEmpty(iotaCheck)) {
            // The result was neither empty nor contained exactly
            // the iota subscript. The ICU4X normalizer doesn't
            // know how to deal with this case.
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, basename);
        }

        uset_close(iotaSubscript);
        uset_close(halfWidthVoicing);
    }
    fprintf(f, "cap = 0x%X\n", passthroughCap);
    fprintf(f, "[trie]\n");
    usrc_writeUCPTrie(f, "trie", utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
    fclose(f);
    handleError(status, __LINE__, basename);
}

// Find the slice `needle` within `storage` and return its index, failing which,
// append all elements of `needle` to `storage` and return the index of it at the end.
template<typename T>
size_t findOrAppend(std::vector<T>& storage, const UChar32* needle, size_t needleLen) {
    // Last index where we might find the start of the complete needle.
    // bounds check is `i + needleLen <= storage.size()` since the inner
    // loop will range from `i` to `i + needleLen - 1` (the `-1` is why we use `<=`)
    for (size_t i = 0; i + needleLen <= storage.size(); i++) {
        for (size_t j = 0;; j++) {
            if (j == needleLen) {
                return i;  // found a match
            }
            if (storage[i + j] != static_cast<uint32_t>(needle[j])) {
                break;
            }
        }
    }
    // We didn't find anything. Append, keeping the append index in mind.
    size_t index = storage.size();
    for(size_t i = 0; i < needleLen; i++) {
        storage.push_back(static_cast<T>(needle[i]));
    }

    return index;
}


// Computes data for canonical decompositions
// See components/normalizer/trie-value-format.md in the ICU4X repo
// for documentation of the trie value format.
void computeDecompositions(const char* basename,
                           const USet* backwardCombiningStarters,
                           std::vector<uint16_t>& storage16,
                           std::vector<uint32_t>& storage32,
                           USet* decompositionStartsWithNonStarter,
                           USet* decompositionStartsWithBackwardCombiningStarter,
                           std::vector<PendingDescriptor>& pendingTrieInsertions,
                           UChar32& decompositionPassthroughBound,
                           UChar32& compositionPassthroughBound) {
    IcuToolErrorCode status("icuexportdata: computeDecompositions");
    const Normalizer2* mainNormalizer;
    const Normalizer2* nfdNormalizer = Normalizer2::getNFDInstance(status);
    const Normalizer2* nfcNormalizer = Normalizer2::getNFCInstance(status);
    FILE* f = nullptr;
    std::vector<uint32_t> nonRecursive32;
    LocalUMutableCPTriePointer nonRecursiveBuilder(umutablecptrie_open(0, 0, status));

    UBool uts46 = false;

    if (uprv_strcmp(basename, "nfkd") == 0) {
        mainNormalizer = Normalizer2::getNFKDInstance(status);
    } else if (uprv_strcmp(basename, "uts46d") == 0) {
        uts46 = true;
        mainNormalizer = Normalizer2::getInstance(nullptr, "uts46", UNORM2_COMPOSE, status);
    } else {
        mainNormalizer = nfdNormalizer;
        f = prepareOutputFile("decompositionex");
    }

    // Max length as of Unicode 14 is 4 for NFD. For NFKD the max
    // is 18 (U+FDFA; special-cased), and the next longest is 8 (U+FDFB).
    const int32_t LONGEST_ENCODABLE_LENGTH_16 = 9;
    const int32_t LONGEST_ENCODABLE_LENGTH_32 = 8;
    const int32_t DECOMPOSITION_BUFFER_SIZE = 20;
    UChar32 utf32[DECOMPOSITION_BUFFER_SIZE];
    const int32_t RAW_DECOMPOSITION_BUFFER_SIZE = 2;
    UChar32 rawUtf32[RAW_DECOMPOSITION_BUFFER_SIZE];

    // Iterate over all scalar values excluding Hangul syllables.
    //
    // We go backwards in order to better find overlapping decompositions.
    //
    // As of Unicode 14:
    // Iterate forward without overlap search:
    // nfd: 16 size: 896, 32 size: 173
    // nfkd: 16 size: 3854, 32 size: 179
    //
    // Iterate forward with overlap search:
    // nfd: 16 size: 888, 32 size: 173
    // nfkd: 16 size: 3266, 32 size: 179
    //
    // Iterate backward with overlap search:
    // nfd: 16 size: 776, 32 size: 173
    // nfkd: 16 size: 2941, 32 size: 179
    //
    // UChar32 is signed!
    for (UChar32 c = 0x10FFFF; c >= 0; --c) {
        if (c >= 0xAC00 && c <= 0xD7A3) {
            // Hangul syllable
            continue;
        }
        if (c >= 0xD800 && c < 0xE000) {
            // Surrogate
            continue;
        }
        if (c == 0xFFFD) {
            // REPLACEMENT CHARACTER
            // This character is a starter that decomposes to self,
            // so without a special case here it would end up as
            // passthrough-eligible in all normalizations forms.
            // However, in the potentially-ill-formed UTF-8 case
            // UTF-8 errors return U+FFFD from the iterator, and
            // errors need to be treated as ineligible for
            // passthrough on the slice fast path. By giving
            // U+FFFD a trie value whose flags make it ineligible
            // for passthrough avoids a specific U+FFFD branch on
            // the passthrough fast path.
            pendingTrieInsertions.push_back({c, NON_ROUND_TRIP_MASK | BACKWARD_COMBINING_MASK});
            continue;
        }
        UnicodeString src;
        UnicodeString dst;
        src.append(c);
        if (mainNormalizer != nfdNormalizer) {
            UnicodeString inter;
            mainNormalizer->normalize(src, inter, status);
            nfdNormalizer->normalize(inter, dst, status);
        } else {
            nfdNormalizer->normalize(src, dst, status);
        }

        UnicodeString nfc;
        nfcNormalizer->normalize(dst, nfc, status);
        UBool roundTripsViaCanonicalComposition = (src == nfc);

        int32_t len = dst.toUTF32(utf32, DECOMPOSITION_BUFFER_SIZE, status);

        if (!len || (len == 1 && utf32[0] == 0xFFFD && c != 0xFFFD)) {
            if (!uts46) {
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }
        }
        if (len > DECOMPOSITION_BUFFER_SIZE) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, basename);
        }
        uint8_t firstCombiningClass = u_getCombiningClass(utf32[0]);
        bool specialNonStarterDecomposition = false;
        bool startsWithBackwardCombiningStarter = false;
        if (firstCombiningClass) {
            decompositionPassthroughBound = c;
            compositionPassthroughBound = c;
            uset_add(decompositionStartsWithNonStarter, c);
            if (src != dst) {
                if (c == 0x0340 || c == 0x0341 || c == 0x0343 || c == 0x0344 || c == 0x0F73 || c == 0x0F75 || c == 0x0F81 || (c == 0xFF9E && utf32[0] == 0x3099) || (c == 0xFF9F && utf32[0] == 0x309A)) {
                    specialNonStarterDecomposition = true;
                } else {
                    // A character whose decomposition starts with a non-starter and isn't the same as the character itself and isn't already hard-coded into ICU4X.
                    status.set(U_INTERNAL_PROGRAM_ERROR);
                    handleError(status, __LINE__, basename);
                }
            }
        } else if (uset_contains(backwardCombiningStarters, utf32[0])) {
            compositionPassthroughBound = c;
            startsWithBackwardCombiningStarter = true;
            uset_add(decompositionStartsWithBackwardCombiningStarter, c);
        }
        if (mainNormalizer != nfdNormalizer) {
            UnicodeString nfd;
            nfdNormalizer->normalize(src, nfd, status);
            if (dst == nfd) {
                continue;
            }
            decompositionPassthroughBound = c;
            compositionPassthroughBound = c;
        }
        if (firstCombiningClass) {
            len = 1;
            if (specialNonStarterDecomposition) {
                // Special marker
                pendingTrieInsertions.push_back({c, NON_ROUND_TRIP_MASK | BACKWARD_COMBINING_MASK | 0xD900 | u_getCombiningClass(c)});
            } else {
                // Use the surrogate range to store the canonical combining class
                // XXX: Should non-started that decompose to self be marked as non-round-trippable in
                // case such semantics turn out to be more useful for `NON_ROUND_TRIP_MASK`?
                pendingTrieInsertions.push_back({c, BACKWARD_COMBINING_MASK | 0xD800 | static_cast<uint32_t>(firstCombiningClass)});
            }
            continue;
        } else {
            if (src == dst) {
                if (startsWithBackwardCombiningStarter) {
                    pendingTrieInsertions.push_back({c, BACKWARD_COMBINING_MASK});
                }
                continue;
            }
            decompositionPassthroughBound = c;
            // ICU4X hard-codes ANGSTROM SIGN
            if (c != 0x212B && mainNormalizer == nfdNormalizer) {
                UnicodeString raw;
                if (!nfdNormalizer->getRawDecomposition(c, raw)) {
                    // We're always supposed to have a non-recursive decomposition
                    // if we had a recursive one.
                    status.set(U_INTERNAL_PROGRAM_ERROR);
                    handleError(status, __LINE__, basename);
                }
                // In addition to actual difference, put the whole range that contains characters
                // with oxia into the non-recursive trie in order to catch cases where characters
                // with oxia have singleton decompositions to corresponding characters with tonos.
                // This way, the run-time decision to fall through can be done on the range
                // without checking for individual characters inside the range.
                if (raw != dst || (c >= 0x1F71 && c <= 0x1FFB)) {
                    int32_t rawLen = raw.toUTF32(rawUtf32, RAW_DECOMPOSITION_BUFFER_SIZE, status);
                    if (!rawLen) {
                        status.set(U_INTERNAL_PROGRAM_ERROR);
                        handleError(status, __LINE__, basename);
                    }
                    if (rawLen == 1) {
                        if (c >= 0xFFFF) {
                            status.set(U_INTERNAL_PROGRAM_ERROR);
                            handleError(status, __LINE__, basename);
                        }
                        umutablecptrie_set(nonRecursiveBuilder.getAlias(), c, static_cast<uint32_t>(rawUtf32[0]), status);
                    } else if (rawUtf32[0] <= 0xFFFF && rawUtf32[1] <= 0xFFFF) {
                        if (!rawUtf32[0] || !rawUtf32[1]) {
                            status.set(U_INTERNAL_PROGRAM_ERROR);
                            handleError(status, __LINE__, basename);
                        }
                        // Swapped for consistency with the primary trie
                        uint32_t bmpPair = static_cast<uint32_t>(rawUtf32[1]) << 16 | static_cast<uint32_t>(rawUtf32[0]);
                        umutablecptrie_set(nonRecursiveBuilder.getAlias(), c, bmpPair, status);
                    } else {
                        // Let's add 1 to index to make it always non-zero to distinguish
                        // it from the default zero.
                        uint32_t index = nonRecursive32.size() + 1;
                        nonRecursive32.push_back(static_cast<uint32_t>(rawUtf32[0]));
                        nonRecursive32.push_back(static_cast<uint32_t>(rawUtf32[1]));
                        if (index > 0xFFFF) {
                            status.set(U_INTERNAL_PROGRAM_ERROR);
                            handleError(status, __LINE__, basename);
                        }
                        umutablecptrie_set(nonRecursiveBuilder.getAlias(), c, index << 16, status);
                    }
                }
            }
        }
        if (!roundTripsViaCanonicalComposition) {
            compositionPassthroughBound = c;
        }
        if (!len) {
            if (!uts46) {
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }
            pendingTrieInsertions.push_back({c, uint32_t(0xFFFFFFFF)});
        } else if (len == 1 && ((utf32[0] >= 0x1161 && utf32[0] <= 0x1175) || (utf32[0] >= 0x11A8 && utf32[0] <= 0x11C2))) {
            // Singleton decompositions to conjoining jamo.
            if (mainNormalizer == nfdNormalizer) {
                // Not supposed to happen in NFD
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }
            pendingTrieInsertions.push_back({c, static_cast<uint32_t>(utf32[0]) | NON_ROUND_TRIP_MASK | (startsWithBackwardCombiningStarter ? BACKWARD_COMBINING_MASK : 0)});
        } else if (!startsWithBackwardCombiningStarter && len == 1 && utf32[0] <= 0xFFFF) {
            pendingTrieInsertions.push_back({c, static_cast<uint32_t>(utf32[0]) | NON_ROUND_TRIP_MASK | (startsWithBackwardCombiningStarter ? BACKWARD_COMBINING_MASK : 0)});
        } else if (c != 0x212B && // ANGSTROM SIGN is special to make the Harfbuzz case branch less in the more common case.
                   !startsWithBackwardCombiningStarter &&
                   len == 2 &&
                   utf32[0] <= 0x7FFF &&
                   utf32[1] <= 0x7FFF &&
                   utf32[0] > 0x1F &&
                   utf32[1] > 0x1F &&
                   !u_getCombiningClass(utf32[0]) &&
                   u_getCombiningClass(utf32[1])) {
            for (int32_t i = 0; i < len; ++i) {
                if (((utf32[i] == 0x0345) && (uprv_strcmp(basename, "uts46d") == 0)) || utf32[i] == 0xFF9E || utf32[i] == 0xFF9F) {
                    // Assert that iota subscript and half-width voicing marks never occur in these
                    // expansions in the normalization forms where they are special.
                    status.set(U_INTERNAL_PROGRAM_ERROR);
                    handleError(status, __LINE__, basename);
                }
            }
            pendingTrieInsertions.push_back({c, static_cast<uint32_t>(utf32[0]) | (static_cast<uint32_t>(utf32[1]) << 15) | (roundTripsViaCanonicalComposition ? 0 : NON_ROUND_TRIP_MASK)});
        } else {
            UBool supplementary = false;
            UBool nonInitialStarter = false;
            for (int32_t i = 0; i < len; ++i) {
                if (((utf32[i] == 0x0345) && (uprv_strcmp(basename, "uts46d") == 0)) || utf32[i] == 0xFF9E || utf32[i] == 0xFF9F) {
                    // Assert that iota subscript and half-width voicing marks never occur in these
                    // expansions in the normalization forms where they are special.
                    status.set(U_INTERNAL_PROGRAM_ERROR);
                    handleError(status, __LINE__, basename);
                }

                if (utf32[i] > 0xFFFF) {
                    supplementary = true;
                }
                if (utf32[i] == 0) {
                    status.set(U_INTERNAL_PROGRAM_ERROR);
                    handleError(status, __LINE__, basename);
                }
                if (i != 0 && !u_getCombiningClass(utf32[i])) {
                    nonInitialStarter = true;
                }
            }
            if (len == 1) {
                // The format doesn't allow for length 1 for BMP,
                // so if these ever occur, they need to be promoted
                // to wider storage. As of Unicode 16 alpha, this
                // case does not arise.
                supplementary = true;
            }
            if (!supplementary) {
                if (len > LONGEST_ENCODABLE_LENGTH_16 || !len || len == 1) {
                    if (len == 18 && c == 0xFDFA) {
                        // Special marker for the one character whose decomposition
                        // is too long. (Too long even if we took the fourth bit into use!)
                        pendingTrieInsertions.push_back({c, NON_ROUND_TRIP_MASK | 1});
                        continue;
                    } else {
                        // Note: There's a fourth bit available, but let's error out
                        // if it's ever needed so that it doesn't get used without
                        // updating docs.
                        status.set(U_INTERNAL_PROGRAM_ERROR);
                        handleError(status, __LINE__, basename);
                    }
                }
            } else if (len > LONGEST_ENCODABLE_LENGTH_32 || !len) {
                // Note: There's a fourth bit available, but let's error out
                // if it's ever needed so that it doesn't get used without
                // updating docs.
                status.set(U_INTERNAL_PROGRAM_ERROR);
                handleError(status, __LINE__, basename);
            }

            size_t index = 0;
            if (!supplementary) {
                index = findOrAppend(storage16, utf32, len);
            } else {
                index = findOrAppend(storage32, utf32, len);
            }
            pendingTrieInsertions.push_back({c, (startsWithBackwardCombiningStarter ? BACKWARD_COMBINING_MASK : 0) | (roundTripsViaCanonicalComposition ? 0 : NON_ROUND_TRIP_MASK), supplementary, !nonInitialStarter, uint32_t(len), uint32_t(index)});
        }
    }
    if (storage16.size() + storage32.size() > 0xFFF) {
        // We actually have 14 bits available, but let's error out so
        // that docs can be updated when taking a reserved bit out of
        // potential future flag usage.
        status.set(U_INTERNAL_PROGRAM_ERROR);
    }
    if (f) {
        usrc_writeArray(f, "scalars32 = [\n  ", nonRecursive32.data(), 32, nonRecursive32.size(), "  ", "\n]\n");

        LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
            nonRecursiveBuilder.getAlias(),
            trieType,
            UCPTRIE_VALUE_BITS_32,
            status));
        handleError(status, __LINE__, basename);

        fprintf(f, "[trie]\n");
        usrc_writeUCPTrie(f, "trie", utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);

        fclose(f);
    }
    handleError(status, __LINE__, basename);
}

#endif // !UCONFIG_NO_NORMALIZATION

enum {
    OPT_HELP_H,
    OPT_HELP_QUESTION_MARK,
    OPT_MODE,
    OPT_TRIE_TYPE,
    OPT_VERSION,
    OPT_DESTDIR,
    OPT_ALL,
    OPT_INDEX,
    OPT_COPYRIGHT,
    OPT_VERBOSE,
    OPT_QUIET,

    OPT_COUNT
};

#define UOPTION_MODE UOPTION_DEF("mode", 'm', UOPT_REQUIRES_ARG)
#define UOPTION_TRIE_TYPE UOPTION_DEF("trie-type", '\1', UOPT_REQUIRES_ARG)
#define UOPTION_ALL UOPTION_DEF("all", '\1', UOPT_NO_ARG)
#define UOPTION_INDEX UOPTION_DEF("index", '\1', UOPT_NO_ARG)

static UOption options[]={
    UOPTION_HELP_H,
    UOPTION_HELP_QUESTION_MARK,
    UOPTION_MODE,
    UOPTION_TRIE_TYPE,
    UOPTION_VERSION,
    UOPTION_DESTDIR,
    UOPTION_ALL,
    UOPTION_INDEX,
    UOPTION_COPYRIGHT,
    UOPTION_VERBOSE,
    UOPTION_QUIET,
};

void printHelp(FILE* stdfile, const char* program) {
  fprintf(stdfile,
          "usage: %s -m mode [-options] [--all | properties...]\n"
          "\tdump Unicode property data to .toml files\n"
          "options:\n"
          "\t-h or -? or --help  this usage text\n"
          "\t-V or --version     show a version message\n"
          "\t-m or --mode        mode: currently only 'uprops', 'ucase', and 'norm', but more may be added\n"
          "\t      --trie-type   set the trie type (small or fast, default small)\n"
          "\t-d or --destdir     destination directory, followed by the path\n"
          "\t      --all         write out all properties known to icuexportdata\n"
          "\t      --index       write an _index.toml summarizing all data exported\n"
          "\t-c or --copyright   include a copyright notice\n"
          "\t-v or --verbose     Turn on verbose output\n"
          "\t-q or --quiet       do not display warnings and progress\n",
          program);
}

int exportUprops(int argc, char* argv[]) {
    // Load list of Unicode properties
    std::vector<const char*> propNames;
    for (int i=1; i<argc; i++) {
        propNames.push_back(argv[i]);
    }
    if (options[OPT_ALL].doesOccur) {
        int i = UCHAR_BINARY_START;
        while (true) {
            if (i == UCHAR_BINARY_LIMIT) {
                i = UCHAR_INT_START;
            }
            if (i == UCHAR_INT_LIMIT) {
                i = UCHAR_GENERAL_CATEGORY_MASK;
            }
            if (i == UCHAR_GENERAL_CATEGORY_MASK + 1) {
                i = UCHAR_BIDI_MIRRORING_GLYPH;
            }
            if (i == UCHAR_BIDI_MIRRORING_GLYPH + 1) {
                i = UCHAR_SCRIPT_EXTENSIONS;
            }
            if (i == UCHAR_SCRIPT_EXTENSIONS + 1) {
                break;
            }
            UProperty uprop = static_cast<UProperty>(i);
            const char* propName = u_getPropertyName(uprop, U_SHORT_PROPERTY_NAME);
            if (propName == nullptr) {
                propName = u_getPropertyName(uprop, U_LONG_PROPERTY_NAME);
                if (propName != nullptr && VERBOSE) {
                    std::cerr << "Note: falling back to long name for: " << propName << std::endl;
                }
            }
            if (propName != nullptr) {
                propNames.push_back(propName);
            } else {
                std::cerr << "Warning: Could not find name for: " << uprop << std::endl;
            }
            i++;
        }
    }

    if (propNames.empty()
            || options[OPT_HELP_H].doesOccur
            || options[OPT_HELP_QUESTION_MARK].doesOccur
            || !options[OPT_MODE].doesOccur) {
        FILE *stdfile=argc<0 ? stderr : stdout;
        fprintf(stdfile,
            "usage: %s -m uprops [-options] [--all | properties...]\n"
            "\tdump Unicode property data to .toml files\n"
            "options:\n"
            "\t-h or -? or --help  this usage text\n"
            "\t-V or --version     show a version message\n"
            "\t-m or --mode        mode: currently only 'uprops', but more may be added\n"
            "\t      --trie-type   set the trie type (small or fast, default small)\n"
            "\t-d or --destdir     destination directory, followed by the path\n"
            "\t      --all         write out all properties known to icuexportdata\n"
            "\t      --index       write an _index.toml summarizing all data exported\n"
            "\t-c or --copyright   include a copyright notice\n"
            "\t-v or --verbose     Turn on verbose output\n"
            "\t-q or --quiet       do not display warnings and progress\n",
            argv[0]);
        return argc<0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
    }

    const char* mode = options[OPT_MODE].value;
    if (uprv_strcmp(mode, "uprops") != 0) {
        fprintf(stderr, "Invalid option for --mode (must be uprops)\n");
        return U_ILLEGAL_ARGUMENT_ERROR;
    }

    if (options[OPT_TRIE_TYPE].doesOccur) {
        if (uprv_strcmp(options[OPT_TRIE_TYPE].value, "fast") == 0) {
            trieType = UCPTRIE_TYPE_FAST;
        } else if (uprv_strcmp(options[OPT_TRIE_TYPE].value, "small") == 0) {
            trieType = UCPTRIE_TYPE_SMALL;
        } else {
            fprintf(stderr, "Invalid option for --trie-type (must be small or fast)\n");
            return U_ILLEGAL_ARGUMENT_ERROR;
        }
    }

    for (const char* propName : propNames) {
        UProperty propEnum = u_getPropertyEnum(propName);
        if (propEnum == UCHAR_INVALID_CODE) {
            std::cerr << "Error: Invalid property alias: " << propName << std::endl;
            return U_ILLEGAL_ARGUMENT_ERROR;
        }

        FILE* f = prepareOutputFile(propName);

        UVersionInfo versionInfo;
        u_getUnicodeVersion(versionInfo);
        char uvbuf[U_MAX_VERSION_STRING_LENGTH];
        u_versionToString(versionInfo, uvbuf);
        fprintf(f, "icu_version = \"%s\"\nunicode_version = \"%s\"\n\n",
            U_ICU_VERSION,
            uvbuf);

        if (propEnum < UCHAR_BINARY_LIMIT) {
            dumpBinaryProperty(propEnum, f);
        } else if (UCHAR_INT_START <= propEnum && propEnum <= UCHAR_INT_LIMIT) {
            dumpEnumeratedProperty(propEnum, f);
        } else if (propEnum == UCHAR_GENERAL_CATEGORY_MASK) {
            dumpGeneralCategoryMask(f);
        } else if (propEnum == UCHAR_BIDI_MIRRORING_GLYPH) {
            dumpBidiMirroringGlyph(f);
        } else if (propEnum == UCHAR_SCRIPT_EXTENSIONS) {
            dumpScriptExtensions(f);
        } else {
            std::cerr << "Don't know how to write property: " << propEnum << std::endl;
            return U_INTERNAL_PROGRAM_ERROR;
        }

        fclose(f);
    }

    if (options[OPT_INDEX].doesOccur) {
        FILE* f = prepareOutputFile("_index");
        fprintf(f, "index = [\n");
        for (const char* propName : propNames) {
            // At this point, propName is a valid property name, so it should be alphanum ASCII
            fprintf(f, "  { filename=\"%s.toml\" },\n", propName);
        }
        fprintf(f, "]\n");
        fclose(f);
    }

    return 0;
}

struct AddRangeHelper {
    UMutableCPTrie* ucptrie;
};

static UBool U_CALLCONV
addRangeToUCPTrie(const void* context, UChar32 start, UChar32 end, uint32_t value) {
    IcuToolErrorCode status("addRangeToUCPTrie");
    UMutableCPTrie* ucptrie = static_cast<const AddRangeHelper*>(context)->ucptrie;
    umutablecptrie_setRange(ucptrie, start, end, value, status);
    handleError(status, __LINE__, "setRange");

    return true;
}

int exportCase(int argc, char* argv[]) {
    if (argc > 1) {
        fprintf(stderr, "ucase mode does not expect additional arguments\n");
        return U_ILLEGAL_ARGUMENT_ERROR;
    }
    (void) argv; // Suppress unused variable warning

    IcuToolErrorCode status("icuexportdata");
    LocalUMutableCPTriePointer builder(umutablecptrie_open(0, 0, status));
    handleError(status, __LINE__, "exportCase");

    int32_t exceptionsLength, unfoldLength;
    const UCaseProps *caseProps = ucase_getSingleton(&exceptionsLength, &unfoldLength);
    const UTrie2* caseTrie = &caseProps->trie;

    AddRangeHelper helper = { builder.getAlias() };
    utrie2_enum(caseTrie, nullptr, addRangeToUCPTrie, &helper);

    UCPTrieValueWidth width = UCPTRIE_VALUE_BITS_16;
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        width,
        status));
    handleError(status, __LINE__, "exportCase");

    FILE* f = prepareOutputFile("ucase");

    UVersionInfo versionInfo;
    u_getUnicodeVersion(versionInfo);
    char uvbuf[U_MAX_VERSION_STRING_LENGTH];
    u_versionToString(versionInfo, uvbuf);
    fprintf(f, "icu_version = \"%s\"\nunicode_version = \"%s\"\n\n",
            U_ICU_VERSION,
            uvbuf);

    fputs("[ucase.code_point_trie]\n", f);
    usrc_writeUCPTrie(f, "case_trie", utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
    fputs("\n", f);

    const char* indent = "  ";
    const char* suffix = "\n]\n";

    fputs("[ucase.exceptions]\n", f);
    const char* exceptionsPrefix = "exceptions = [\n  ";
    int32_t exceptionsWidth = 16;
    usrc_writeArray(f, exceptionsPrefix, caseProps->exceptions, exceptionsWidth,
                    exceptionsLength, indent, suffix);
    fputs("\n", f);

    fputs("[ucase.unfold]\n", f);
    const char* unfoldPrefix = "unfold = [\n  ";
    int32_t unfoldWidth = 16;
    usrc_writeArray(f, unfoldPrefix, caseProps->unfold, unfoldWidth,
                    unfoldLength, indent, suffix);

    return 0;
}

#if !UCONFIG_NO_NORMALIZATION

int exportNorm() {
    IcuToolErrorCode status("icuexportdata: exportNorm");
    USet* backwardCombiningStarters = uset_openEmpty();
    writeCanonicalCompositions(backwardCombiningStarters);

    std::vector<uint16_t> storage16;
    std::vector<uint32_t> storage32;

    // Note: the USets are not exported. They are only used to check that a new
    // Unicode version doesn't violate expectations that are hard-coded in ICU4X.
    USet* nfdDecompositionStartsWithNonStarter = uset_openEmpty();
    USet* nfdDecompositionStartsWithBackwardCombiningStarter = uset_openEmpty();
    std::vector<PendingDescriptor> nfdPendingTrieInsertions;
    UChar32 nfdBound = 0x10FFFF;
    UChar32 nfcBound = 0x10FFFF;
    computeDecompositions("nfd",
                          backwardCombiningStarters,
                          storage16,
                          storage32,
                          nfdDecompositionStartsWithNonStarter,
                          nfdDecompositionStartsWithBackwardCombiningStarter,
                          nfdPendingTrieInsertions,
                          nfdBound,
                          nfcBound);
    if (!(nfdBound == 0xC0 && nfcBound == 0x300)) {
        // Unexpected bounds for NFD/NFC.
        status.set(U_INTERNAL_PROGRAM_ERROR);
        handleError(status, __LINE__, "exportNorm");
    }

    uint32_t baseSize16 = storage16.size();
    uint32_t baseSize32 = storage32.size();

    USet* nfkdDecompositionStartsWithNonStarter = uset_openEmpty();
    USet* nfkdDecompositionStartsWithBackwardCombiningStarter = uset_openEmpty();
    std::vector<PendingDescriptor> nfkdPendingTrieInsertions;
    UChar32 nfkdBound = 0x10FFFF;
    UChar32 nfkcBound = 0x10FFFF;
    computeDecompositions("nfkd",
                          backwardCombiningStarters,
                          storage16,
                          storage32,
                          nfkdDecompositionStartsWithNonStarter,
                          nfkdDecompositionStartsWithBackwardCombiningStarter,
                          nfkdPendingTrieInsertions,
                          nfkdBound,
                          nfkcBound);
    if (!(nfkdBound <= 0xC0 && nfkcBound <= 0x300)) {
        status.set(U_INTERNAL_PROGRAM_ERROR);
        handleError(status, __LINE__, "exportNorm");
    }
    if (nfkcBound > 0xC0) {
        if (nfkdBound != 0xC0) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, "exportNorm");
        }
    } else {
        if (nfkdBound != nfkcBound) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, "exportNorm");
        }
    }

    USet* uts46DecompositionStartsWithNonStarter = uset_openEmpty();
    USet* uts46DecompositionStartsWithBackwardCombiningStarter = uset_openEmpty();
    std::vector<PendingDescriptor> uts46PendingTrieInsertions;
    UChar32 uts46dBound = 0x10FFFF;
    UChar32 uts46Bound = 0x10FFFF;
    computeDecompositions("uts46d",
                          backwardCombiningStarters,
                          storage16,
                          storage32,
                          uts46DecompositionStartsWithNonStarter,
                          uts46DecompositionStartsWithBackwardCombiningStarter,
                          uts46PendingTrieInsertions,
                          uts46dBound,
                          uts46Bound);
    if (!(uts46dBound <= 0xC0 && uts46Bound <= 0x300)) {
        status.set(U_INTERNAL_PROGRAM_ERROR);
        handleError(status, __LINE__, "exportNorm");
    }
    if (uts46Bound > 0xC0) {
        if (uts46dBound != 0xC0) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, "exportNorm");
        }
    } else {
        if (uts46dBound != uts46Bound) {
            status.set(U_INTERNAL_PROGRAM_ERROR);
            handleError(status, __LINE__, "exportNorm");
        }
    }

    uint32_t supplementSize16 = storage16.size() - baseSize16;
    uint32_t supplementSize32 = storage32.size() - baseSize32;

    writeDecompositionData("nfd", baseSize16, baseSize32, supplementSize16, nfdDecompositionStartsWithNonStarter, nullptr, nfdPendingTrieInsertions, nfdPendingTrieInsertions, static_cast<char16_t>(nfcBound));
    writeDecompositionData("nfkd", baseSize16, baseSize32, supplementSize16, nfkdDecompositionStartsWithNonStarter, nfdDecompositionStartsWithNonStarter, nfkdPendingTrieInsertions, nfdPendingTrieInsertions, static_cast<char16_t>(nfkcBound));
    writeDecompositionData("uts46d", baseSize16, baseSize32, supplementSize16, uts46DecompositionStartsWithNonStarter, nfdDecompositionStartsWithNonStarter, uts46PendingTrieInsertions, nfdPendingTrieInsertions, static_cast<char16_t>(uts46Bound));

    writeDecompositionTables("nfdex", storage16.data(), baseSize16, storage32.data(), baseSize32);
    writeDecompositionTables("nfkdex", storage16.data() + baseSize16, supplementSize16, storage32.data() + baseSize32, supplementSize32);

    uset_close(nfdDecompositionStartsWithNonStarter);
    uset_close(nfkdDecompositionStartsWithNonStarter);
    uset_close(uts46DecompositionStartsWithNonStarter);

    uset_close(nfdDecompositionStartsWithBackwardCombiningStarter);
    uset_close(nfkdDecompositionStartsWithBackwardCombiningStarter);
    uset_close(uts46DecompositionStartsWithBackwardCombiningStarter);

    uset_close(backwardCombiningStarters);
    handleError(status, __LINE__, "exportNorm");
    return 0;
}

#endif // !UCONFIG_NO_NORMALIZATION

int main(int argc, char* argv[]) {
    U_MAIN_INIT_ARGS(argc, argv);

    /* preset then read command line options */
    options[OPT_DESTDIR].value=u_getDataDirectory();
    argc=u_parseArgs(argc, argv, UPRV_LENGTHOF(options), options);

    if(options[OPT_VERSION].doesOccur) {
        printf("icuexportdata version %s, ICU tool to dump data files for external consumers\n",
               U_ICU_DATA_VERSION);
        printf("%s\n", U_COPYRIGHT_STRING);
        exit(0);
    }

    /* error handling, printing usage message */
    if(argc<0) {
        fprintf(stderr,
            "error in command line argument \"%s\"\n",
            argv[-argc]);
    }

    if (argc < 0
            || options[OPT_HELP_H].doesOccur
            || options[OPT_HELP_QUESTION_MARK].doesOccur
            || !options[OPT_MODE].doesOccur) {
        FILE *stdfile=argc<0 ? stderr : stdout;
        printHelp(stdfile, argv[0]);
        return argc<0 ? U_ILLEGAL_ARGUMENT_ERROR : U_ZERO_ERROR;
    }

    /* get the options values */
    haveCopyright = options[OPT_COPYRIGHT].doesOccur;
    destdir = options[OPT_DESTDIR].value;
    VERBOSE = options[OPT_VERBOSE].doesOccur;
    QUIET = options[OPT_QUIET].doesOccur;

    if (options[OPT_TRIE_TYPE].doesOccur) {
        if (uprv_strcmp(options[OPT_TRIE_TYPE].value, "fast") == 0) {
            trieType = UCPTRIE_TYPE_FAST;
        } else if (uprv_strcmp(options[OPT_TRIE_TYPE].value, "small") == 0) {
            trieType = UCPTRIE_TYPE_SMALL;
        } else {
            fprintf(stderr, "Invalid option for --trie-type (must be small or fast)\n");
            return U_ILLEGAL_ARGUMENT_ERROR;
        }
    }

    const char* mode = options[OPT_MODE].value;
    if (uprv_strcmp(mode, "norm") == 0) {
#if !UCONFIG_NO_NORMALIZATION
        return exportNorm();
#else
    fprintf(stderr, "Exporting normalization data not supported when compiling without normalization support.\n");
    return U_ILLEGAL_ARGUMENT_ERROR;
#endif
    }
    if (uprv_strcmp(mode, "uprops") == 0) {
        return exportUprops(argc, argv);
    } else if (uprv_strcmp(mode, "ucase") == 0) {
        return exportCase(argc, argv);
    }

    fprintf(stderr, "Invalid option for --mode (must be uprops, ucase, or norm)\n");
    return U_ILLEGAL_ARGUMENT_ERROR;
}
