// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <iostream>
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
#include "ucase.h"
#include "writesrc.h"

U_NAMESPACE_USE

/*
 * Global - verbosity
 */
UBool VERBOSE = FALSE;
UBool QUIET = FALSE;

UBool haveCopyright = TRUE;
UCPTrieType trieType = UCPTRIE_TYPE_SMALL;
const char* destdir = "";

// Mask constants for modified values in the Script CodePointTrie, values are logically 12-bits.
int16_t DATAEXPORT_SCRIPT_X_WITH_COMMON    = 0x0400;
int16_t DATAEXPORT_SCRIPT_X_WITH_INHERITED = 0x0800;
int16_t DATAEXPORT_SCRIPT_X_WITH_OTHER     = 0x0c00;

// TODO(ICU-21821): Replace this with a call to a library function
int32_t scxCodePoints[] = {
      7415, 7377, 7380, 7387, 7390, 7391, 7394, 7395, 7396, 7397,
      7398, 7399, 7400, 7403, 7404, 7406, 7407, 7408, 7409, 113824,
      113825, 113826, 113827, 834, 837, 7616, 7617, 12294, 12350, 12351,
      12688, 12689, 12690, 12691, 12692, 12693, 12694, 12695, 12696, 12697,
      12698, 12699, 12700, 12701, 12702, 12703, 12736, 12737, 12738, 12739,
      12740, 12741, 12742, 12743, 12744, 12745, 12746, 12747, 12748, 12749,
      12750, 12751, 12752, 12753, 12754, 12755, 12756, 12757, 12758, 12759,
      12760, 12761, 12762, 12763, 12764, 12765, 12766, 12767, 12768, 12769,
      12770, 12771, 12832, 12833, 12834, 12835, 12836, 12837, 12838, 12839,
      12840, 12841, 12842, 12843, 12844, 12845, 12846, 12847, 12848, 12849,
      12850, 12851, 12852, 12853, 12854, 12855, 12856, 12857, 12858, 12859,
      12860, 12861, 12862, 12863, 12864, 12865, 12866, 12867, 12868, 12869,
      12870, 12871, 12928, 12929, 12930, 12931, 12932, 12933, 12934, 12935,
      12936, 12937, 12938, 12939, 12940, 12941, 12942, 12943, 12944, 12945,
      12946, 12947, 12948, 12949, 12950, 12951, 12952, 12953, 12954, 12955,
      12956, 12957, 12958, 12959, 12960, 12961, 12962, 12963, 12964, 12965,
      12966, 12967, 12968, 12969, 12970, 12971, 12972, 12973, 12974, 12975,
      12976, 12992, 12993, 12994, 12995, 12996, 12997, 12998, 12999, 13000,
      13001, 13002, 13003, 13055, 13144, 13145, 13146, 13147, 13148, 13149,
      13150, 13151, 13152, 13153, 13154, 13155, 13156, 13157, 13158, 13159,
      13160, 13161, 13162, 13163, 13164, 13165, 13166, 13167, 13168, 13179,
      13180, 13181, 13182, 13183, 13280, 13281, 13282, 13283, 13284, 13285,
      13286, 13287, 13288, 13289, 13290, 13291, 13292, 13293, 13294, 13295,
      13296, 13297, 13298, 13299, 13300, 13301, 13302, 13303, 13304, 13305,
      13306, 13307, 13308, 13309, 13310, 119648, 119649, 119650, 119651, 119652,
      119653, 119654, 119655, 119656, 119657, 119658, 119659, 119660, 119661, 119662,
      119663, 119664, 119665, 127568, 127569, 867, 868, 869, 870, 871,
      872, 873, 874, 875, 876, 877, 878, 879, 7418, 7674,
      66272, 66273, 66274, 66275, 66276, 66277, 66278, 66279, 66280, 66281,
      66282, 66283, 66284, 66285, 66286, 66287, 66288, 66289, 66290, 66291,
      66292, 66293, 66294, 66295, 66296, 66297, 66298, 66299, 1748, 64830,
      64831, 1611, 1612, 1613, 1614, 1615, 1616, 1617, 1618, 1619,
      1620, 1621, 1648, 65010, 65021, 7381, 7382, 7384, 7393, 7402,
      7405, 7413, 7414, 43249, 12330, 12331, 12332, 12333, 43471, 65794,
      65847, 65848, 65849, 65850, 65851, 65852, 65853, 65854, 65855, 1156,
      1159, 11843, 42607, 1157, 1158, 1155, 7672, 7379, 7411, 7416,
      7417, 7401, 7383, 7385, 7388, 7389, 7392, 43251, 4347, 3046,
      3047, 3048, 3049, 3050, 3051, 3052, 3053, 3054, 3055, 3056,
      3057, 3058, 3059, 70401, 70403, 70459, 70460, 73680, 73681, 73683,
      2790, 2791, 2792, 2793, 2794, 2795, 2796, 2797, 2798, 2799,
      2662, 2663, 2664, 2665, 2666, 2667, 2668, 2669, 2670, 2671,
      42752, 42753, 42754, 42755, 42756, 42757, 42758, 42759, 12337, 12338,
      12339, 12340, 12341, 12441, 12442, 12443, 12444, 12448, 12540, 65392,
      65438, 65439, 3302, 3303, 3304, 3305, 3306, 3307, 3308, 3309,
      3310, 3311, 8239, 68338, 6146, 6147, 6149, 1564, 1632, 1633,
      1634, 1635, 1636, 1637, 1638, 1639, 1640, 1641, 2534, 2535,
      2536, 2537, 2538, 2539, 2540, 2541, 2542, 2543, 4160, 4161,
      4162, 4163, 4164, 4165, 4166, 4167, 4168, 4169, 65792, 65793,
      65799, 65800, 65801, 65802, 65803, 65804, 65805, 65806, 65807, 65808,
      65809, 65810, 65811, 65812, 65813, 65814, 65815, 65816, 65817, 65818,
      65819, 65820, 65821, 65822, 65823, 65824, 65825, 65826, 65827, 65828,
      65829, 65830, 65831, 65832, 65833, 65834, 65835, 65836, 65837, 65838,
      65839, 65840, 65841, 65842, 65843, 7412, 8432, 12348, 12349, 43310,
      7376, 7378, 5941, 5942, 2406, 2407, 2408, 2409, 2410, 2411,
      2412, 2413, 2414, 2415, 12291, 12307, 12316, 12317, 12318, 12319,
      12336, 12343, 65093, 65094, 1548, 1563, 12289, 12290, 12296, 12297,
      12298, 12299, 12300, 12301, 12302, 12303, 12304, 12305, 12308, 12309,
      12310, 12311, 12312, 12313, 12314, 12315, 12539, 65377, 65378, 65379,
      65380, 65381, 7386, 1567, 7410, 1600, 43062, 43063, 43064, 43065,
      2386, 2385, 43059, 43060, 43061, 43056, 43057, 43058, 2404, 2405
    };

void handleError(ErrorCode& status, const char* context) {
    if (status.isFailure()) {
        std::cerr << "Error: " << context << ": " << status.errorName() << std::endl;
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

void dumpBinaryProperty(UProperty uproperty, FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpBinaryProperty");
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    const USet* uset = u_getBinaryPropertySet(uproperty, status);
    handleError(status, fullPropName);

    fputs("[[binary_property]]\n", f);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) fprintf(f, "short_name = \"%s\"\n", shortPropName);
    usrc_writeUnicodeSet(f, uset, UPRV_TARGET_SYNTAX_TOML);
}

void dumpEnumeratedProperty(UProperty uproperty, FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpEnumeratedProperty");
    const char* fullPropName = u_getPropertyName(uproperty, U_LONG_PROPERTY_NAME);
    const char* shortPropName = u_getPropertyName(uproperty, U_SHORT_PROPERTY_NAME);
    const UCPMap* umap = u_getIntPropertyMap(uproperty, status);
    handleError(status, fullPropName);

    fputs("[[enum_property]]\n", f);
    fprintf(f, "long_name = \"%s\"\n", fullPropName);
    if (shortPropName) fprintf(f, "short_name = \"%s\"\n", shortPropName);
    PropertyValueNameGetter valueNameGetter(uproperty);
    usrc_writeUCPMap(f, umap, &valueNameGetter, UPRV_TARGET_SYNTAX_TOML);
    fputs("\n", f);

    U_ASSERT(u_getIntPropertyMinValue(uproperty) >= 0);
    int32_t maxValue = u_getIntPropertyMaxValue(uproperty);
    U_ASSERT(maxValue >= 0);
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
    handleError(status, fullPropName);

    fputs("[enum_property.code_point_trie]\n", f);
    usrc_writeUCPTrie(f, shortPropName, utrie.getAlias(), UPRV_TARGET_SYNTAX_TOML);
}

void dumpScriptExtensions(FILE* f) {
    IcuToolErrorCode status("icuexportdata: dumpScriptExtensions");

    fputs("[[script_extensions]]\n", f);
    const char* scxFullPropName = u_getPropertyName(UCHAR_SCRIPT_EXTENSIONS, U_LONG_PROPERTY_NAME);
    const char* scxShortPropName = u_getPropertyName(UCHAR_SCRIPT_EXTENSIONS, U_SHORT_PROPERTY_NAME);
    fprintf(f, "long_name = \"%s\"\n", scxFullPropName);
    if (scxShortPropName) fprintf(f, "short_name = \"%s\"\n", scxShortPropName);

    // We want to use 16 bits for our exported trie of sc/scx data because we
    // need 12 bits to match the 12 bits of data stored for sc/scx in the trie
    // in the uprops.icu data file.
    UCPTrieValueWidth scWidth = UCPTRIE_VALUE_BITS_16;

    // Create a mutable UCPTrie builder populated with Script property values data.
    const UCPMap* scInvMap = u_getIntPropertyMap(UCHAR_SCRIPT, status);
    handleError(status, scxFullPropName);
    LocalUMutableCPTriePointer builder(umutablecptrie_fromUCPMap(scInvMap, status));
    handleError(status, scxFullPropName);

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
        handleError(status, scxFullPropName);

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
        handleError(status, scxFullPropName);
    }
    fputs("]\n\n", f);  // Print the TOML close delimiter for the outer array.

    // Convert from mutable trie builder to immutable trie.
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        scWidth,
        status));
    handleError(status, scxFullPropName);

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
    handleError(status, basename);

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
          "\t-m or --mode        mode: currently only 'uprops' and 'ucase', but more may be added\n"
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
                i = UCHAR_SCRIPT_EXTENSIONS;
            }
            if (i == UCHAR_SCRIPT_EXTENSIONS + 1) {
                break;
            }
            UProperty uprop = static_cast<UProperty>(i);
            const char* propName = u_getPropertyName(uprop, U_SHORT_PROPERTY_NAME);
            if (propName == NULL) {
                propName = u_getPropertyName(uprop, U_LONG_PROPERTY_NAME);
                if (propName != NULL && VERBOSE) {
                    std::cerr << "Note: falling back to long name for: " << propName << std::endl;
                }
            }
            if (propName != NULL) {
                propNames.push_back(propName);
            } else {
                std::cerr << "Warning: Could not find name for: " << uprop << std::endl;
            }
            i++;
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
    UMutableCPTrie* ucptrie = ((const AddRangeHelper*) context)->ucptrie;
    umutablecptrie_setRange(ucptrie, start, end, value, status);
    handleError(status, "setRange");

    return TRUE;
}

int exportCase(int argc, char* argv[]) {
    if (argc > 1) {
        fprintf(stderr, "ucase mode does not expect additional arguments\n");
        return U_ILLEGAL_ARGUMENT_ERROR;
    }
    (void) argv; // Suppress unused variable warning

    IcuToolErrorCode status("icuexportdata");
    LocalUMutableCPTriePointer builder(umutablecptrie_open(0, 0, status));
    handleError(status, "exportCase");

    int32_t exceptionsLength, unfoldLength;
    const UCaseProps *caseProps = ucase_getSingleton(&exceptionsLength, &unfoldLength);
    const UTrie2* caseTrie = &caseProps->trie;

    AddRangeHelper helper = { builder.getAlias() };
    utrie2_enum(caseTrie, NULL, addRangeToUCPTrie, &helper);

    UCPTrieValueWidth width = UCPTRIE_VALUE_BITS_16;
    LocalUCPTriePointer utrie(umutablecptrie_buildImmutable(
        builder.getAlias(),
        trieType,
        width,
        status));
    handleError(status, "exportCase");

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
    if (uprv_strcmp(mode, "uprops") == 0) {
        return exportUprops(argc, argv);
    } else if (uprv_strcmp(mode, "ucase") == 0) {
        return exportCase(argc, argv);
    }

    fprintf(stderr, "Invalid option for --mode (must be uprops or ucase)\n");
    return U_ILLEGAL_ARGUMENT_ERROR;
}
