// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2016, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucsdet.h"

#include "csdetect.h"
#include "csmatch.h"
#include "uenumimp.h"

#include "cmemory.h"
#include "cstring.h"
#include "umutex.h"
#include "ucln_in.h"
#include "uarrsort.h"
#include "inputext.h"
#include "csrsbcs.h"
#include "csrmbcs.h"
#include "csrutf8.h"
#include "csrucode.h"
#include "csr2022.h"

#define NEW_ARRAY(type,count) (type *) uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))

U_NAMESPACE_BEGIN

struct CSRecognizerInfo : public UMemory {
    CSRecognizerInfo(CharsetRecognizer *recognizer, UBool isDefaultEnabled)
        : recognizer(recognizer), isDefaultEnabled(isDefaultEnabled) {}

    ~CSRecognizerInfo() {delete recognizer;}

    CharsetRecognizer *recognizer;
    UBool isDefaultEnabled;
};

U_NAMESPACE_END

static icu::CSRecognizerInfo **fCSRecognizers = NULL;
static icu::UInitOnce gCSRecognizersInitOnce = U_INITONCE_INITIALIZER;
static int32_t fCSRecognizers_size = 0;

U_CDECL_BEGIN
static UBool U_CALLCONV csdet_cleanup(void)
{
    U_NAMESPACE_USE
    if (fCSRecognizers != NULL) {
        for(int32_t r = 0; r < fCSRecognizers_size; r += 1) {
            delete fCSRecognizers[r];
            fCSRecognizers[r] = NULL;
        }

        DELETE_ARRAY(fCSRecognizers);
        fCSRecognizers = NULL;
        fCSRecognizers_size = 0;
    }
    gCSRecognizersInitOnce.reset();

    return TRUE;
}

static int32_t U_CALLCONV
charsetMatchComparator(const void * /*context*/, const void *left, const void *right)
{
    U_NAMESPACE_USE

    const CharsetMatch **csm_l = (const CharsetMatch **) left;
    const CharsetMatch **csm_r = (const CharsetMatch **) right;

    // NOTE: compare is backwards to sort from highest to lowest.
    return (*csm_r)->getConfidence() - (*csm_l)->getConfidence();
}

static void U_CALLCONV initRecognizers(UErrorCode &status) {
    U_NAMESPACE_USE
    ucln_i18n_registerCleanup(UCLN_I18N_CSDET, csdet_cleanup);
    CSRecognizerInfo *tempArray[] = {
        new CSRecognizerInfo(new CharsetRecog_UTF8(), TRUE),

        new CSRecognizerInfo(new CharsetRecog_UTF_16_BE(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_UTF_16_LE(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_UTF_32_BE(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_UTF_32_LE(), TRUE),

        new CSRecognizerInfo(new CharsetRecog_8859_1(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_2(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_5_ru(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_6_ar(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_7_el(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_8_I_he(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_8_he(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_windows_1251(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_windows_1256(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_KOI8_R(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_8859_9_tr(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_sjis(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_gb_18030(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_euc_jp(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_euc_kr(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_big5(), TRUE),

        new CSRecognizerInfo(new CharsetRecog_2022JP(), TRUE),
#if !UCONFIG_ONLY_HTML_CONVERSION
        new CSRecognizerInfo(new CharsetRecog_2022KR(), TRUE),
        new CSRecognizerInfo(new CharsetRecog_2022CN(), TRUE),

        new CSRecognizerInfo(new CharsetRecog_IBM424_he_rtl(), FALSE),
        new CSRecognizerInfo(new CharsetRecog_IBM424_he_ltr(), FALSE),
        new CSRecognizerInfo(new CharsetRecog_IBM420_ar_rtl(), FALSE),
        new CSRecognizerInfo(new CharsetRecog_IBM420_ar_ltr(), FALSE)
#endif
    };
    int32_t rCount = UPRV_LENGTHOF(tempArray);

    fCSRecognizers = NEW_ARRAY(CSRecognizerInfo *, rCount);

    if (fCSRecognizers == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } 
    else {
        fCSRecognizers_size = rCount;
        for (int32_t r = 0; r < rCount; r += 1) {
            fCSRecognizers[r] = tempArray[r];
            if (fCSRecognizers[r] == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
            }
        }
    }
}

U_CDECL_END

U_NAMESPACE_BEGIN

void CharsetDetector::setRecognizers(UErrorCode &status)
{
    umtx_initOnce(gCSRecognizersInitOnce, &initRecognizers, status);
}

CharsetDetector::CharsetDetector(UErrorCode &status)
  : textIn(new InputText(status)), resultArray(NULL),
    resultCount(0), fStripTags(FALSE), fFreshTextSet(FALSE),
    fEnabledRecognizers(NULL)
{
    if (U_FAILURE(status)) {
        return;
    }

    setRecognizers(status);

    if (U_FAILURE(status)) {
        return;
    }

    resultArray = (CharsetMatch **)uprv_malloc(sizeof(CharsetMatch *)*fCSRecognizers_size);

    if (resultArray == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }

    for(int32_t i = 0; i < fCSRecognizers_size; i += 1) {
        resultArray[i] = new CharsetMatch();

        if (resultArray[i] == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            break;
        }
    }
}

CharsetDetector::~CharsetDetector()
{
    delete textIn;

    for(int32_t i = 0; i < fCSRecognizers_size; i += 1) {
        delete resultArray[i];
    }

    uprv_free(resultArray);

    if (fEnabledRecognizers) {
        uprv_free(fEnabledRecognizers);
    }
}

void CharsetDetector::setText(const char *in, int32_t len)
{
    textIn->setText(in, len);
    fFreshTextSet = TRUE;
}

UBool CharsetDetector::setStripTagsFlag(UBool flag)
{
    UBool temp = fStripTags;
    fStripTags = flag;
    fFreshTextSet = TRUE;
    return temp;
}

UBool CharsetDetector::getStripTagsFlag() const
{
    return fStripTags;
}

void CharsetDetector::setDeclaredEncoding(const char *encoding, int32_t len) const
{
    textIn->setDeclaredEncoding(encoding,len);
}

int32_t CharsetDetector::getDetectableCount()
{
    UErrorCode status = U_ZERO_ERROR;

    setRecognizers(status);

    return fCSRecognizers_size; 
}

const CharsetMatch *CharsetDetector::detect(UErrorCode &status)
{
    int32_t maxMatchesFound = 0;

    detectAll(maxMatchesFound, status);

    if(maxMatchesFound > 0) {
        return resultArray[0];
    } else {
        return NULL;
    }
}

const CharsetMatch * const *CharsetDetector::detectAll(int32_t &maxMatchesFound, UErrorCode &status)
{
    if(!textIn->isSet()) {
        status = U_MISSING_RESOURCE_ERROR;// TODO:  Need to set proper status code for input text not set

        return NULL;
    } else if (fFreshTextSet) {
        CharsetRecognizer *csr;
        int32_t            i;

        textIn->MungeInput(fStripTags);

        // Iterate over all possible charsets, remember all that
        // give a match quality > 0.
        resultCount = 0;
        for (i = 0; i < fCSRecognizers_size; i += 1) {
            csr = fCSRecognizers[i]->recognizer;
            if (csr->match(textIn, resultArray[resultCount])) {
                resultCount++;
            }
        }

        if (resultCount > 1) {
            uprv_sortArray(resultArray, resultCount, sizeof resultArray[0], charsetMatchComparator, NULL, TRUE, &status);
        }
        fFreshTextSet = FALSE;
    }

    maxMatchesFound = resultCount;

    return resultArray;
}

void CharsetDetector::setDetectableCharset(const char *encoding, UBool enabled, UErrorCode &status)
{
    if (U_FAILURE(status)) {
        return;
    }

    int32_t modIdx = -1;
    UBool isDefaultVal = FALSE;
    for (int32_t i = 0; i < fCSRecognizers_size; i++) {
        CSRecognizerInfo *csrinfo = fCSRecognizers[i];
        if (uprv_strcmp(csrinfo->recognizer->getName(), encoding) == 0) {
            modIdx = i;
            isDefaultVal = (csrinfo->isDefaultEnabled == enabled);
            break;
        }
    }
    if (modIdx < 0) {
        // No matching encoding found
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (fEnabledRecognizers == NULL && !isDefaultVal) {
        // Create an array storing the non default setting
        fEnabledRecognizers = NEW_ARRAY(UBool, fCSRecognizers_size);
        if (fEnabledRecognizers == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        // Initialize the array with default info
        for (int32_t i = 0; i < fCSRecognizers_size; i++) {
            fEnabledRecognizers[i] = fCSRecognizers[i]->isDefaultEnabled;
        }
    }

    if (fEnabledRecognizers != NULL) {
        fEnabledRecognizers[modIdx] = enabled;
    }
}

/*const char *CharsetDetector::getCharsetName(int32_t index, UErrorCode &status) const
{
    if( index > fCSRecognizers_size-1 || index < 0) {
        status = U_INDEX_OUTOFBOUNDS_ERROR;

        return 0;
    } else {
        return fCSRecognizers[index]->getName();
    }
}*/

U_NAMESPACE_END

U_CDECL_BEGIN
typedef struct {
    int32_t currIndex;
    UBool all;
    UBool *enabledRecognizers;
} Context;



static void U_CALLCONV
enumClose(UEnumeration *en) {
    if(en->context != NULL) {
        DELETE_ARRAY(en->context);
    }

    DELETE_ARRAY(en);
}

static int32_t U_CALLCONV
enumCount(UEnumeration *en, UErrorCode *) {
    if (((Context *)en->context)->all) {
        // ucsdet_getAllDetectableCharsets, all charset detector names
        return fCSRecognizers_size;
    }

    // Otherwise, ucsdet_getDetectableCharsets - only enabled ones
    int32_t count = 0;
    UBool *enabledArray = ((Context *)en->context)->enabledRecognizers;
    if (enabledArray != NULL) {
        // custom set
        for (int32_t i = 0; i < fCSRecognizers_size; i++) {
            if (enabledArray[i]) {
                count++;
            }
        }
    } else {
        // default set
        for (int32_t i = 0; i < fCSRecognizers_size; i++) {
            if (fCSRecognizers[i]->isDefaultEnabled) {
                count++;
            }
        }
    }
    return count;
}

static const char* U_CALLCONV
enumNext(UEnumeration *en, int32_t *resultLength, UErrorCode * /*status*/) {
    const char *currName = NULL;

    if (((Context *)en->context)->currIndex < fCSRecognizers_size) {
        if (((Context *)en->context)->all) {
            // ucsdet_getAllDetectableCharsets, all charset detector names
            currName = fCSRecognizers[((Context *)en->context)->currIndex]->recognizer->getName();
            ((Context *)en->context)->currIndex++;
        } else {
            // ucsdet_getDetectableCharsets
            UBool *enabledArray = ((Context *)en->context)->enabledRecognizers;
            if (enabledArray != NULL) {
                // custom set
                while (currName == NULL && ((Context *)en->context)->currIndex < fCSRecognizers_size) {
                    if (enabledArray[((Context *)en->context)->currIndex]) {
                        currName = fCSRecognizers[((Context *)en->context)->currIndex]->recognizer->getName();
                    }
                    ((Context *)en->context)->currIndex++;
                }
            } else {
                // default set
                while (currName == NULL && ((Context *)en->context)->currIndex < fCSRecognizers_size) {
                    if (fCSRecognizers[((Context *)en->context)->currIndex]->isDefaultEnabled) {
                        currName = fCSRecognizers[((Context *)en->context)->currIndex]->recognizer->getName();
                    }
                    ((Context *)en->context)->currIndex++;
                }
            }
        }
    }

    if(resultLength != NULL) {
        *resultLength = currName == NULL ? 0 : (int32_t)uprv_strlen(currName);
    }

    return currName;
}


static void U_CALLCONV
enumReset(UEnumeration *en, UErrorCode *) {
    ((Context *)en->context)->currIndex = 0;
}

static const UEnumeration gCSDetEnumeration = {
    NULL,
    NULL,
    enumClose,
    enumCount,
    uenum_unextDefault,
    enumNext,
    enumReset
};

U_CDECL_END

U_NAMESPACE_BEGIN

UEnumeration * CharsetDetector::getAllDetectableCharsets(UErrorCode &status)
{

    /* Initialize recognized charsets. */
    setRecognizers(status);

    if(U_FAILURE(status)) {
        return 0;
    }

    UEnumeration *en = NEW_ARRAY(UEnumeration, 1);
    if (en == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }
    memcpy(en, &gCSDetEnumeration, sizeof(UEnumeration));
    en->context = (void*)NEW_ARRAY(Context, 1);
    if (en->context == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        DELETE_ARRAY(en);
        return 0;
    }
    uprv_memset(en->context, 0, sizeof(Context));
    ((Context*)en->context)->all = TRUE;
    return en;
}

UEnumeration * CharsetDetector::getDetectableCharsets(UErrorCode &status) const
{
    if(U_FAILURE(status)) {
        return 0;
    }

    UEnumeration *en = NEW_ARRAY(UEnumeration, 1);
    if (en == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }
    memcpy(en, &gCSDetEnumeration, sizeof(UEnumeration));
    en->context = (void*)NEW_ARRAY(Context, 1);
    if (en->context == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        DELETE_ARRAY(en);
        return 0;
    }
    uprv_memset(en->context, 0, sizeof(Context));
    ((Context*)en->context)->all = FALSE;
    ((Context*)en->context)->enabledRecognizers = fEnabledRecognizers;
    return en;
}

U_NAMESPACE_END

#endif
