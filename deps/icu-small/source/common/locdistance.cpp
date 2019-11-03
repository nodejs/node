// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

// locdistance.cpp
// created: 2019may08 Markus W. Scherer

#include "unicode/utypes.h"
#include "unicode/bytestrie.h"
#include "unicode/localematcher.h"
#include "unicode/locid.h"
#include "unicode/uobject.h"
#include "unicode/ures.h"
#include "cstring.h"
#include "locdistance.h"
#include "loclikelysubtags.h"
#include "uassert.h"
#include "ucln_cmn.h"
#include "uinvchar.h"
#include "umutex.h"

U_NAMESPACE_BEGIN

namespace {

/**
 * Bit flag used on the last character of a subtag in the trie.
 * Must be set consistently by the builder and the lookup code.
 */
constexpr int32_t END_OF_SUBTAG = 0x80;
/** Distance value bit flag, set by the builder. */
constexpr int32_t DISTANCE_SKIP_SCRIPT = 0x80;
/** Distance value bit flag, set by trieNext(). */
constexpr int32_t DISTANCE_IS_FINAL = 0x100;
constexpr int32_t DISTANCE_IS_FINAL_OR_SKIP_SCRIPT = DISTANCE_IS_FINAL | DISTANCE_SKIP_SCRIPT;

constexpr int32_t ABOVE_THRESHOLD = 100;

// Indexes into array of distances.
enum {
    IX_DEF_LANG_DISTANCE,
    IX_DEF_SCRIPT_DISTANCE,
    IX_DEF_REGION_DISTANCE,
    IX_MIN_REGION_DISTANCE,
    IX_LIMIT
};

LocaleDistance *gLocaleDistance = nullptr;
UInitOnce gInitOnce = U_INITONCE_INITIALIZER;

UBool U_CALLCONV cleanup() {
    delete gLocaleDistance;
    gLocaleDistance = nullptr;
    gInitOnce.reset();
    return TRUE;
}

}  // namespace

void U_CALLCONV LocaleDistance::initLocaleDistance(UErrorCode &errorCode) {
    // This function is invoked only via umtx_initOnce().
    U_ASSERT(gLocaleDistance == nullptr);
    const XLikelySubtags &likely = *XLikelySubtags::getSingleton(errorCode);
    if (U_FAILURE(errorCode)) { return; }
    const LocaleDistanceData &data = likely.getDistanceData();
    if (data.distanceTrieBytes == nullptr ||
            data.regionToPartitions == nullptr || data.partitions == nullptr ||
            // ok if no paradigms
            data.distances == nullptr) {
        errorCode = U_MISSING_RESOURCE_ERROR;
        return;
    }
    gLocaleDistance = new LocaleDistance(data);
    if (gLocaleDistance == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    ucln_common_registerCleanup(UCLN_COMMON_LOCALE_DISTANCE, cleanup);
}

const LocaleDistance *LocaleDistance::getSingleton(UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return nullptr; }
    umtx_initOnce(gInitOnce, &LocaleDistance::initLocaleDistance, errorCode);
    return gLocaleDistance;
}

LocaleDistance::LocaleDistance(const LocaleDistanceData &data) :
        trie(data.distanceTrieBytes),
        regionToPartitionsIndex(data.regionToPartitions), partitionArrays(data.partitions),
        paradigmLSRs(data.paradigms), paradigmLSRsLength(data.paradigmsLength),
        defaultLanguageDistance(data.distances[IX_DEF_LANG_DISTANCE]),
        defaultScriptDistance(data.distances[IX_DEF_SCRIPT_DISTANCE]),
        defaultRegionDistance(data.distances[IX_DEF_REGION_DISTANCE]),
        minRegionDistance(data.distances[IX_MIN_REGION_DISTANCE]) {
    // For the default demotion value, use the
    // default region distance between unrelated Englishes.
    // Thus, unless demotion is turned off,
    // a mere region difference for one desired locale
    // is as good as a perfect match for the next following desired locale.
    // As of CLDR 36, we have <languageMatch desired="en_*_*" supported="en_*_*" distance="5"/>.
    LSR en("en", "Latn", "US");
    LSR enGB("en", "Latn", "GB");
    const LSR *p_enGB = &enGB;
    defaultDemotionPerDesiredLocale = getBestIndexAndDistance(en, &p_enGB, 1,
            50, ULOCMATCH_FAVOR_LANGUAGE) & 0xff;
}

int32_t LocaleDistance::getBestIndexAndDistance(
        const LSR &desired,
        const LSR **supportedLSRs, int32_t supportedLSRsLength,
        int32_t threshold, ULocMatchFavorSubtag favorSubtag) const {
    BytesTrie iter(trie);
    // Look up the desired language only once for all supported LSRs.
    // Its "distance" is either a match point value of 0, or a non-match negative value.
    // Note: The data builder verifies that there are no <*, supported> or <desired, *> rules.
    int32_t desLangDistance = trieNext(iter, desired.language, false);
    uint64_t desLangState = desLangDistance >= 0 && supportedLSRsLength > 1 ? iter.getState64() : 0;
    // Index of the supported LSR with the lowest distance.
    int32_t bestIndex = -1;
    for (int32_t slIndex = 0; slIndex < supportedLSRsLength; ++slIndex) {
        const LSR &supported = *supportedLSRs[slIndex];
        bool star = false;
        int32_t distance = desLangDistance;
        if (distance >= 0) {
            U_ASSERT((distance & DISTANCE_IS_FINAL) == 0);
            if (slIndex != 0) {
                iter.resetToState64(desLangState);
            }
            distance = trieNext(iter, supported.language, true);
        }
        // Note: The data builder verifies that there are no rules with "any" (*) language and
        // real (non *) script or region subtags.
        // This means that if the lookup for either language fails we can use
        // the default distances without further lookups.
        int32_t flags;
        if (distance >= 0) {
            flags = distance & DISTANCE_IS_FINAL_OR_SKIP_SCRIPT;
            distance &= ~DISTANCE_IS_FINAL_OR_SKIP_SCRIPT;
        } else {  // <*, *>
            if (uprv_strcmp(desired.language, supported.language) == 0) {
                distance = 0;
            } else {
                distance = defaultLanguageDistance;
            }
            flags = 0;
            star = true;
        }
        U_ASSERT(0 <= distance && distance <= 100);
        // We implement "favor subtag" by reducing the language subtag distance
        // (unscientifically reducing it to a quarter of the normal value),
        // so that the script distance is relatively more important.
        // For example, given a default language distance of 80, we reduce it to 20,
        // which is below the default threshold of 50, which is the default script distance.
        if (favorSubtag == ULOCMATCH_FAVOR_SCRIPT) {
            distance >>= 2;
        }
        if (distance >= threshold) {
            continue;
        }

        int32_t scriptDistance;
        if (star || flags != 0) {
            if (uprv_strcmp(desired.script, supported.script) == 0) {
                scriptDistance = 0;
            } else {
                scriptDistance = defaultScriptDistance;
            }
        } else {
            scriptDistance = getDesSuppScriptDistance(iter, iter.getState64(),
                    desired.script, supported.script);
            flags = scriptDistance & DISTANCE_IS_FINAL;
            scriptDistance &= ~DISTANCE_IS_FINAL;
        }
        distance += scriptDistance;
        if (distance >= threshold) {
            continue;
        }

        if (uprv_strcmp(desired.region, supported.region) == 0) {
            // regionDistance = 0
        } else if (star || (flags & DISTANCE_IS_FINAL) != 0) {
            distance += defaultRegionDistance;
        } else {
            int32_t remainingThreshold = threshold - distance;
            if (minRegionDistance >= remainingThreshold) {
                continue;
            }

            // From here on we know the regions are not equal.
            // Map each region to zero or more partitions. (zero = one non-matching string)
            // (Each array of single-character partition strings is encoded as one string.)
            // If either side has more than one, then we find the maximum distance.
            // This could be optimized by adding some more structure, but probably not worth it.
            distance += getRegionPartitionsDistance(
                    iter, iter.getState64(),
                    partitionsForRegion(desired),
                    partitionsForRegion(supported),
                    remainingThreshold);
        }
        if (distance < threshold) {
            if (distance == 0) {
                return slIndex << 8;
            }
            bestIndex = slIndex;
            threshold = distance;
        }
    }
    return bestIndex >= 0 ? (bestIndex << 8) | threshold : 0xffffff00 | ABOVE_THRESHOLD;
}

int32_t LocaleDistance::getDesSuppScriptDistance(
        BytesTrie &iter, uint64_t startState, const char *desired, const char *supported) {
    // Note: The data builder verifies that there are no <*, supported> or <desired, *> rules.
    int32_t distance = trieNext(iter, desired, false);
    if (distance >= 0) {
        distance = trieNext(iter, supported, true);
    }
    if (distance < 0) {
        UStringTrieResult result = iter.resetToState64(startState).next(u'*');  // <*, *>
        U_ASSERT(USTRINGTRIE_HAS_VALUE(result));
        if (uprv_strcmp(desired, supported) == 0) {
            distance = 0;  // same script
        } else {
            distance = iter.getValue();
            U_ASSERT(distance >= 0);
        }
        if (result == USTRINGTRIE_FINAL_VALUE) {
            distance |= DISTANCE_IS_FINAL;
        }
    }
    return distance;
}

int32_t LocaleDistance::getRegionPartitionsDistance(
        BytesTrie &iter, uint64_t startState,
        const char *desiredPartitions, const char *supportedPartitions, int32_t threshold) {
    char desired = *desiredPartitions++;
    char supported = *supportedPartitions++;
    U_ASSERT(desired != 0 && supported != 0);
    // See if we have single desired/supported partitions, from NUL-terminated
    // partition strings without explicit length.
    bool suppLengthGt1 = *supportedPartitions != 0;  // gt1: more than 1 character
    // equivalent to: if (desLength == 1 && suppLength == 1)
    if (*desiredPartitions == 0 && !suppLengthGt1) {
        // Fastpath for single desired/supported partitions.
        UStringTrieResult result = iter.next(uprv_invCharToAscii(desired) | END_OF_SUBTAG);
        if (USTRINGTRIE_HAS_NEXT(result)) {
            result = iter.next(uprv_invCharToAscii(supported) | END_OF_SUBTAG);
            if (USTRINGTRIE_HAS_VALUE(result)) {
                return iter.getValue();
            }
        }
        return getFallbackRegionDistance(iter, startState);
    }

    const char *supportedStart = supportedPartitions - 1;  // for restart of inner loop
    int32_t regionDistance = 0;
    // Fall back to * only once, not for each pair of partition strings.
    bool star = false;
    for (;;) {
        // Look up each desired-partition string only once,
        // not for each (desired, supported) pair.
        UStringTrieResult result = iter.next(uprv_invCharToAscii(desired) | END_OF_SUBTAG);
        if (USTRINGTRIE_HAS_NEXT(result)) {
            uint64_t desState = suppLengthGt1 ? iter.getState64() : 0;
            for (;;) {
                result = iter.next(uprv_invCharToAscii(supported) | END_OF_SUBTAG);
                int32_t d;
                if (USTRINGTRIE_HAS_VALUE(result)) {
                    d = iter.getValue();
                } else if (star) {
                    d = 0;
                } else {
                    d = getFallbackRegionDistance(iter, startState);
                    star = true;
                }
                if (d >= threshold) {
                    return d;
                } else if (regionDistance < d) {
                    regionDistance = d;
                }
                if ((supported = *supportedPartitions++) != 0) {
                    iter.resetToState64(desState);
                } else {
                    break;
                }
            }
        } else if (!star) {
            int32_t d = getFallbackRegionDistance(iter, startState);
            if (d >= threshold) {
                return d;
            } else if (regionDistance < d) {
                regionDistance = d;
            }
            star = true;
        }
        if ((desired = *desiredPartitions++) != 0) {
            iter.resetToState64(startState);
            supportedPartitions = supportedStart;
            supported = *supportedPartitions++;
        } else {
            break;
        }
    }
    return regionDistance;
}

int32_t LocaleDistance::getFallbackRegionDistance(BytesTrie &iter, uint64_t startState) {
#if U_DEBUG
    UStringTrieResult result =
#endif
    iter.resetToState64(startState).next(u'*');  // <*, *>
    U_ASSERT(USTRINGTRIE_HAS_VALUE(result));
    int32_t distance = iter.getValue();
    U_ASSERT(distance >= 0);
    return distance;
}

int32_t LocaleDistance::trieNext(BytesTrie &iter, const char *s, bool wantValue) {
    uint8_t c;
    if ((c = *s) == 0) {
        return -1;  // no empty subtags in the distance data
    }
    for (;;) {
        c = uprv_invCharToAscii(c);
        // EBCDIC: If *s is not an invariant character,
        // then c is now 0 and will simply not match anything, which is harmless.
        uint8_t next = *++s;
        if (next != 0) {
            if (!USTRINGTRIE_HAS_NEXT(iter.next(c))) {
                return -1;
            }
        } else {
            // last character of this subtag
            UStringTrieResult result = iter.next(c | END_OF_SUBTAG);
            if (wantValue) {
                if (USTRINGTRIE_HAS_VALUE(result)) {
                    int32_t value = iter.getValue();
                    if (result == USTRINGTRIE_FINAL_VALUE) {
                        value |= DISTANCE_IS_FINAL;
                    }
                    return value;
                }
            } else {
                if (USTRINGTRIE_HAS_NEXT(result)) {
                    return 0;
                }
            }
            return -1;
        }
        c = next;
    }
}

UBool LocaleDistance::isParadigmLSR(const LSR &lsr) const {
    // Linear search for a very short list (length 6 as of 2019).
    // If there are many paradigm LSRs we should use a hash set.
    U_ASSERT(paradigmLSRsLength <= 15);
    for (int32_t i = 0; i < paradigmLSRsLength; ++i) {
        if (lsr == paradigmLSRs[i]) { return true; }
    }
    return false;
}

U_NAMESPACE_END
