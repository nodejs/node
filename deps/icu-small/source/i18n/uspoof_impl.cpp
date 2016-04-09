/*
**********************************************************************
*   Copyright (C) 2008-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/uspoof.h"
#include "unicode/uchar.h"
#include "unicode/uniset.h"
#include "unicode/utf16.h"
#include "utrie2.h"
#include "cmemory.h"
#include "cstring.h"
#include "identifier_info.h"
#include "scriptset.h"
#include "umutex.h"
#include "udataswp.h"
#include "uassert.h"
#include "uspoof_impl.h"

#if !UCONFIG_NO_NORMALIZATION


U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(SpoofImpl)

SpoofImpl::SpoofImpl(SpoofData *data, UErrorCode &status) :
        fMagic(0), fChecks(USPOOF_ALL_CHECKS), fSpoofData(data), fAllowedCharsSet(NULL) ,
        fAllowedLocales(NULL), fCachedIdentifierInfo(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    fRestrictionLevel = USPOOF_HIGHLY_RESTRICTIVE;

    UnicodeSet *allowedCharsSet = new UnicodeSet(0, 0x10ffff);
    allowedCharsSet->freeze();
    fAllowedCharsSet = allowedCharsSet;
    fAllowedLocales  = uprv_strdup("");
    if (fAllowedCharsSet == NULL || fAllowedLocales == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    fMagic = USPOOF_MAGIC;
}


SpoofImpl::SpoofImpl() :
        fMagic(USPOOF_MAGIC), fChecks(USPOOF_ALL_CHECKS), fSpoofData(NULL), fAllowedCharsSet(NULL) ,
        fAllowedLocales(NULL), fCachedIdentifierInfo(NULL) {
    UnicodeSet *allowedCharsSet = new UnicodeSet(0, 0x10ffff);
    allowedCharsSet->freeze();
    fAllowedCharsSet = allowedCharsSet;
    fAllowedLocales  = uprv_strdup("");
    fRestrictionLevel = USPOOF_HIGHLY_RESTRICTIVE;
}


// Copy Constructor, used by the user level clone() function.
SpoofImpl::SpoofImpl(const SpoofImpl &src, UErrorCode &status)  :
        fMagic(0), fChecks(USPOOF_ALL_CHECKS), fSpoofData(NULL), fAllowedCharsSet(NULL) ,
        fAllowedLocales(NULL), fCachedIdentifierInfo(NULL) {
    if (U_FAILURE(status)) {
        return;
    }
    fMagic = src.fMagic;
    fChecks = src.fChecks;
    if (src.fSpoofData != NULL) {
        fSpoofData = src.fSpoofData->addReference();
    }
    fAllowedCharsSet = static_cast<const UnicodeSet *>(src.fAllowedCharsSet->clone());
    if (fAllowedCharsSet == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    fAllowedLocales = uprv_strdup(src.fAllowedLocales);
    fRestrictionLevel = src.fRestrictionLevel;
}

SpoofImpl::~SpoofImpl() {
    fMagic = 0;                // head off application errors by preventing use of
                               //    of deleted objects.
    if (fSpoofData != NULL) {
        fSpoofData->removeReference();   // Will delete if refCount goes to zero.
    }
    delete fAllowedCharsSet;
    uprv_free((void *)fAllowedLocales);
    delete fCachedIdentifierInfo;
}

//
//  Incoming parameter check on Status and the SpoofChecker object
//    received from the C API.
//
const SpoofImpl *SpoofImpl::validateThis(const USpoofChecker *sc, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (sc == NULL) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    SpoofImpl *This = (SpoofImpl *)sc;
    if (This->fMagic != USPOOF_MAGIC ||
        This->fSpoofData == NULL) {
        status = U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    if (!SpoofData::validateDataVersion(This->fSpoofData->fRawData, status)) {
        return NULL;
    }
    return This;
}

SpoofImpl *SpoofImpl::validateThis(USpoofChecker *sc, UErrorCode &status) {
    return const_cast<SpoofImpl *>
        (SpoofImpl::validateThis(const_cast<const USpoofChecker *>(sc), status));
}



//--------------------------------------------------------------------------------------
//
//  confusableLookup()    This is the heart of the confusable skeleton generation
//                        implementation.
//
//                        Given a source character, produce the corresponding
//                        replacement character(s), appending them to the dest string.
//
//---------------------------------------------------------------------------------------
int32_t SpoofImpl::confusableLookup(UChar32 inChar, int32_t tableMask, UnicodeString &dest) const {

    // Binary search the spoof data key table for the inChar
    int32_t  *low   = fSpoofData->fCFUKeys;
    int32_t  *mid   = NULL;
    int32_t  *limit = low + fSpoofData->fRawData->fCFUKeysSize;
    UChar32   midc;
    do {
        int32_t delta = ((int32_t)(limit-low))/2;
        mid = low + delta;
        midc = *mid & 0x1fffff;
        if (inChar == midc) {
            goto foundChar;
        } else if (inChar < midc) {
            limit = mid;
        } else {
            low = mid;
        }
    } while (low < limit-1);
    mid = low;
    midc = *mid & 0x1fffff;
    if (inChar != midc) {
        // Char not found.  It maps to itself.
        int i = 0;
        dest.append(inChar);
        return i;
    }
  foundChar:
    int32_t keyFlags = *mid & 0xff000000;
    if ((keyFlags & tableMask) == 0) {
        // We found the right key char, but the entry doesn't pertain to the
        //  table we need.  See if there is an adjacent key that does
        if (keyFlags & USPOOF_KEY_MULTIPLE_VALUES) {
            int32_t *altMid;
            for (altMid = mid-1; (*altMid&0x00ffffff) == inChar; altMid--) {
                keyFlags = *altMid & 0xff000000;
                if (keyFlags & tableMask) {
                    mid = altMid;
                    goto foundKey;
                }
            }
            for (altMid = mid+1; (*altMid&0x00ffffff) == inChar; altMid++) {
                keyFlags = *altMid & 0xff000000;
                if (keyFlags & tableMask) {
                    mid = altMid;
                    goto foundKey;
                }
            }
        }
        // No key entry for this char & table.
        // The input char maps to itself.
        int i = 0;
        dest.append(inChar);
        return i;
    }

  foundKey:
    int32_t  stringLen = USPOOF_KEY_LENGTH_FIELD(keyFlags) + 1;
    int32_t keyTableIndex = (int32_t)(mid - fSpoofData->fCFUKeys);

    // Value is either a UChar  (for strings of length 1) or
    //                 an index into the string table (for longer strings)
    uint16_t value = fSpoofData->fCFUValues[keyTableIndex];
    if (stringLen == 1) {
        dest.append((UChar)value);
        return 1;
    }

    // String length of 4 from the above lookup is used for all strings of length >= 4.
    // For these, get the real length from the string lengths table,
    //   which maps string table indexes to lengths.
    //   All strings of the same length are stored contiguously in the string table.
    //   'value' from the lookup above is the starting index for the desired string.

    int32_t ix;
    if (stringLen == 4) {
        int32_t stringLengthsLimit = fSpoofData->fRawData->fCFUStringLengthsSize;
        for (ix = 0; ix < stringLengthsLimit; ix++) {
            if (fSpoofData->fCFUStringLengths[ix].fLastString >= value) {
                stringLen = fSpoofData->fCFUStringLengths[ix].fStrLength;
                break;
            }
        }
        U_ASSERT(ix < stringLengthsLimit);
    }

    U_ASSERT(value + stringLen <= fSpoofData->fRawData->fCFUStringTableLen);
    UChar *src = &fSpoofData->fCFUStrings[value];
    dest.append(src, stringLen);
    return stringLen;
}


//---------------------------------------------------------------------------------------
//
//  wholeScriptCheck()
//
//      Input text is already normalized to NFD
//      Return the set of scripts, each of which can represent something that is
//             confusable with the input text.  The script of the input text
//             is included; input consisting of characters from a single script will
//             always produce a result consisting of a set containing that script.
//
//---------------------------------------------------------------------------------------
void SpoofImpl::wholeScriptCheck(
        const UnicodeString &text, ScriptSet *result, UErrorCode &status) const {

    UTrie2 *table =
        (fChecks & USPOOF_ANY_CASE) ? fSpoofData->fAnyCaseTrie : fSpoofData->fLowerCaseTrie;
    result->setAll();
    int32_t length = text.length();
    for (int32_t inputIdx=0; inputIdx < length;) {
        UChar32 c = text.char32At(inputIdx);
        inputIdx += U16_LENGTH(c);
        uint32_t index = utrie2_get32(table, c);
        if (index == 0) {
            // No confusables in another script for this char.
            // TODO:  we should change the data to have sets with just the single script
            //        bit for the script of this char.  Gets rid of this special case.
            //        Until then, grab the script from the char and intersect it with the set.
            UScriptCode cpScript = uscript_getScript(c, &status);
            U_ASSERT(cpScript > USCRIPT_INHERITED);
            result->intersect(cpScript, status);
        } else if (index == 1) {
            // Script == Common or Inherited.  Nothing to do.
        } else {
            result->intersect(fSpoofData->fScriptSets[index]);
        }
    }
}


void SpoofImpl::setAllowedLocales(const char *localesList, UErrorCode &status) {
    UnicodeSet    allowedChars;
    UnicodeSet    *tmpSet = NULL;
    const char    *locStart = localesList;
    const char    *locEnd = NULL;
    const char    *localesListEnd = localesList + uprv_strlen(localesList);
    int32_t        localeListCount = 0;   // Number of locales provided by caller.

    // Loop runs once per locale from the localesList, a comma separated list of locales.
    do {
        locEnd = uprv_strchr(locStart, ',');
        if (locEnd == NULL) {
            locEnd = localesListEnd;
        }
        while (*locStart == ' ') {
            locStart++;
        }
        const char *trimmedEnd = locEnd-1;
        while (trimmedEnd > locStart && *trimmedEnd == ' ') {
            trimmedEnd--;
        }
        if (trimmedEnd <= locStart) {
            break;
        }
        const char *locale = uprv_strndup(locStart, (int32_t)(trimmedEnd + 1 - locStart));
        localeListCount++;

        // We have one locale from the locales list.
        // Add the script chars for this locale to the accumulating set of allowed chars.
        // If the locale is no good, we will be notified back via status.
        addScriptChars(locale, &allowedChars, status);
        uprv_free((void *)locale);
        if (U_FAILURE(status)) {
            break;
        }
        locStart = locEnd + 1;
    } while (locStart < localesListEnd);

    // If our caller provided an empty list of locales, we disable the allowed characters checking
    if (localeListCount == 0) {
        uprv_free((void *)fAllowedLocales);
        fAllowedLocales = uprv_strdup("");
        tmpSet = new UnicodeSet(0, 0x10ffff);
        if (fAllowedLocales == NULL || tmpSet == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        tmpSet->freeze();
        delete fAllowedCharsSet;
        fAllowedCharsSet = tmpSet;
        fChecks &= ~USPOOF_CHAR_LIMIT;
        return;
    }


    // Add all common and inherited characters to the set of allowed chars.
    UnicodeSet tempSet;
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_COMMON, status);
    allowedChars.addAll(tempSet);
    tempSet.applyIntPropertyValue(UCHAR_SCRIPT, USCRIPT_INHERITED, status);
    allowedChars.addAll(tempSet);

    // If anything went wrong, we bail out without changing
    // the state of the spoof checker.
    if (U_FAILURE(status)) {
        return;
    }

    // Store the updated spoof checker state.
    tmpSet = static_cast<UnicodeSet *>(allowedChars.clone());
    const char *tmpLocalesList = uprv_strdup(localesList);
    if (tmpSet == NULL || tmpLocalesList == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_free((void *)fAllowedLocales);
    fAllowedLocales = tmpLocalesList;
    tmpSet->freeze();
    delete fAllowedCharsSet;
    fAllowedCharsSet = tmpSet;
    fChecks |= USPOOF_CHAR_LIMIT;
}


const char * SpoofImpl::getAllowedLocales(UErrorCode &/*status*/) {
    return fAllowedLocales;
}


// Given a locale (a language), add all the characters from all of the scripts used with that language
// to the allowedChars UnicodeSet

void SpoofImpl::addScriptChars(const char *locale, UnicodeSet *allowedChars, UErrorCode &status) {
    UScriptCode scripts[30];

    int32_t numScripts = uscript_getCode(locale, scripts, UPRV_LENGTHOF(scripts), &status);
    if (U_FAILURE(status)) {
        return;
    }
    if (status == U_USING_DEFAULT_WARNING) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }
    UnicodeSet tmpSet;
    int32_t    i;
    for (i=0; i<numScripts; i++) {
        tmpSet.applyIntPropertyValue(UCHAR_SCRIPT, scripts[i], status);
        allowedChars->addAll(tmpSet);
    }
}


// Convert a text format hex number.  Utility function used by builder code.  Static.
// Input: UChar *string text.  Output: a UChar32
// Input has been pre-checked, and will have no non-hex chars.
// The number must fall in the code point range of 0..0x10ffff
// Static Function.
UChar32 SpoofImpl::ScanHex(const UChar *s, int32_t start, int32_t limit, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    U_ASSERT(limit-start > 0);
    uint32_t val = 0;
    int i;
    for (i=start; i<limit; i++) {
        int digitVal = s[i] - 0x30;
        if (digitVal>9) {
            digitVal = 0xa + (s[i] - 0x41);  // Upper Case 'A'
        }
        if (digitVal>15) {
            digitVal = 0xa + (s[i] - 0x61);  // Lower Case 'a'
        }
        U_ASSERT(digitVal <= 0xf);
        val <<= 4;
        val += digitVal;
    }
    if (val > 0x10ffff) {
        status = U_PARSE_ERROR;
        val = 0;
    }
    return (UChar32)val;
}

// IdentifierInfo Cache. IdentifierInfo objects are somewhat expensive to create.
//                       Maintain a one-element cache, which is sufficient to avoid repeatedly
//                       creating new ones unless we get multi-thread concurrency in spoof
//                       check operations, which should be statistically uncommon.

// These functions are used in place of new & delete of an IdentifierInfo.
// They will recycle the IdentifierInfo when possible.
// They are logically const, and used within const functions that must be thread safe.
IdentifierInfo *SpoofImpl::getIdentifierInfo(UErrorCode &status) const {
    IdentifierInfo *returnIdInfo = NULL;
    if (U_FAILURE(status)) {
        return returnIdInfo;
    }
    SpoofImpl *nonConstThis = const_cast<SpoofImpl *>(this);
    {
        Mutex m;
        returnIdInfo = nonConstThis->fCachedIdentifierInfo;
        nonConstThis->fCachedIdentifierInfo = NULL;
    }
    if (returnIdInfo == NULL) {
        returnIdInfo = new IdentifierInfo(status);
        if (U_SUCCESS(status) && returnIdInfo == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        if (U_FAILURE(status) && returnIdInfo != NULL) {
            delete returnIdInfo;
            returnIdInfo = NULL;
        }
    }
    return returnIdInfo;
}


void SpoofImpl::releaseIdentifierInfo(IdentifierInfo *idInfo) const {
    if (idInfo != NULL) {
        SpoofImpl *nonConstThis = const_cast<SpoofImpl *>(this);
        {
            Mutex m;
            if (nonConstThis->fCachedIdentifierInfo == NULL) {
                nonConstThis->fCachedIdentifierInfo = idInfo;
                idInfo = NULL;
            }
        }
        delete idInfo;
    }
}




//----------------------------------------------------------------------------------------------
//
//   class SpoofData Implementation
//
//----------------------------------------------------------------------------------------------


UBool SpoofData::validateDataVersion(const SpoofDataHeader *rawData, UErrorCode &status) {
    if (U_FAILURE(status) ||
        rawData == NULL ||
        rawData->fMagic != USPOOF_MAGIC ||
        rawData->fFormatVersion[0] > 1 ||
        rawData->fFormatVersion[1] > 0) {
            status = U_INVALID_FORMAT_ERROR;
            return FALSE;
    }
    return TRUE;
}

static UBool U_CALLCONV
spoofDataIsAcceptable(void *context,
                        const char * /* type */, const char * /*name*/,
                        const UDataInfo *pInfo) {
    if(
        pInfo->size >= 20 &&
        pInfo->isBigEndian == U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily == U_CHARSET_FAMILY &&
        pInfo->dataFormat[0] == 0x43 &&  // dataFormat="Cfu "
        pInfo->dataFormat[1] == 0x66 &&
        pInfo->dataFormat[2] == 0x75 &&
        pInfo->dataFormat[3] == 0x20 &&
        pInfo->formatVersion[0] == 1
    ) {
        UVersionInfo *version = static_cast<UVersionInfo *>(context);
        if(version != NULL) {
            uprv_memcpy(version, pInfo->dataVersion, 4);
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

//
//  SpoofData::getDefault() - return a wrapper around the spoof data that is
//                            baked into the default ICU data.
//
//               Called once, from the initOnce() function in uspoof_impl.cpp; the resulting
//               SpoofData is shared by all spoof checkers using the default data.
//
SpoofData *SpoofData::getDefault(UErrorCode &status) {
    UDataMemory *udm = udata_openChoice(NULL, "cfu", "confusables",
                                        spoofDataIsAcceptable,
                                        NULL,       // context, would receive dataVersion if supplied.
                                        &status);
    if (U_FAILURE(status)) {
        return NULL;
    }
    SpoofData *This = new SpoofData(udm, status);
    if (U_FAILURE(status)) {
        delete This;
        return NULL;
    }
    if (This == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return This;
}

SpoofData::SpoofData(UDataMemory *udm, UErrorCode &status)
{
    reset();
    if (U_FAILURE(status)) {
        return;
    }
    fUDM = udm;
    // fRawData is non-const because it may be constructed by the data builder.
    fRawData = reinterpret_cast<SpoofDataHeader *>(
            const_cast<void *>(udata_getMemory(udm)));
    validateDataVersion(fRawData, status);
    initPtrs(status);
}


SpoofData::SpoofData(const void *data, int32_t length, UErrorCode &status)
{
    reset();
    if (U_FAILURE(status)) {
        return;
    }
    if ((size_t)length < sizeof(SpoofDataHeader)) {
        status = U_INVALID_FORMAT_ERROR;
        return;
    }
    void *ncData = const_cast<void *>(data);
    fRawData = static_cast<SpoofDataHeader *>(ncData);
    if (length < fRawData->fLength) {
        status = U_INVALID_FORMAT_ERROR;
        return;
    }
    validateDataVersion(fRawData, status);
    initPtrs(status);
}


// Spoof Data constructor for use from data builder.
//   Initializes a new, empty data area that will be populated later.
SpoofData::SpoofData(UErrorCode &status) {
    reset();
    if (U_FAILURE(status)) {
        return;
    }
    fDataOwned = true;

    // The spoof header should already be sized to be a multiple of 16 bytes.
    // Just in case it's not, round it up.
    uint32_t initialSize = (sizeof(SpoofDataHeader) + 15) & ~15;
    U_ASSERT(initialSize == sizeof(SpoofDataHeader));

    fRawData = static_cast<SpoofDataHeader *>(uprv_malloc(initialSize));
    fMemLimit = initialSize;
    if (fRawData == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_memset(fRawData, 0, initialSize);

    fRawData->fMagic = USPOOF_MAGIC;
    fRawData->fFormatVersion[0] = 1;
    fRawData->fFormatVersion[1] = 0;
    fRawData->fFormatVersion[2] = 0;
    fRawData->fFormatVersion[3] = 0;
    initPtrs(status);
}

// reset() - initialize all fields.
//           Should be updated if any new fields are added.
//           Called by constructors to put things in a known initial state.
void SpoofData::reset() {
   fRawData = NULL;
   fDataOwned = FALSE;
   fUDM      = NULL;
   fMemLimit = 0;
   fRefCount = 1;
   fCFUKeys = NULL;
   fCFUValues = NULL;
   fCFUStringLengths = NULL;
   fCFUStrings = NULL;
   fAnyCaseTrie = NULL;
   fLowerCaseTrie = NULL;
   fScriptSets = NULL;
}


//  SpoofData::initPtrs()
//            Initialize the pointers to the various sections of the raw data.
//
//            This function is used both during the Trie building process (multiple
//            times, as the individual data sections are added), and
//            during the opening of a Spoof Checker from prebuilt data.
//
//            The pointers for non-existent data sections (identified by an offset of 0)
//            are set to NULL.
//
//            Note:  During building the data, adding each new data section
//            reallocs the raw data area, which likely relocates it, which
//            in turn requires reinitializing all of the pointers into it, hence
//            multiple calls to this function during building.
//
void SpoofData::initPtrs(UErrorCode &status) {
    fCFUKeys = NULL;
    fCFUValues = NULL;
    fCFUStringLengths = NULL;
    fCFUStrings = NULL;
    if (U_FAILURE(status)) {
        return;
    }
    if (fRawData->fCFUKeys != 0) {
        fCFUKeys = (int32_t *)((char *)fRawData + fRawData->fCFUKeys);
    }
    if (fRawData->fCFUStringIndex != 0) {
        fCFUValues = (uint16_t *)((char *)fRawData + fRawData->fCFUStringIndex);
    }
    if (fRawData->fCFUStringLengths != 0) {
        fCFUStringLengths = (SpoofStringLengthsElement *)((char *)fRawData + fRawData->fCFUStringLengths);
    }
    if (fRawData->fCFUStringTable != 0) {
        fCFUStrings = (UChar *)((char *)fRawData + fRawData->fCFUStringTable);
    }

    if (fAnyCaseTrie ==  NULL && fRawData->fAnyCaseTrie != 0) {
        fAnyCaseTrie = utrie2_openFromSerialized(UTRIE2_16_VALUE_BITS,
            (char *)fRawData + fRawData->fAnyCaseTrie, fRawData->fAnyCaseTrieLength, NULL, &status);
    }
    if (fLowerCaseTrie ==  NULL && fRawData->fLowerCaseTrie != 0) {
        fLowerCaseTrie = utrie2_openFromSerialized(UTRIE2_16_VALUE_BITS,
            (char *)fRawData + fRawData->fLowerCaseTrie, fRawData->fLowerCaseTrieLength, NULL, &status);
    }

    if (fRawData->fScriptSets != 0) {
        fScriptSets = (ScriptSet *)((char *)fRawData + fRawData->fScriptSets);
    }
}


SpoofData::~SpoofData() {
    utrie2_close(fAnyCaseTrie);
    fAnyCaseTrie = NULL;
    utrie2_close(fLowerCaseTrie);
    fLowerCaseTrie = NULL;
    if (fDataOwned) {
        uprv_free(fRawData);
    }
    fRawData = NULL;
    if (fUDM != NULL) {
        udata_close(fUDM);
    }
    fUDM = NULL;
}


void SpoofData::removeReference() {
    if (umtx_atomic_dec(&fRefCount) == 0) {
        delete this;
    }
}


SpoofData *SpoofData::addReference() {
    umtx_atomic_inc(&fRefCount);
    return this;
}


void *SpoofData::reserveSpace(int32_t numBytes,  UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    if (!fDataOwned) {
        U_ASSERT(FALSE);
        status = U_INTERNAL_PROGRAM_ERROR;
        return NULL;
    }

    numBytes = (numBytes + 15) & ~15;   // Round up to a multiple of 16
    uint32_t returnOffset = fMemLimit;
    fMemLimit += numBytes;
    fRawData = static_cast<SpoofDataHeader *>(uprv_realloc(fRawData, fMemLimit));
    fRawData->fLength = fMemLimit;
    uprv_memset((char *)fRawData + returnOffset, 0, numBytes);
    initPtrs(status);
    return (char *)fRawData + returnOffset;
}


U_NAMESPACE_END

U_NAMESPACE_USE

//-----------------------------------------------------------------------------
//
//  uspoof_swap   -  byte swap and char encoding swap of spoof data
//
//-----------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uspoof_swap(const UDataSwapper *ds, const void *inData, int32_t length, void *outData,
           UErrorCode *status) {

    if (status == NULL || U_FAILURE(*status)) {
        return 0;
    }
    if(ds==NULL || inData==NULL || length<-1 || (length>0 && outData==NULL)) {
        *status=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    //
    //  Check that the data header is for spoof data.
    //    (Header contents are defined in gencfu.cpp)
    //
    const UDataInfo *pInfo = (const UDataInfo *)((const char *)inData+4);
    if(!(  pInfo->dataFormat[0]==0x43 &&   /* dataFormat="Cfu " */
           pInfo->dataFormat[1]==0x66 &&
           pInfo->dataFormat[2]==0x75 &&
           pInfo->dataFormat[3]==0x20 &&
           pInfo->formatVersion[0]==1  )) {
        udata_printError(ds, "uspoof_swap(): data format %02x.%02x.%02x.%02x "
                             "(format version %02x %02x %02x %02x) is not recognized\n",
                         pInfo->dataFormat[0], pInfo->dataFormat[1],
                         pInfo->dataFormat[2], pInfo->dataFormat[3],
                         pInfo->formatVersion[0], pInfo->formatVersion[1],
                         pInfo->formatVersion[2], pInfo->formatVersion[3]);
        *status=U_UNSUPPORTED_ERROR;
        return 0;
    }

    //
    // Swap the data header.  (This is the generic ICU Data Header, not the uspoof Specific
    //                         header).  This swap also conveniently gets us
    //                         the size of the ICU d.h., which lets us locate the start
    //                         of the uspoof specific data.
    //
    int32_t headerSize=udata_swapDataHeader(ds, inData, length, outData, status);


    //
    // Get the Spoof Data Header, and check that it appears to be OK.
    //
    //
    const uint8_t   *inBytes =(const uint8_t *)inData+headerSize;
    SpoofDataHeader *spoofDH = (SpoofDataHeader *)inBytes;
    if (ds->readUInt32(spoofDH->fMagic)   != USPOOF_MAGIC ||
        ds->readUInt32(spoofDH->fLength)  <  sizeof(SpoofDataHeader))
    {
        udata_printError(ds, "uspoof_swap(): Spoof Data header is invalid.\n");
        *status=U_UNSUPPORTED_ERROR;
        return 0;
    }

    //
    // Prefight operation?  Just return the size
    //
    int32_t spoofDataLength = ds->readUInt32(spoofDH->fLength);
    int32_t totalSize = headerSize + spoofDataLength;
    if (length < 0) {
        return totalSize;
    }

    //
    // Check that length passed in is consistent with length from Spoof data header.
    //
    if (length < totalSize) {
        udata_printError(ds, "uspoof_swap(): too few bytes (%d after ICU Data header) for spoof data.\n",
                            spoofDataLength);
        *status=U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
        }


    //
    // Swap the Data.  Do the data itself first, then the Spoof Data Header, because
    //                 we need to reference the header to locate the data, and an
    //                 inplace swap of the header leaves it unusable.
    //
    uint8_t          *outBytes = (uint8_t *)outData + headerSize;
    SpoofDataHeader  *outputDH = (SpoofDataHeader *)outBytes;

    int32_t   sectionStart;
    int32_t   sectionLength;

    //
    // If not swapping in place, zero out the output buffer before starting.
    //    Gaps may exist between the individual sections, and these must be zeroed in
    //    the output buffer.  The simplest way to do that is to just zero the whole thing.
    //
    if (inBytes != outBytes) {
        uprv_memset(outBytes, 0, spoofDataLength);
    }

    // Confusables Keys Section   (fCFUKeys)
    sectionStart  = ds->readUInt32(spoofDH->fCFUKeys);
    sectionLength = ds->readUInt32(spoofDH->fCFUKeysSize) * 4;
    ds->swapArray32(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Index Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringIndex);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringIndexSize) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Table Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringTable);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringTableLen) * 2;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // String Lengths Section
    sectionStart  = ds->readUInt32(spoofDH->fCFUStringLengths);
    sectionLength = ds->readUInt32(spoofDH->fCFUStringLengthsSize) * 4;
    ds->swapArray16(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // Any Case Trie
    sectionStart  = ds->readUInt32(spoofDH->fAnyCaseTrie);
    sectionLength = ds->readUInt32(spoofDH->fAnyCaseTrieLength);
    utrie2_swap(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // Lower Case Trie
    sectionStart  = ds->readUInt32(spoofDH->fLowerCaseTrie);
    sectionLength = ds->readUInt32(spoofDH->fLowerCaseTrieLength);
    utrie2_swap(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // Script Sets.  The data is an array of int32_t
    sectionStart  = ds->readUInt32(spoofDH->fScriptSets);
    sectionLength = ds->readUInt32(spoofDH->fScriptSetsLength) * sizeof(ScriptSet);
    ds->swapArray32(ds, inBytes+sectionStart, sectionLength, outBytes+sectionStart, status);

    // And, last, swap the header itself.
    //   int32_t   fMagic             // swap this
    //   uint8_t   fFormatVersion[4]  // Do not swap this, just copy
    //   int32_t   fLength and all the rest       // Swap the rest, all is 32 bit stuff.
    //
    uint32_t magic = ds->readUInt32(spoofDH->fMagic);
    ds->writeUInt32((uint32_t *)&outputDH->fMagic, magic);

    if (outputDH->fFormatVersion != spoofDH->fFormatVersion) {
        uprv_memcpy(outputDH->fFormatVersion, spoofDH->fFormatVersion, sizeof(spoofDH->fFormatVersion));
    }
    // swap starting at fLength
    ds->swapArray32(ds, &spoofDH->fLength, sizeof(SpoofDataHeader)-8 /* minus magic and fFormatVersion[4] */, &outputDH->fLength, status);

    return totalSize;
}

#endif
