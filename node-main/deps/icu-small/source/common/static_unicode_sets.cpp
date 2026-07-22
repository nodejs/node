// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "static_unicode_sets.h"
#include "umutex.h"
#include "ucln_cmn.h"
#include "unicode/uniset.h"
#include "uresimp.h"
#include "cstring.h"
#include "uassert.h"

using namespace icu;
using namespace icu::unisets;


namespace {

UnicodeSet* gUnicodeSets[UNISETS_KEY_COUNT] = {};

// Save the empty instance in static memory to have well-defined behavior if a
// regular UnicodeSet cannot be allocated.
alignas(UnicodeSet)
char gEmptyUnicodeSet[sizeof(UnicodeSet)];

// Whether the gEmptyUnicodeSet is initialized and ready to use.
UBool gEmptyUnicodeSetInitialized = false;

inline UnicodeSet* getImpl(Key key) {
    UnicodeSet* candidate = gUnicodeSets[key];
    if (candidate == nullptr) {
        return reinterpret_cast<UnicodeSet*>(gEmptyUnicodeSet);
    }
    return candidate;
}

UnicodeSet* computeUnion(Key k1, Key k2) {
    UnicodeSet* result = new UnicodeSet();
    if (result == nullptr) {
        return nullptr;
    }
    result->addAll(*getImpl(k1));
    result->addAll(*getImpl(k2));
    result->freeze();
    return result;
}

UnicodeSet* computeUnion(Key k1, Key k2, Key k3) {
    UnicodeSet* result = new UnicodeSet();
    if (result == nullptr) {
        return nullptr;
    }
    result->addAll(*getImpl(k1));
    result->addAll(*getImpl(k2));
    result->addAll(*getImpl(k3));
    result->freeze();
    return result;
}


void saveSet(Key key, const UnicodeString& unicodeSetPattern, UErrorCode& status) {
    // assert unicodeSets.get(key) == null;
    gUnicodeSets[key] = new UnicodeSet(unicodeSetPattern, status);
}

class ParseDataSink : public ResourceSink {
  public:
    void put(const char* key, ResourceValue& value, UBool /*noFallback*/, UErrorCode& status) override {
        ResourceTable contextsTable = value.getTable(status);
        if (U_FAILURE(status)) { return; }
        for (int i = 0; contextsTable.getKeyAndValue(i, key, value); i++) {
            if (uprv_strcmp(key, "date") == 0) {
                // ignore
            } else {
                ResourceTable strictnessTable = value.getTable(status);
                if (U_FAILURE(status)) { return; }
                for (int j = 0; strictnessTable.getKeyAndValue(j, key, value); j++) {
                    bool isLenient = (uprv_strcmp(key, "lenient") == 0);
                    ResourceArray array = value.getArray(status);
                    if (U_FAILURE(status)) { return; }
                    for (int k = 0; k < array.getSize(); k++) {
                        array.getValue(k, value);
                        UnicodeString str = value.getUnicodeString(status);
                        if (U_FAILURE(status)) { return; }
                        // There is both lenient and strict data for comma/period,
                        // but not for any of the other symbols.
                        if (str.indexOf(u'.') != -1) {
                            saveSet(isLenient ? PERIOD : STRICT_PERIOD, str, status);
                        } else if (str.indexOf(u',') != -1) {
                            saveSet(isLenient ? COMMA : STRICT_COMMA, str, status);
                        } else if (str.indexOf(u'+') != -1) {
                            saveSet(PLUS_SIGN, str, status);
                        } else if (str.indexOf(u'-') != -1) {
                            saveSet(MINUS_SIGN, str, status);
                        } else if (str.indexOf(u'$') != -1) {
                            saveSet(DOLLAR_SIGN, str, status);
                        } else if (str.indexOf(u'£') != -1) {
                            saveSet(POUND_SIGN, str, status);
                        } else if (str.indexOf(u'₹') != -1) {
                            saveSet(RUPEE_SIGN, str, status);
                        } else if (str.indexOf(u'¥') != -1) {
                            saveSet(YEN_SIGN, str, status);
                        } else if (str.indexOf(u'₩') != -1) {
                            saveSet(WON_SIGN, str, status);
                        } else if (str.indexOf(u'%') != -1) {
                            saveSet(PERCENT_SIGN, str, status);
                        } else if (str.indexOf(u'‰') != -1) {
                            saveSet(PERMILLE_SIGN, str, status);
                        } else if (str.indexOf(u'’') != -1) {
                            saveSet(APOSTROPHE_SIGN, str, status);
                        } else {
                            // Unknown class of parse lenients
                            // TODO(ICU-20428): Make ICU automatically accept new classes?
                            U_ASSERT(false);
                        }
                        if (U_FAILURE(status)) { return; }
                    }
                }
            }
        }
    }
};


icu::UInitOnce gNumberParseUniSetsInitOnce {};

UBool U_CALLCONV cleanupNumberParseUniSets() {
    if (gEmptyUnicodeSetInitialized) {
        reinterpret_cast<UnicodeSet*>(gEmptyUnicodeSet)->~UnicodeSet();
        gEmptyUnicodeSetInitialized = false;
    }
    for (int32_t i = 0; i < UNISETS_KEY_COUNT; i++) {
        delete gUnicodeSets[i];
        gUnicodeSets[i] = nullptr;
    }
    gNumberParseUniSetsInitOnce.reset();
    return true;
}

void U_CALLCONV initNumberParseUniSets(UErrorCode& status) {
    ucln_common_registerCleanup(UCLN_COMMON_NUMPARSE_UNISETS, cleanupNumberParseUniSets);

    // Initialize the empty instance for well-defined fallback behavior
    new(gEmptyUnicodeSet) UnicodeSet();
    reinterpret_cast<UnicodeSet*>(gEmptyUnicodeSet)->freeze();
    gEmptyUnicodeSetInitialized = true;

    // These sets were decided after discussion with icu-design@. See tickets #13084 and #13309.
    // Zs+TAB is "horizontal whitespace" according to UTS #18 (blank property).
    gUnicodeSets[DEFAULT_IGNORABLES] = new UnicodeSet(
            u"[[:Zs:][\\u0009][:Bidi_Control:][:Variation_Selector:]]", status);
    gUnicodeSets[STRICT_IGNORABLES] = new UnicodeSet(u"[[:Bidi_Control:]]", status);

    LocalUResourceBundlePointer rb(ures_open(nullptr, "root", &status));
    if (U_FAILURE(status)) { return; }
    ParseDataSink sink;
    ures_getAllItemsWithFallback(rb.getAlias(), "parse", sink, status);
    if (U_FAILURE(status)) { return; }

    // NOTE: It is OK for these assertions to fail if there was a no-data build.
    U_ASSERT(gUnicodeSets[COMMA] != nullptr);
    U_ASSERT(gUnicodeSets[STRICT_COMMA] != nullptr);
    U_ASSERT(gUnicodeSets[PERIOD] != nullptr);
    U_ASSERT(gUnicodeSets[STRICT_PERIOD] != nullptr);
    U_ASSERT(gUnicodeSets[APOSTROPHE_SIGN] != nullptr);

    LocalPointer<UnicodeSet> otherGrouping(new UnicodeSet(
        u"[٬‘\\u0020\\u00A0\\u2000-\\u200A\\u202F\\u205F\\u3000]",
        status
    ), status);
    if (U_FAILURE(status)) { return; }
    otherGrouping->addAll(*gUnicodeSets[APOSTROPHE_SIGN]);
    gUnicodeSets[OTHER_GROUPING_SEPARATORS] = otherGrouping.orphan();
    gUnicodeSets[ALL_SEPARATORS] = computeUnion(COMMA, PERIOD, OTHER_GROUPING_SEPARATORS);
    gUnicodeSets[STRICT_ALL_SEPARATORS] = computeUnion(
            STRICT_COMMA, STRICT_PERIOD, OTHER_GROUPING_SEPARATORS);

    U_ASSERT(gUnicodeSets[MINUS_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[PLUS_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[PERCENT_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[PERMILLE_SIGN] != nullptr);

    gUnicodeSets[INFINITY_SIGN] = new UnicodeSet(u"[∞]", status);
    if (U_FAILURE(status)) { return; }

    U_ASSERT(gUnicodeSets[DOLLAR_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[POUND_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[RUPEE_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[YEN_SIGN] != nullptr);
    U_ASSERT(gUnicodeSets[WON_SIGN] != nullptr);

    gUnicodeSets[DIGITS] = new UnicodeSet(u"[:digit:]", status);
    if (U_FAILURE(status)) { return; }
    gUnicodeSets[DIGITS_OR_ALL_SEPARATORS] = computeUnion(DIGITS, ALL_SEPARATORS);
    gUnicodeSets[DIGITS_OR_STRICT_ALL_SEPARATORS] = computeUnion(DIGITS, STRICT_ALL_SEPARATORS);

    for (auto* uniset : gUnicodeSets) {
        if (uniset != nullptr) {
            uniset->freeze();
        }
    }
}

}

const UnicodeSet* unisets::get(Key key) {
    UErrorCode localStatus = U_ZERO_ERROR;
    umtx_initOnce(gNumberParseUniSetsInitOnce, &initNumberParseUniSets, localStatus);
    if (U_FAILURE(localStatus)) {
        return reinterpret_cast<UnicodeSet*>(gEmptyUnicodeSet);
    }
    return getImpl(key);
}

Key unisets::chooseFrom(UnicodeString str, Key key1) {
    return get(key1)->contains(str) ? key1 : NONE;
}

Key unisets::chooseFrom(UnicodeString str, Key key1, Key key2) {
    return get(key1)->contains(str) ? key1 : chooseFrom(str, key2);
}

//Key unisets::chooseCurrency(UnicodeString str) {
//    if (get(DOLLAR_SIGN)->contains(str)) {
//        return DOLLAR_SIGN;
//    } else if (get(POUND_SIGN)->contains(str)) {
//        return POUND_SIGN;
//    } else if (get(RUPEE_SIGN)->contains(str)) {
//        return RUPEE_SIGN;
//    } else if (get(YEN_SIGN)->contains(str)) {
//        return YEN_SIGN;
//    } else {
//        return NONE;
//    }
//}


#endif /* #if !UCONFIG_NO_FORMATTING */
