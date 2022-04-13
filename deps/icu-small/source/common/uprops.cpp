// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2002-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uprops.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002feb24
*   created by: Markus W. Scherer
*
*   Implementations for mostly non-core Unicode character properties
*   stored in uprops.icu.
*
*   With the APIs implemented here, almost all properties files and
*   their associated implementation files are used from this file,
*   including those for normalization and case mappings.
*/

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/ucptrie.h"
#include "unicode/udata.h"
#include "unicode/unorm2.h"
#include "unicode/uscript.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "cstring.h"
#include "emojiprops.h"
#include "mutex.h"
#include "normalizer2impl.h"
#include "umutex.h"
#include "ubidi_props.h"
#include "uprops.h"
#include "ucase.h"
#include "ucln_cmn.h"
#include "ulayout_props.h"
#include "ustr_imp.h"

U_NAMESPACE_USE

// Unicode text layout properties data -----------------------------------------

namespace {

icu::UInitOnce gLayoutInitOnce = U_INITONCE_INITIALIZER;
UDataMemory *gLayoutMemory = nullptr;

UCPTrie *gInpcTrie = nullptr;  // Indic_Positional_Category
UCPTrie *gInscTrie = nullptr;  // Indic_Syllabic_Category
UCPTrie *gVoTrie = nullptr;  // Vertical_Orientation

int32_t gMaxInpcValue = 0;
int32_t gMaxInscValue = 0;
int32_t gMaxVoValue = 0;

UBool U_CALLCONV uprops_cleanup() {
    udata_close(gLayoutMemory);
    gLayoutMemory = nullptr;

    ucptrie_close(gInpcTrie);
    gInpcTrie = nullptr;
    ucptrie_close(gInscTrie);
    gInscTrie = nullptr;
    ucptrie_close(gVoTrie);
    gVoTrie = nullptr;

    gMaxInpcValue = 0;
    gMaxInscValue = 0;
    gMaxVoValue = 0;

    gLayoutInitOnce.reset();
    return TRUE;
}

UBool U_CALLCONV
ulayout_isAcceptable(void * /*context*/,
                     const char * /* type */, const char * /*name*/,
                     const UDataInfo *pInfo) {
    return pInfo->size >= 20 &&
        pInfo->isBigEndian == U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily == U_CHARSET_FAMILY &&
        pInfo->dataFormat[0] == ULAYOUT_FMT_0 &&
        pInfo->dataFormat[1] == ULAYOUT_FMT_1 &&
        pInfo->dataFormat[2] == ULAYOUT_FMT_2 &&
        pInfo->dataFormat[3] == ULAYOUT_FMT_3 &&
        pInfo->formatVersion[0] == 1;
}

// UInitOnce singleton initialization function
void U_CALLCONV ulayout_load(UErrorCode &errorCode) {
    gLayoutMemory = udata_openChoice(
        nullptr, ULAYOUT_DATA_TYPE, ULAYOUT_DATA_NAME,
        ulayout_isAcceptable, nullptr, &errorCode);
    if (U_FAILURE(errorCode)) { return; }

    const uint8_t *inBytes = (const uint8_t *)udata_getMemory(gLayoutMemory);
    const int32_t *inIndexes = (const int32_t *)inBytes;
    int32_t indexesLength = inIndexes[ULAYOUT_IX_INDEXES_LENGTH];
    if (indexesLength < 12) {
        errorCode = U_INVALID_FORMAT_ERROR;  // Not enough indexes.
        return;
    }
    int32_t offset = indexesLength * 4;
    int32_t top = inIndexes[ULAYOUT_IX_INPC_TRIE_TOP];
    int32_t trieSize = top - offset;
    if (trieSize >= 16) {
        gInpcTrie = ucptrie_openFromBinary(
            UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY,
            inBytes + offset, trieSize, nullptr, &errorCode);
    }
    offset = top;
    top = inIndexes[ULAYOUT_IX_INSC_TRIE_TOP];
    trieSize = top - offset;
    if (trieSize >= 16) {
        gInscTrie = ucptrie_openFromBinary(
            UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY,
            inBytes + offset, trieSize, nullptr, &errorCode);
    }
    offset = top;
    top = inIndexes[ULAYOUT_IX_VO_TRIE_TOP];
    trieSize = top - offset;
    if (trieSize >= 16) {
        gVoTrie = ucptrie_openFromBinary(
            UCPTRIE_TYPE_ANY, UCPTRIE_VALUE_BITS_ANY,
            inBytes + offset, trieSize, nullptr, &errorCode);
    }

    uint32_t maxValues = inIndexes[ULAYOUT_IX_MAX_VALUES];
    gMaxInpcValue = maxValues >> ULAYOUT_MAX_INPC_SHIFT;
    gMaxInscValue = (maxValues >> ULAYOUT_MAX_INSC_SHIFT) & 0xff;
    gMaxVoValue = (maxValues >> ULAYOUT_MAX_VO_SHIFT) & 0xff;

    ucln_common_registerCleanup(UCLN_COMMON_UPROPS, uprops_cleanup);
}

UBool ulayout_ensureData(UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return FALSE; }
    umtx_initOnce(gLayoutInitOnce, &ulayout_load, errorCode);
    return U_SUCCESS(errorCode);
}

UBool ulayout_ensureData() {
    UErrorCode errorCode = U_ZERO_ERROR;
    return ulayout_ensureData(errorCode);
}

}  // namespace

/* general properties API functions ----------------------------------------- */

struct BinaryProperty;

typedef UBool BinaryPropertyContains(const BinaryProperty &prop, UChar32 c, UProperty which);

struct BinaryProperty {
    int32_t column;  // SRC_PROPSVEC column, or "source" if mask==0
    uint32_t mask;
    BinaryPropertyContains *contains;
};

static UBool defaultContains(const BinaryProperty &prop, UChar32 c, UProperty /*which*/) {
    /* systematic, directly stored properties */
    return (u_getUnicodeProperties(c, prop.column)&prop.mask)!=0;
}

static UBool caseBinaryPropertyContains(const BinaryProperty &/*prop*/, UChar32 c, UProperty which) {
    return static_cast<UBool>(ucase_hasBinaryProperty(c, which));
}

static UBool isBidiControl(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return ubidi_isBidiControl(c);
}

static UBool isMirrored(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return ubidi_isMirrored(c);
}

static UBool isJoinControl(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return ubidi_isJoinControl(c);
}

#if UCONFIG_NO_NORMALIZATION
static UBool hasFullCompositionExclusion(const BinaryProperty &, UChar32, UProperty) {
    return FALSE;
}
#else
static UBool hasFullCompositionExclusion(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    // By definition, Full_Composition_Exclusion is the same as NFC_QC=No.
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2Impl *impl=Normalizer2Factory::getNFCImpl(errorCode);
    return U_SUCCESS(errorCode) && impl->isCompNo(impl->getNorm16(c));
}
#endif

// UCHAR_NF*_INERT properties
#if UCONFIG_NO_NORMALIZATION
static UBool isNormInert(const BinaryProperty &, UChar32, UProperty) {
    return FALSE;
}
#else
static UBool isNormInert(const BinaryProperty &/*prop*/, UChar32 c, UProperty which) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2 *norm2=Normalizer2Factory::getInstance(
        (UNormalizationMode)(which-UCHAR_NFD_INERT+UNORM_NFD), errorCode);
    return U_SUCCESS(errorCode) && norm2->isInert(c);
}
#endif

#if UCONFIG_NO_NORMALIZATION
static UBool changesWhenCasefolded(const BinaryProperty &, UChar32, UProperty) {
    return FALSE;
}
#else
static UBool changesWhenCasefolded(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    UnicodeString nfd;
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2 *nfcNorm2=Normalizer2::getNFCInstance(errorCode);
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    if(nfcNorm2->getDecomposition(c, nfd)) {
        /* c has a decomposition */
        if(nfd.length()==1) {
            c=nfd[0];  /* single BMP code point */
        } else if(nfd.length()<=U16_MAX_LENGTH &&
                  nfd.length()==U16_LENGTH(c=nfd.char32At(0))
        ) {
            /* single supplementary code point */
        } else {
            c=U_SENTINEL;
        }
    } else if(c<0) {
        return FALSE;  /* protect against bad input */
    }
    if(c>=0) {
        /* single code point */
        const UChar *resultString;
        return (UBool)(ucase_toFullFolding(c, &resultString, U_FOLD_CASE_DEFAULT)>=0);
    } else {
        /* guess some large but stack-friendly capacity */
        UChar dest[2*UCASE_MAX_STRING_LENGTH];
        int32_t destLength;
        destLength=u_strFoldCase(dest, UPRV_LENGTHOF(dest),
                                  nfd.getBuffer(), nfd.length(),
                                  U_FOLD_CASE_DEFAULT, &errorCode);
        return (UBool)(U_SUCCESS(errorCode) &&
                       0!=u_strCompare(nfd.getBuffer(), nfd.length(),
                                       dest, destLength, FALSE));
    }
}
#endif

#if UCONFIG_NO_NORMALIZATION
static UBool changesWhenNFKC_Casefolded(const BinaryProperty &, UChar32, UProperty) {
    return FALSE;
}
#else
static UBool changesWhenNFKC_Casefolded(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2Impl *kcf=Normalizer2Factory::getNFKC_CFImpl(errorCode);
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    UnicodeString src(c);
    UnicodeString dest;
    {
        // The ReorderingBuffer must be in a block because its destructor
        // needs to release dest's buffer before we look at its contents.
        ReorderingBuffer buffer(*kcf, dest);
        // Small destCapacity for NFKC_CF(c).
        if(buffer.init(5, errorCode)) {
            const UChar *srcArray=src.getBuffer();
            kcf->compose(srcArray, srcArray+src.length(), FALSE,
                          TRUE, buffer, errorCode);
        }
    }
    return U_SUCCESS(errorCode) && dest!=src;
}
#endif

#if UCONFIG_NO_NORMALIZATION
static UBool isCanonSegmentStarter(const BinaryProperty &, UChar32, UProperty) {
    return FALSE;
}
#else
static UBool isCanonSegmentStarter(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const Normalizer2Impl *impl=Normalizer2Factory::getNFCImpl(errorCode);
    return
        U_SUCCESS(errorCode) && impl->ensureCanonIterData(errorCode) &&
        impl->isCanonSegmentStarter(c);
}
#endif

static UBool isPOSIX_alnum(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_isalnumPOSIX(c);
}

static UBool isPOSIX_blank(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_isblank(c);
}

static UBool isPOSIX_graph(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_isgraphPOSIX(c);
}

static UBool isPOSIX_print(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_isprintPOSIX(c);
}

static UBool isPOSIX_xdigit(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_isxdigit(c);
}

static UBool isRegionalIndicator(const BinaryProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    // Property starts are a subset of lb=RI etc.
    return 0x1F1E6<=c && c<=0x1F1FF;
}

static UBool hasEmojiProperty(const BinaryProperty &/*prop*/, UChar32 c, UProperty which) {
    return EmojiProps::hasBinaryProperty(c, which);
}

static const BinaryProperty binProps[UCHAR_BINARY_LIMIT]={
    /*
     * column and mask values for binary properties from u_getUnicodeProperties().
     * Must be in order of corresponding UProperty,
     * and there must be exactly one entry per binary UProperty.
     *
     * Properties with mask==0 are handled in code.
     * For them, column is the UPropertySource value.
     */
    { 1,                U_MASK(UPROPS_ALPHABETIC), defaultContains },
    { 1,                U_MASK(UPROPS_ASCII_HEX_DIGIT), defaultContains },
    { UPROPS_SRC_BIDI,  0, isBidiControl },
    { UPROPS_SRC_BIDI,  0, isMirrored },
    { 1,                U_MASK(UPROPS_DASH), defaultContains },
    { 1,                U_MASK(UPROPS_DEFAULT_IGNORABLE_CODE_POINT), defaultContains },
    { 1,                U_MASK(UPROPS_DEPRECATED), defaultContains },
    { 1,                U_MASK(UPROPS_DIACRITIC), defaultContains },
    { 1,                U_MASK(UPROPS_EXTENDER), defaultContains },
    { UPROPS_SRC_NFC,   0, hasFullCompositionExclusion },
    { 1,                U_MASK(UPROPS_GRAPHEME_BASE), defaultContains },
    { 1,                U_MASK(UPROPS_GRAPHEME_EXTEND), defaultContains },
    { 1,                U_MASK(UPROPS_GRAPHEME_LINK), defaultContains },
    { 1,                U_MASK(UPROPS_HEX_DIGIT), defaultContains },
    { 1,                U_MASK(UPROPS_HYPHEN), defaultContains },
    { 1,                U_MASK(UPROPS_ID_CONTINUE), defaultContains },
    { 1,                U_MASK(UPROPS_ID_START), defaultContains },
    { 1,                U_MASK(UPROPS_IDEOGRAPHIC), defaultContains },
    { 1,                U_MASK(UPROPS_IDS_BINARY_OPERATOR), defaultContains },
    { 1,                U_MASK(UPROPS_IDS_TRINARY_OPERATOR), defaultContains },
    { UPROPS_SRC_BIDI,  0, isJoinControl },
    { 1,                U_MASK(UPROPS_LOGICAL_ORDER_EXCEPTION), defaultContains },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_LOWERCASE
    { 1,                U_MASK(UPROPS_MATH), defaultContains },
    { 1,                U_MASK(UPROPS_NONCHARACTER_CODE_POINT), defaultContains },
    { 1,                U_MASK(UPROPS_QUOTATION_MARK), defaultContains },
    { 1,                U_MASK(UPROPS_RADICAL), defaultContains },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_SOFT_DOTTED
    { 1,                U_MASK(UPROPS_TERMINAL_PUNCTUATION), defaultContains },
    { 1,                U_MASK(UPROPS_UNIFIED_IDEOGRAPH), defaultContains },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_UPPERCASE
    { 1,                U_MASK(UPROPS_WHITE_SPACE), defaultContains },
    { 1,                U_MASK(UPROPS_XID_CONTINUE), defaultContains },
    { 1,                U_MASK(UPROPS_XID_START), defaultContains },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CASE_SENSITIVE
    { 1,                U_MASK(UPROPS_S_TERM), defaultContains },
    { 1,                U_MASK(UPROPS_VARIATION_SELECTOR), defaultContains },
    { UPROPS_SRC_NFC,   0, isNormInert },  // UCHAR_NFD_INERT
    { UPROPS_SRC_NFKC,  0, isNormInert },  // UCHAR_NFKD_INERT
    { UPROPS_SRC_NFC,   0, isNormInert },  // UCHAR_NFC_INERT
    { UPROPS_SRC_NFKC,  0, isNormInert },  // UCHAR_NFKC_INERT
    { UPROPS_SRC_NFC_CANON_ITER, 0, isCanonSegmentStarter },
    { 1,                U_MASK(UPROPS_PATTERN_SYNTAX), defaultContains },
    { 1,                U_MASK(UPROPS_PATTERN_WHITE_SPACE), defaultContains },
    { UPROPS_SRC_CHAR_AND_PROPSVEC,  0, isPOSIX_alnum },
    { UPROPS_SRC_CHAR,  0, isPOSIX_blank },
    { UPROPS_SRC_CHAR,  0, isPOSIX_graph },
    { UPROPS_SRC_CHAR,  0, isPOSIX_print },
    { UPROPS_SRC_CHAR,  0, isPOSIX_xdigit },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CASED
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CASE_IGNORABLE
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CHANGES_WHEN_LOWERCASED
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CHANGES_WHEN_UPPERCASED
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CHANGES_WHEN_TITLECASED
    { UPROPS_SRC_CASE_AND_NORM,  0, changesWhenCasefolded },
    { UPROPS_SRC_CASE,  0, caseBinaryPropertyContains },  // UCHAR_CHANGES_WHEN_CASEMAPPED
    { UPROPS_SRC_NFKC_CF, 0, changesWhenNFKC_Casefolded },
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI_PRESENTATION
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI_MODIFIER
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI_MODIFIER_BASE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI_COMPONENT
    { 2,                0, isRegionalIndicator },
    { 1,                U_MASK(UPROPS_PREPENDED_CONCATENATION_MARK), defaultContains },
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EXTENDED_PICTOGRAPHIC
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_BASIC_EMOJI
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_EMOJI_KEYCAP_SEQUENCE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_RGI_EMOJI_FLAG_SEQUENCE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_RGI_EMOJI_TAG_SEQUENCE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_RGI_EMOJI_ZWJ_SEQUENCE
    { UPROPS_SRC_EMOJI, 0, hasEmojiProperty },  // UCHAR_RGI_EMOJI
};

U_CAPI UBool U_EXPORT2
u_hasBinaryProperty(UChar32 c, UProperty which) {
    /* c is range-checked in the functions that are called from here */
    if(which<UCHAR_BINARY_START || UCHAR_BINARY_LIMIT<=which) {
        /* not a known binary property */
        return FALSE;
    } else {
        const BinaryProperty &prop=binProps[which];
        return prop.contains(prop, c, which);
    }
}

U_CAPI UBool U_EXPORT2
u_stringHasBinaryProperty(const UChar *s, int32_t length, UProperty which) {
    if (s == nullptr && length != 0) { return false; }
    if (length == 1) {
        return u_hasBinaryProperty(s[0], which);  // single code point
    } else if (length == 2 || (length < 0 && *s != 0)) {  // not empty string
        // first code point
        int32_t i = 0;
        UChar32 c;
        U16_NEXT(s, i, length, c);
        if (length > 0 ? i == length : s[i] == 0) {
            return u_hasBinaryProperty(c, which);  // single code point
        }
    }
    // Only call into EmojiProps for a relevant property,
    // so that we not unnecessarily try to load its data file.
    return UCHAR_BASIC_EMOJI <= which && which <= UCHAR_RGI_EMOJI &&
        EmojiProps::hasBinaryProperty(s, length, which);
}

struct IntProperty;

typedef int32_t IntPropertyGetValue(const IntProperty &prop, UChar32 c, UProperty which);
typedef int32_t IntPropertyGetMaxValue(const IntProperty &prop, UProperty which);

struct IntProperty {
    int32_t column;  // SRC_PROPSVEC column, or "source" if mask==0
    uint32_t mask;
    int32_t shift;  // =maxValue if getMaxValueFromShift() is used
    IntPropertyGetValue *getValue;
    IntPropertyGetMaxValue *getMaxValue;
};

static int32_t defaultGetValue(const IntProperty &prop, UChar32 c, UProperty /*which*/) {
    /* systematic, directly stored properties */
    return (int32_t)(u_getUnicodeProperties(c, prop.column)&prop.mask)>>prop.shift;
}

static int32_t defaultGetMaxValue(const IntProperty &prop, UProperty /*which*/) {
    return (uprv_getMaxValues(prop.column)&prop.mask)>>prop.shift;
}

static int32_t getMaxValueFromShift(const IntProperty &prop, UProperty /*which*/) {
    return prop.shift;
}

static int32_t getBiDiClass(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return (int32_t)u_charDirection(c);
}

static int32_t getBiDiPairedBracketType(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return (int32_t)ubidi_getPairedBracketType(c);
}

static int32_t biDiGetMaxValue(const IntProperty &/*prop*/, UProperty which) {
    return ubidi_getMaxValue(which);
}

#if UCONFIG_NO_NORMALIZATION
static int32_t getCombiningClass(const IntProperty &, UChar32, UProperty) {
    return 0;
}
#else
static int32_t getCombiningClass(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return u_getCombiningClass(c);
}
#endif

static int32_t getGeneralCategory(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return (int32_t)u_charType(c);
}

static int32_t getJoiningGroup(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return ubidi_getJoiningGroup(c);
}

static int32_t getJoiningType(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return ubidi_getJoiningType(c);
}

static int32_t getNumericType(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    int32_t ntv=(int32_t)GET_NUMERIC_TYPE_VALUE(u_getMainProperties(c));
    return UPROPS_NTV_GET_TYPE(ntv);
}

static int32_t getScript(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    UErrorCode errorCode=U_ZERO_ERROR;
    return (int32_t)uscript_getScript(c, &errorCode);
}

static int32_t scriptGetMaxValue(const IntProperty &/*prop*/, UProperty /*which*/) {
    uint32_t scriptX=uprv_getMaxValues(0)&UPROPS_SCRIPT_X_MASK;
    return uprops_mergeScriptCodeOrIndex(scriptX);
}

/*
 * Map some of the Grapheme Cluster Break values to Hangul Syllable Types.
 * Hangul_Syllable_Type is fully redundant with a subset of Grapheme_Cluster_Break.
 */
static const UHangulSyllableType gcbToHst[]={
    U_HST_NOT_APPLICABLE,   /* U_GCB_OTHER */
    U_HST_NOT_APPLICABLE,   /* U_GCB_CONTROL */
    U_HST_NOT_APPLICABLE,   /* U_GCB_CR */
    U_HST_NOT_APPLICABLE,   /* U_GCB_EXTEND */
    U_HST_LEADING_JAMO,     /* U_GCB_L */
    U_HST_NOT_APPLICABLE,   /* U_GCB_LF */
    U_HST_LV_SYLLABLE,      /* U_GCB_LV */
    U_HST_LVT_SYLLABLE,     /* U_GCB_LVT */
    U_HST_TRAILING_JAMO,    /* U_GCB_T */
    U_HST_VOWEL_JAMO        /* U_GCB_V */
    /*
     * Omit GCB values beyond what we need for hst.
     * The code below checks for the array length.
     */
};

static int32_t getHangulSyllableType(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    /* see comments on gcbToHst[] above */
    int32_t gcb=(int32_t)(u_getUnicodeProperties(c, 2)&UPROPS_GCB_MASK)>>UPROPS_GCB_SHIFT;
    if(gcb<UPRV_LENGTHOF(gcbToHst)) {
        return gcbToHst[gcb];
    } else {
        return U_HST_NOT_APPLICABLE;
    }
}

#if UCONFIG_NO_NORMALIZATION
static int32_t getNormQuickCheck(const IntProperty &, UChar32, UProperty) {
    return 0;
}
#else
static int32_t getNormQuickCheck(const IntProperty &/*prop*/, UChar32 c, UProperty which) {
    return (int32_t)unorm_getQuickCheck(c, (UNormalizationMode)(which-UCHAR_NFD_QUICK_CHECK+UNORM_NFD));
}
#endif

#if UCONFIG_NO_NORMALIZATION
static int32_t getLeadCombiningClass(const IntProperty &, UChar32, UProperty) {
    return 0;
}
#else
static int32_t getLeadCombiningClass(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return unorm_getFCD16(c)>>8;
}
#endif

#if UCONFIG_NO_NORMALIZATION
static int32_t getTrailCombiningClass(const IntProperty &, UChar32, UProperty) {
    return 0;
}
#else
static int32_t getTrailCombiningClass(const IntProperty &/*prop*/, UChar32 c, UProperty /*which*/) {
    return unorm_getFCD16(c)&0xff;
}
#endif

static int32_t getInPC(const IntProperty &, UChar32 c, UProperty) {
    return ulayout_ensureData() && gInpcTrie != nullptr ? ucptrie_get(gInpcTrie, c) : 0;
}

static int32_t getInSC(const IntProperty &, UChar32 c, UProperty) {
    return ulayout_ensureData() && gInscTrie != nullptr ? ucptrie_get(gInscTrie, c) : 0;
}

static int32_t getVo(const IntProperty &, UChar32 c, UProperty) {
    return ulayout_ensureData() && gVoTrie != nullptr ? ucptrie_get(gVoTrie, c) : 0;
}

static int32_t layoutGetMaxValue(const IntProperty &/*prop*/, UProperty which) {
    if (!ulayout_ensureData()) { return 0; }
    switch (which) {
    case UCHAR_INDIC_POSITIONAL_CATEGORY:
        return gMaxInpcValue;
    case UCHAR_INDIC_SYLLABIC_CATEGORY:
        return gMaxInscValue;
    case UCHAR_VERTICAL_ORIENTATION:
        return gMaxVoValue;
    default:
        return 0;
    }
}

static const IntProperty intProps[UCHAR_INT_LIMIT-UCHAR_INT_START]={
    /*
     * column, mask and shift values for int-value properties from u_getUnicodeProperties().
     * Must be in order of corresponding UProperty,
     * and there must be exactly one entry per int UProperty.
     *
     * Properties with mask==0 are handled in code.
     * For them, column is the UPropertySource value.
     */
    { UPROPS_SRC_BIDI,  0, 0,                               getBiDiClass, biDiGetMaxValue },
    { 0,                UPROPS_BLOCK_MASK, UPROPS_BLOCK_SHIFT, defaultGetValue, defaultGetMaxValue },
    { UPROPS_SRC_NFC,   0, 0xff,                            getCombiningClass, getMaxValueFromShift },
    { 2,                UPROPS_DT_MASK, 0,                  defaultGetValue, defaultGetMaxValue },
    { 0,                UPROPS_EA_MASK, UPROPS_EA_SHIFT,    defaultGetValue, defaultGetMaxValue },
    { UPROPS_SRC_CHAR,  0, (int32_t)U_CHAR_CATEGORY_COUNT-1,getGeneralCategory, getMaxValueFromShift },
    { UPROPS_SRC_BIDI,  0, 0,                               getJoiningGroup, biDiGetMaxValue },
    { UPROPS_SRC_BIDI,  0, 0,                               getJoiningType, biDiGetMaxValue },
    { 2,                UPROPS_LB_MASK, UPROPS_LB_SHIFT,    defaultGetValue, defaultGetMaxValue },
    { UPROPS_SRC_CHAR,  0, (int32_t)U_NT_COUNT-1,           getNumericType, getMaxValueFromShift },
    { UPROPS_SRC_PROPSVEC, 0, 0,                            getScript, scriptGetMaxValue },
    { UPROPS_SRC_PROPSVEC, 0, (int32_t)U_HST_COUNT-1,       getHangulSyllableType, getMaxValueFromShift },
    // UCHAR_NFD_QUICK_CHECK: max=1=YES -- never "maybe", only "no" or "yes"
    { UPROPS_SRC_NFC,   0, (int32_t)UNORM_YES,              getNormQuickCheck, getMaxValueFromShift },
    // UCHAR_NFKD_QUICK_CHECK: max=1=YES -- never "maybe", only "no" or "yes"
    { UPROPS_SRC_NFKC,  0, (int32_t)UNORM_YES,              getNormQuickCheck, getMaxValueFromShift },
    // UCHAR_NFC_QUICK_CHECK: max=2=MAYBE
    { UPROPS_SRC_NFC,   0, (int32_t)UNORM_MAYBE,            getNormQuickCheck, getMaxValueFromShift },
    // UCHAR_NFKC_QUICK_CHECK: max=2=MAYBE
    { UPROPS_SRC_NFKC,  0, (int32_t)UNORM_MAYBE,            getNormQuickCheck, getMaxValueFromShift },
    { UPROPS_SRC_NFC,   0, 0xff,                            getLeadCombiningClass, getMaxValueFromShift },
    { UPROPS_SRC_NFC,   0, 0xff,                            getTrailCombiningClass, getMaxValueFromShift },
    { 2,                UPROPS_GCB_MASK, UPROPS_GCB_SHIFT,  defaultGetValue, defaultGetMaxValue },
    { 2,                UPROPS_SB_MASK, UPROPS_SB_SHIFT,    defaultGetValue, defaultGetMaxValue },
    { 2,                UPROPS_WB_MASK, UPROPS_WB_SHIFT,    defaultGetValue, defaultGetMaxValue },
    { UPROPS_SRC_BIDI,  0, 0,                               getBiDiPairedBracketType, biDiGetMaxValue },
    { UPROPS_SRC_INPC,  0, 0,                               getInPC, layoutGetMaxValue },
    { UPROPS_SRC_INSC,  0, 0,                               getInSC, layoutGetMaxValue },
    { UPROPS_SRC_VO,    0, 0,                               getVo, layoutGetMaxValue },
};

U_CAPI int32_t U_EXPORT2
u_getIntPropertyValue(UChar32 c, UProperty which) {
    if(which<UCHAR_INT_START) {
        if(UCHAR_BINARY_START<=which && which<UCHAR_BINARY_LIMIT) {
            const BinaryProperty &prop=binProps[which];
            return prop.contains(prop, c, which);
        }
    } else if(which<UCHAR_INT_LIMIT) {
        const IntProperty &prop=intProps[which-UCHAR_INT_START];
        return prop.getValue(prop, c, which);
    } else if(which==UCHAR_GENERAL_CATEGORY_MASK) {
        return U_MASK(u_charType(c));
    }
    return 0;  // undefined
}

U_CAPI int32_t U_EXPORT2
u_getIntPropertyMinValue(UProperty /*which*/) {
    return 0; /* all binary/enum/int properties have a minimum value of 0 */
}

U_CAPI int32_t U_EXPORT2
u_getIntPropertyMaxValue(UProperty which) {
    if(which<UCHAR_INT_START) {
        if(UCHAR_BINARY_START<=which && which<UCHAR_BINARY_LIMIT) {
            return 1;  // maximum TRUE for all binary properties
        }
    } else if(which<UCHAR_INT_LIMIT) {
        const IntProperty &prop=intProps[which-UCHAR_INT_START];
        return prop.getMaxValue(prop, which);
    }
    return -1;  // undefined
}

U_CFUNC UPropertySource U_EXPORT2
uprops_getSource(UProperty which) {
    if(which<UCHAR_BINARY_START) {
        return UPROPS_SRC_NONE; /* undefined */
    } else if(which<UCHAR_BINARY_LIMIT) {
        const BinaryProperty &prop=binProps[which];
        if(prop.mask!=0) {
            return UPROPS_SRC_PROPSVEC;
        } else {
            return (UPropertySource)prop.column;
        }
    } else if(which<UCHAR_INT_START) {
        return UPROPS_SRC_NONE; /* undefined */
    } else if(which<UCHAR_INT_LIMIT) {
        const IntProperty &prop=intProps[which-UCHAR_INT_START];
        if(prop.mask!=0) {
            return UPROPS_SRC_PROPSVEC;
        } else {
            return (UPropertySource)prop.column;
        }
    } else if(which<UCHAR_STRING_START) {
        switch(which) {
        case UCHAR_GENERAL_CATEGORY_MASK:
        case UCHAR_NUMERIC_VALUE:
            return UPROPS_SRC_CHAR;

        default:
            return UPROPS_SRC_NONE;
        }
    } else if(which<UCHAR_STRING_LIMIT) {
        switch(which) {
        case UCHAR_AGE:
            return UPROPS_SRC_PROPSVEC;

        case UCHAR_BIDI_MIRRORING_GLYPH:
            return UPROPS_SRC_BIDI;

        case UCHAR_CASE_FOLDING:
        case UCHAR_LOWERCASE_MAPPING:
        case UCHAR_SIMPLE_CASE_FOLDING:
        case UCHAR_SIMPLE_LOWERCASE_MAPPING:
        case UCHAR_SIMPLE_TITLECASE_MAPPING:
        case UCHAR_SIMPLE_UPPERCASE_MAPPING:
        case UCHAR_TITLECASE_MAPPING:
        case UCHAR_UPPERCASE_MAPPING:
            return UPROPS_SRC_CASE;

        case UCHAR_ISO_COMMENT:
        case UCHAR_NAME:
        case UCHAR_UNICODE_1_NAME:
            return UPROPS_SRC_NAMES;

        default:
            return UPROPS_SRC_NONE;
        }
    } else {
        switch(which) {
        case UCHAR_SCRIPT_EXTENSIONS:
            return UPROPS_SRC_PROPSVEC;
        default:
            return UPROPS_SRC_NONE; /* undefined */
        }
    }
}

U_CFUNC void U_EXPORT2
uprops_addPropertyStarts(UPropertySource src, const USetAdder *sa, UErrorCode *pErrorCode) {
    if (!ulayout_ensureData(*pErrorCode)) { return; }
    const UCPTrie *trie;
    switch (src) {
    case UPROPS_SRC_INPC:
        trie = gInpcTrie;
        break;
    case UPROPS_SRC_INSC:
        trie = gInscTrie;
        break;
    case UPROPS_SRC_VO:
        trie = gVoTrie;
        break;
    default:
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (trie == nullptr) {
        *pErrorCode = U_MISSING_RESOURCE_ERROR;
        return;
    }

    // Add the start code point of each same-value range of the trie.
    UChar32 start = 0, end;
    while ((end = ucptrie_getRange(trie, start, UCPMAP_RANGE_NORMAL, 0,
                                   nullptr, nullptr, nullptr)) >= 0) {
        sa->add(sa->set, start);
        start = end + 1;
    }
}

#if !UCONFIG_NO_NORMALIZATION

U_CAPI int32_t U_EXPORT2
u_getFC_NFKC_Closure(UChar32 c, UChar *dest, int32_t destCapacity, UErrorCode *pErrorCode) {
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(destCapacity<0 || (dest==NULL && destCapacity>0)) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    // Compute the FC_NFKC_Closure on the fly:
    // We have the API for complete coverage of Unicode properties, although
    // this value by itself is not useful via API.
    // (What could be useful is a custom normalization table that combines
    // case folding and NFKC.)
    // For the derivation, see Unicode's DerivedNormalizationProps.txt.
    const Normalizer2 *nfkc=Normalizer2::getNFKCInstance(*pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    // first: b = NFKC(Fold(a))
    UnicodeString folded1String;
    const UChar *folded1;
    int32_t folded1Length=ucase_toFullFolding(c, &folded1, U_FOLD_CASE_DEFAULT);
    if(folded1Length<0) {
        const Normalizer2Impl *nfkcImpl=Normalizer2Factory::getImpl(nfkc);
        if(nfkcImpl->getCompQuickCheck(nfkcImpl->getNorm16(c))!=UNORM_NO) {
            return u_terminateUChars(dest, destCapacity, 0, pErrorCode);  // c does not change at all under CaseFolding+NFKC
        }
        folded1String.setTo(c);
    } else {
        if(folded1Length>UCASE_MAX_STRING_LENGTH) {
            folded1String.setTo(folded1Length);
        } else {
            folded1String.setTo(FALSE, folded1, folded1Length);
        }
    }
    UnicodeString kc1=nfkc->normalize(folded1String, *pErrorCode);
    // second: c = NFKC(Fold(b))
    UnicodeString folded2String(kc1);
    UnicodeString kc2=nfkc->normalize(folded2String.foldCase(), *pErrorCode);
    // if (c != b) add the mapping from a to c
    if(U_FAILURE(*pErrorCode) || kc1==kc2) {
        return u_terminateUChars(dest, destCapacity, 0, pErrorCode);
    } else {
        return kc2.extract(dest, destCapacity, *pErrorCode);
    }
}

#endif
