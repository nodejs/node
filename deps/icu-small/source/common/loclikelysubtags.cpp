// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

// loclikelysubtags.cpp
// created: 2019may08 Markus W. Scherer

#include <utility>
#include "unicode/utypes.h"
#include "unicode/bytestrie.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"
#include "unicode/uobject.h"
#include "unicode/ures.h"
#include "charstr.h"
#include "cstring.h"
#include "loclikelysubtags.h"
#include "lsr.h"
#include "uassert.h"
#include "ucln_cmn.h"
#include "uhash.h"
#include "uinvchar.h"
#include "umutex.h"
#include "uresdata.h"
#include "uresimp.h"

U_NAMESPACE_BEGIN

namespace {

constexpr char PSEUDO_ACCENTS_PREFIX = '\'';  // -XA, -PSACCENT
constexpr char PSEUDO_BIDI_PREFIX = '+';  // -XB, -PSBIDI
constexpr char PSEUDO_CRACKED_PREFIX = ',';  // -XC, -PSCRACK

/**
 * Stores NUL-terminated strings with duplicate elimination.
 * Checks for unique UTF-16 string pointers and converts to invariant characters.
 */
class UniqueCharStrings {
public:
    UniqueCharStrings(UErrorCode &errorCode) : strings(nullptr) {
        uhash_init(&map, uhash_hashUChars, uhash_compareUChars, uhash_compareLong, &errorCode);
        if (U_FAILURE(errorCode)) { return; }
        strings = new CharString();
        if (strings == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
        }
    }
    ~UniqueCharStrings() {
        uhash_close(&map);
        delete strings;
    }

    /** Returns/orphans the CharString that contains all strings. */
    CharString *orphanCharStrings() {
        CharString *result = strings;
        strings = nullptr;
        return result;
    }

    /** Adds a string and returns a unique number for it. */
    int32_t add(const UnicodeString &s, UErrorCode &errorCode) {
        if (U_FAILURE(errorCode)) { return 0; }
        if (isFrozen) {
            errorCode = U_NO_WRITE_PERMISSION;
            return 0;
        }
        // The string points into the resource bundle.
        const char16_t *p = s.getBuffer();
        int32_t oldIndex = uhash_geti(&map, p);
        if (oldIndex != 0) {  // found duplicate
            return oldIndex;
        }
        // Explicit NUL terminator for the previous string.
        // The strings object is also terminated with one implicit NUL.
        strings->append(0, errorCode);
        int32_t newIndex = strings->length();
        strings->appendInvariantChars(s, errorCode);
        uhash_puti(&map, const_cast<char16_t *>(p), newIndex, &errorCode);
        return newIndex;
    }

    void freeze() { isFrozen = true; }

    /**
     * Returns a string pointer for its unique number, if this object is frozen.
     * Otherwise nullptr.
     */
    const char *get(int32_t i) const {
        U_ASSERT(isFrozen);
        return isFrozen && i > 0 ? strings->data() + i : nullptr;
    }

private:
    UHashtable map;
    CharString *strings;
    bool isFrozen = false;
};

}  // namespace

LocaleDistanceData::LocaleDistanceData(LocaleDistanceData &&data) :
        distanceTrieBytes(data.distanceTrieBytes),
        regionToPartitions(data.regionToPartitions),
        partitions(data.partitions),
        paradigms(data.paradigms), paradigmsLength(data.paradigmsLength),
        distances(data.distances) {
    data.partitions = nullptr;
    data.paradigms = nullptr;
}

LocaleDistanceData::~LocaleDistanceData() {
    uprv_free(partitions);
    delete[] paradigms;
}

// TODO(ICU-20777): Rename to just LikelySubtagsData.
struct XLikelySubtagsData {
    UResourceBundle *langInfoBundle = nullptr;
    UniqueCharStrings strings;
    CharStringMap languageAliases;
    CharStringMap regionAliases;
    const uint8_t *trieBytes = nullptr;
    LSR *lsrs = nullptr;
    int32_t lsrsLength = 0;

    LocaleDistanceData distanceData;

    XLikelySubtagsData(UErrorCode &errorCode) : strings(errorCode) {}

    ~XLikelySubtagsData() {
        ures_close(langInfoBundle);
        delete[] lsrs;
    }

    void load(UErrorCode &errorCode) {
        langInfoBundle = ures_openDirect(nullptr, "langInfo", &errorCode);
        if (U_FAILURE(errorCode)) { return; }
        StackUResourceBundle stackTempBundle;
        ResourceDataValue value;
        ures_getValueWithFallback(langInfoBundle, "likely", stackTempBundle.getAlias(),
                                  value, errorCode);
        ResourceTable likelyTable = value.getTable(errorCode);
        if (U_FAILURE(errorCode)) { return; }

        // Read all strings in the resource bundle and convert them to invariant char *.
        LocalMemory<int32_t> languageIndexes, regionIndexes, lsrSubtagIndexes;
        int32_t languagesLength = 0, regionsLength = 0, lsrSubtagsLength = 0;
        if (!readStrings(likelyTable, "languageAliases", value,
                         languageIndexes, languagesLength, errorCode) ||
                !readStrings(likelyTable, "regionAliases", value,
                             regionIndexes, regionsLength, errorCode) ||
                !readStrings(likelyTable, "lsrs", value,
                             lsrSubtagIndexes,lsrSubtagsLength, errorCode)) {
            return;
        }
        if ((languagesLength & 1) != 0 ||
                (regionsLength & 1) != 0 ||
                (lsrSubtagsLength % 3) != 0) {
            errorCode = U_INVALID_FORMAT_ERROR;
            return;
        }
        if (lsrSubtagsLength == 0) {
            errorCode = U_MISSING_RESOURCE_ERROR;
            return;
        }

        if (!likelyTable.findValue("trie", value)) {
            errorCode = U_MISSING_RESOURCE_ERROR;
            return;
        }
        int32_t length;
        trieBytes = value.getBinary(length, errorCode);
        if (U_FAILURE(errorCode)) { return; }

        // Also read distance/matcher data if available,
        // to open & keep only one resource bundle pointer
        // and to use one single UniqueCharStrings.
        UErrorCode matchErrorCode = U_ZERO_ERROR;
        ures_getValueWithFallback(langInfoBundle, "match", stackTempBundle.getAlias(),
                                  value, matchErrorCode);
        LocalMemory<int32_t> partitionIndexes, paradigmSubtagIndexes;
        int32_t partitionsLength = 0, paradigmSubtagsLength = 0;
        if (U_SUCCESS(matchErrorCode)) {
            ResourceTable matchTable = value.getTable(errorCode);
            if (U_FAILURE(errorCode)) { return; }

            if (matchTable.findValue("trie", value)) {
                distanceData.distanceTrieBytes = value.getBinary(length, errorCode);
                if (U_FAILURE(errorCode)) { return; }
            }

            if (matchTable.findValue("regionToPartitions", value)) {
                distanceData.regionToPartitions = value.getBinary(length, errorCode);
                if (U_FAILURE(errorCode)) { return; }
                if (length < LSR::REGION_INDEX_LIMIT) {
                    errorCode = U_INVALID_FORMAT_ERROR;
                    return;
                }
            }

            if (!readStrings(matchTable, "partitions", value,
                             partitionIndexes, partitionsLength, errorCode) ||
                    !readStrings(matchTable, "paradigms", value,
                                 paradigmSubtagIndexes, paradigmSubtagsLength, errorCode)) {
                return;
            }
            if ((paradigmSubtagsLength % 3) != 0) {
                errorCode = U_INVALID_FORMAT_ERROR;
                return;
            }

            if (matchTable.findValue("distances", value)) {
                distanceData.distances = value.getIntVector(length, errorCode);
                if (U_FAILURE(errorCode)) { return; }
                if (length < 4) {  // LocaleDistance IX_LIMIT
                    errorCode = U_INVALID_FORMAT_ERROR;
                    return;
                }
            }
        } else if (matchErrorCode == U_MISSING_RESOURCE_ERROR) {
            // ok for likely subtags
        } else {  // error other than missing resource
            errorCode = matchErrorCode;
            return;
        }

        // Fetch & store invariant-character versions of strings
        // only after we have collected and de-duplicated all of them.
        strings.freeze();

        languageAliases = CharStringMap(languagesLength / 2, errorCode);
        for (int32_t i = 0; i < languagesLength; i += 2) {
            languageAliases.put(strings.get(languageIndexes[i]),
                                strings.get(languageIndexes[i + 1]), errorCode);
        }

        regionAliases = CharStringMap(regionsLength / 2, errorCode);
        for (int32_t i = 0; i < regionsLength; i += 2) {
            regionAliases.put(strings.get(regionIndexes[i]),
                              strings.get(regionIndexes[i + 1]), errorCode);
        }
        if (U_FAILURE(errorCode)) { return; }

        lsrsLength = lsrSubtagsLength / 3;
        lsrs = new LSR[lsrsLength];
        if (lsrs == nullptr) {
            errorCode = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        for (int32_t i = 0, j = 0; i < lsrSubtagsLength; i += 3, ++j) {
            lsrs[j] = LSR(strings.get(lsrSubtagIndexes[i]),
                          strings.get(lsrSubtagIndexes[i + 1]),
                          strings.get(lsrSubtagIndexes[i + 2]));
        }

        if (partitionsLength > 0) {
            distanceData.partitions = static_cast<const char **>(
                uprv_malloc(partitionsLength * sizeof(const char *)));
            if (distanceData.partitions == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            for (int32_t i = 0; i < partitionsLength; ++i) {
                distanceData.partitions[i] = strings.get(partitionIndexes[i]);
            }
        }

        if (paradigmSubtagsLength > 0) {
            distanceData.paradigmsLength = paradigmSubtagsLength / 3;
            LSR *paradigms = new LSR[distanceData.paradigmsLength];
            if (paradigms == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
            for (int32_t i = 0, j = 0; i < paradigmSubtagsLength; i += 3, ++j) {
                paradigms[j] = LSR(strings.get(paradigmSubtagIndexes[i]),
                                   strings.get(paradigmSubtagIndexes[i + 1]),
                                   strings.get(paradigmSubtagIndexes[i + 2]));
            }
            distanceData.paradigms = paradigms;
        }
    }

private:
    bool readStrings(const ResourceTable &table, const char *key, ResourceValue &value,
                     LocalMemory<int32_t> &indexes, int32_t &length, UErrorCode &errorCode) {
        if (table.findValue(key, value)) {
            ResourceArray stringArray = value.getArray(errorCode);
            if (U_FAILURE(errorCode)) { return false; }
            length = stringArray.getSize();
            if (length == 0) { return true; }
            int32_t *rawIndexes = indexes.allocateInsteadAndCopy(length);
            if (rawIndexes == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return false;
            }
            for (int i = 0; i < length; ++i) {
                stringArray.getValue(i, value);  // returns TRUE because i < length
                rawIndexes[i] = strings.add(value.getUnicodeString(errorCode), errorCode);
                if (U_FAILURE(errorCode)) { return false; }
            }
        }
        return true;
    }
};

namespace {

XLikelySubtags *gLikelySubtags = nullptr;
UInitOnce gInitOnce = U_INITONCE_INITIALIZER;

UBool U_CALLCONV cleanup() {
    delete gLikelySubtags;
    gLikelySubtags = nullptr;
    gInitOnce.reset();
    return TRUE;
}

}  // namespace

void U_CALLCONV XLikelySubtags::initLikelySubtags(UErrorCode &errorCode) {
    // This function is invoked only via umtx_initOnce().
    U_ASSERT(gLikelySubtags == nullptr);
    XLikelySubtagsData data(errorCode);
    data.load(errorCode);
    if (U_FAILURE(errorCode)) { return; }
    gLikelySubtags = new XLikelySubtags(data);
    if (gLikelySubtags == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    ucln_common_registerCleanup(UCLN_COMMON_LIKELY_SUBTAGS, cleanup);
}

const XLikelySubtags *XLikelySubtags::getSingleton(UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return nullptr; }
    umtx_initOnce(gInitOnce, &XLikelySubtags::initLikelySubtags, errorCode);
    return gLikelySubtags;
}

XLikelySubtags::XLikelySubtags(XLikelySubtagsData &data) :
        langInfoBundle(data.langInfoBundle),
        strings(data.strings.orphanCharStrings()),
        languageAliases(std::move(data.languageAliases)),
        regionAliases(std::move(data.regionAliases)),
        trie(data.trieBytes),
        lsrs(data.lsrs),
#if U_DEBUG
        lsrsLength(data.lsrsLength),
#endif
        distanceData(std::move(data.distanceData)) {
    data.langInfoBundle = nullptr;
    data.lsrs = nullptr;

    // Cache the result of looking up language="und" encoded as "*", and "und-Zzzz" ("**").
    UStringTrieResult result = trie.next(u'*');
    U_ASSERT(USTRINGTRIE_HAS_NEXT(result));
    trieUndState = trie.getState64();
    result = trie.next(u'*');
    U_ASSERT(USTRINGTRIE_HAS_NEXT(result));
    trieUndZzzzState = trie.getState64();
    result = trie.next(u'*');
    U_ASSERT(USTRINGTRIE_HAS_VALUE(result));
    defaultLsrIndex = trie.getValue();
    trie.reset();

    for (char16_t c = u'a'; c <= u'z'; ++c) {
        result = trie.next(c);
        if (result == USTRINGTRIE_NO_VALUE) {
            trieFirstLetterStates[c - u'a'] = trie.getState64();
        }
        trie.reset();
    }
}

XLikelySubtags::~XLikelySubtags() {
    ures_close(langInfoBundle);
    delete strings;
    delete[] lsrs;
}

LSR XLikelySubtags::makeMaximizedLsrFrom(const Locale &locale, UErrorCode &errorCode) const {
    const char *name = locale.getName();
    if (uprv_isAtSign(name[0]) && name[1] == 'x' && name[2] == '=') {  // name.startsWith("@x=")
        // Private use language tag x-subtag-subtag...
        return LSR(name, "", "");
    }
    return makeMaximizedLsr(locale.getLanguage(), locale.getScript(), locale.getCountry(),
                            locale.getVariant(), errorCode);
}

namespace {

const char *getCanonical(const CharStringMap &aliases, const char *alias) {
    const char *canonical = aliases.get(alias);
    return canonical == nullptr ? alias : canonical;
}

}  // namespace

LSR XLikelySubtags::makeMaximizedLsr(const char *language, const char *script, const char *region,
                                     const char *variant, UErrorCode &errorCode) const {
    // Handle pseudolocales like en-XA, ar-XB, fr-PSCRACK.
    // They should match only themselves,
    // not other locales with what looks like the same language and script subtags.
    char c1;
    if (region[0] == 'X' && (c1 = region[1]) != 0 && region[2] == 0) {
        switch (c1) {
        case 'A':
            return LSR(PSEUDO_ACCENTS_PREFIX, language, script, region, errorCode);
        case 'B':
            return LSR(PSEUDO_BIDI_PREFIX, language, script, region, errorCode);
        case 'C':
            return LSR(PSEUDO_CRACKED_PREFIX, language, script, region, errorCode);
        default:  // normal locale
            break;
        }
    }

    if (variant[0] == 'P' && variant[1] == 'S') {
        if (uprv_strcmp(variant, "PSACCENT") == 0) {
            return LSR(PSEUDO_ACCENTS_PREFIX, language, script,
                       *region == 0 ? "XA" : region, errorCode);
        } else if (uprv_strcmp(variant, "PSBIDI") == 0) {
            return LSR(PSEUDO_BIDI_PREFIX, language, script,
                       *region == 0 ? "XB" : region, errorCode);
        } else if (uprv_strcmp(variant, "PSCRACK") == 0) {
            return LSR(PSEUDO_CRACKED_PREFIX, language, script,
                       *region == 0 ? "XC" : region, errorCode);
        }
        // else normal locale
    }

    language = getCanonical(languageAliases, language);
    // (We have no script mappings.)
    region = getCanonical(regionAliases, region);
    return maximize(language, script, region);
}

LSR XLikelySubtags::maximize(const char *language, const char *script, const char *region) const {
    if (uprv_strcmp(language, "und") == 0) {
        language = "";
    }
    if (uprv_strcmp(script, "Zzzz") == 0) {
        script = "";
    }
    if (uprv_strcmp(region, "ZZ") == 0) {
        region = "";
    }
    if (*script != 0 && *region != 0 && *language != 0) {
        return LSR(language, script, region);  // already maximized
    }

    uint32_t retainOldMask = 0;
    BytesTrie iter(trie);
    uint64_t state;
    int32_t value;
    // Small optimization: Array lookup for first language letter.
    int32_t c0;
    if (0 <= (c0 = uprv_lowerOrdinal(language[0])) && c0 <= 25 &&
            language[1] != 0 &&  // language.length() >= 2
            (state = trieFirstLetterStates[c0]) != 0) {
        value = trieNext(iter.resetToState64(state), language, 1);
    } else {
        value = trieNext(iter, language, 0);
    }
    if (value >= 0) {
        if (*language != 0) {
            retainOldMask |= 4;
        }
        state = iter.getState64();
    } else {
        retainOldMask |= 4;
        iter.resetToState64(trieUndState);  // "und" ("*")
        state = 0;
    }

    if (value > 0) {
        // Intermediate or final value from just language.
        if (value == SKIP_SCRIPT) {
            value = 0;
        }
        if (*script != 0) {
            retainOldMask |= 2;
        }
    } else {
        value = trieNext(iter, script, 0);
        if (value >= 0) {
            if (*script != 0) {
                retainOldMask |= 2;
            }
            state = iter.getState64();
        } else {
            retainOldMask |= 2;
            if (state == 0) {
                iter.resetToState64(trieUndZzzzState);  // "und-Zzzz" ("**")
            } else {
                iter.resetToState64(state);
                value = trieNext(iter, "", 0);
                U_ASSERT(value >= 0);
                state = iter.getState64();
            }
        }
    }

    if (value > 0) {
        // Final value from just language or language+script.
        if (*region != 0) {
            retainOldMask |= 1;
        }
    } else {
        value = trieNext(iter, region, 0);
        if (value >= 0) {
            if (*region != 0) {
                retainOldMask |= 1;
            }
        } else {
            retainOldMask |= 1;
            if (state == 0) {
                value = defaultLsrIndex;
            } else {
                iter.resetToState64(state);
                value = trieNext(iter, "", 0);
                U_ASSERT(value > 0);
            }
        }
    }
    U_ASSERT(value < lsrsLength);
    const LSR &result = lsrs[value];

    if (*language == 0) {
        language = "und";
    }

    if (retainOldMask == 0) {
        // Quickly return a copy of the lookup-result LSR
        // without new allocation of the subtags.
        return LSR(result.language, result.script, result.region);
    }
    if ((retainOldMask & 4) == 0) {
        language = result.language;
    }
    if ((retainOldMask & 2) == 0) {
        script = result.script;
    }
    if ((retainOldMask & 1) == 0) {
        region = result.region;
    }
    return LSR(language, script, region);
}

int32_t XLikelySubtags::trieNext(BytesTrie &iter, const char *s, int32_t i) {
    UStringTrieResult result;
    uint8_t c;
    if ((c = s[i]) == 0) {
        result = iter.next(u'*');
    } else {
        for (;;) {
            c = uprv_invCharToAscii(c);
            // EBCDIC: If s[i] is not an invariant character,
            // then c is now 0 and will simply not match anything, which is harmless.
            uint8_t next = s[++i];
            if (next != 0) {
                if (!USTRINGTRIE_HAS_NEXT(iter.next(c))) {
                    return -1;
                }
            } else {
                // last character of this subtag
                result = iter.next(c | 0x80);
                break;
            }
            c = next;
        }
    }
    switch (result) {
    case USTRINGTRIE_NO_MATCH: return -1;
    case USTRINGTRIE_NO_VALUE: return 0;
    case USTRINGTRIE_INTERMEDIATE_VALUE:
        U_ASSERT(iter.getValue() == SKIP_SCRIPT);
        return SKIP_SCRIPT;
    case USTRINGTRIE_FINAL_VALUE: return iter.getValue();
    default: return -1;
    }
}

// TODO(ICU-20777): Switch Locale/uloc_ likely-subtags API from the old code
// in loclikely.cpp to this new code, including activating this
// minimizeSubtags() function. The LocaleMatcher does not minimize.
#if 0
LSR XLikelySubtags::minimizeSubtags(const char *languageIn, const char *scriptIn,
                                    const char *regionIn, ULocale.Minimize fieldToFavor,
                                    UErrorCode &errorCode) const {
    LSR result = maximize(languageIn, scriptIn, regionIn);

    // We could try just a series of checks, like:
    // LSR result2 = addLikelySubtags(languageIn, "", "");
    // if result.equals(result2) return result2;
    // However, we can optimize 2 of the cases:
    //   (languageIn, "", "")
    //   (languageIn, "", regionIn)

    // value00 = lookup(result.language, "", "")
    BytesTrie iter = new BytesTrie(trie);
    int value = trieNext(iter, result.language, 0);
    U_ASSERT(value >= 0);
    if (value == 0) {
        value = trieNext(iter, "", 0);
        U_ASSERT(value >= 0);
        if (value == 0) {
            value = trieNext(iter, "", 0);
        }
    }
    U_ASSERT(value > 0);
    LSR value00 = lsrs[value];
    boolean favorRegionOk = false;
    if (result.script.equals(value00.script)) { //script is default
        if (result.region.equals(value00.region)) {
            return new LSR(result.language, "", "");
        } else if (fieldToFavor == ULocale.Minimize.FAVOR_REGION) {
            return new LSR(result.language, "", result.region);
        } else {
            favorRegionOk = true;
        }
    }

    // The last case is not as easy to optimize.
    // Maybe do later, but for now use the straightforward code.
    LSR result2 = maximize(languageIn, scriptIn, "");
    if (result2.equals(result)) {
        return new LSR(result.language, result.script, "");
    } else if (favorRegionOk) {
        return new LSR(result.language, "", result.region);
    }
    return result;
}
#endif

U_NAMESPACE_END
