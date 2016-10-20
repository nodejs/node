// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2009-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */

#include "unicode/currpinf.h"

#if !UCONFIG_NO_FORMATTING

//#define CURRENCY_PLURAL_INFO_DEBUG 1

#ifdef CURRENCY_PLURAL_INFO_DEBUG
#include <iostream>
#endif


#include "unicode/locid.h"
#include "unicode/plurrule.h"
#include "unicode/ures.h"
#include "unicode/numsys.h"
#include "cstring.h"
#include "hash.h"
#include "uresimp.h"
#include "ureslocs.h"

U_NAMESPACE_BEGIN


static const UChar gNumberPatternSeparator = 0x3B; // ;

U_CDECL_BEGIN

/**
 * @internal ICU 4.2
 */
static UBool U_CALLCONV ValueComparator(UHashTok val1, UHashTok val2);

UBool
U_CALLCONV ValueComparator(UHashTok val1, UHashTok val2) {
    const UnicodeString* affix_1 = (UnicodeString*)val1.pointer;
    const UnicodeString* affix_2 = (UnicodeString*)val2.pointer;
    return  *affix_1 == *affix_2;
}

U_CDECL_END


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CurrencyPluralInfo)

static const UChar gDefaultCurrencyPluralPattern[] = {'0', '.', '#', '#', ' ', 0xA4, 0xA4, 0xA4, 0};
static const UChar gTripleCurrencySign[] = {0xA4, 0xA4, 0xA4, 0};
static const UChar gPluralCountOther[] = {0x6F, 0x74, 0x68, 0x65, 0x72, 0};
static const UChar gPart0[] = {0x7B, 0x30, 0x7D, 0};
static const UChar gPart1[] = {0x7B, 0x31, 0x7D, 0};

static const char gNumberElementsTag[]="NumberElements";
static const char gLatnTag[]="latn";
static const char gPatternsTag[]="patterns";
static const char gDecimalFormatTag[]="decimalFormat";
static const char gCurrUnitPtnTag[]="CurrencyUnitPatterns";

CurrencyPluralInfo::CurrencyPluralInfo(UErrorCode& status)
:   fPluralCountToCurrencyUnitPattern(NULL),
    fPluralRules(NULL),
    fLocale(NULL) {
    initialize(Locale::getDefault(), status);
}

CurrencyPluralInfo::CurrencyPluralInfo(const Locale& locale, UErrorCode& status)
:   fPluralCountToCurrencyUnitPattern(NULL),
    fPluralRules(NULL),
    fLocale(NULL) {
    initialize(locale, status);
}

CurrencyPluralInfo::CurrencyPluralInfo(const CurrencyPluralInfo& info) 
:   UObject(info),
    fPluralCountToCurrencyUnitPattern(NULL),
    fPluralRules(NULL),
    fLocale(NULL) {
    *this = info;
}


CurrencyPluralInfo&
CurrencyPluralInfo::operator=(const CurrencyPluralInfo& info) {
    if (this == &info) {
        return *this;
    }

    deleteHash(fPluralCountToCurrencyUnitPattern);
    UErrorCode status = U_ZERO_ERROR;
    fPluralCountToCurrencyUnitPattern = initHash(status);
    copyHash(info.fPluralCountToCurrencyUnitPattern, 
             fPluralCountToCurrencyUnitPattern, status);
    if ( U_FAILURE(status) ) {
        return *this;
    }

    delete fPluralRules;
    delete fLocale;
    if (info.fPluralRules) {
        fPluralRules = info.fPluralRules->clone();
    } else {
        fPluralRules = NULL;
    }
    if (info.fLocale) {
        fLocale = info.fLocale->clone();
    } else {
        fLocale = NULL;
    }
    return *this;
}


CurrencyPluralInfo::~CurrencyPluralInfo() {
    deleteHash(fPluralCountToCurrencyUnitPattern);
    fPluralCountToCurrencyUnitPattern = NULL;
    delete fPluralRules;
    delete fLocale;
    fPluralRules = NULL;
    fLocale = NULL;
}

UBool
CurrencyPluralInfo::operator==(const CurrencyPluralInfo& info) const {
#ifdef CURRENCY_PLURAL_INFO_DEBUG
    if (*fPluralRules == *info.fPluralRules) {
        std::cout << "same plural rules\n";
    }
    if (*fLocale == *info.fLocale) {
        std::cout << "same locale\n";
    }
    if (fPluralCountToCurrencyUnitPattern->equals(*info.fPluralCountToCurrencyUnitPattern)) {
        std::cout << "same pattern\n";
    }
#endif
    return *fPluralRules == *info.fPluralRules &&
           *fLocale == *info.fLocale &&
           fPluralCountToCurrencyUnitPattern->equals(*info.fPluralCountToCurrencyUnitPattern);
}


CurrencyPluralInfo*
CurrencyPluralInfo::clone() const {
    return new CurrencyPluralInfo(*this);
}

const PluralRules* 
CurrencyPluralInfo::getPluralRules() const {
    return fPluralRules;
}

UnicodeString&
CurrencyPluralInfo::getCurrencyPluralPattern(const UnicodeString&  pluralCount,
                                             UnicodeString& result) const {
    const UnicodeString* currencyPluralPattern = 
        (UnicodeString*)fPluralCountToCurrencyUnitPattern->get(pluralCount);
    if (currencyPluralPattern == NULL) {
        // fall back to "other"
        if (pluralCount.compare(gPluralCountOther, 5)) {
            currencyPluralPattern = 
                (UnicodeString*)fPluralCountToCurrencyUnitPattern->get(UnicodeString(TRUE, gPluralCountOther, 5));
        }
        if (currencyPluralPattern == NULL) {
            // no currencyUnitPatterns defined, 
            // fallback to predefined defult.
            // This should never happen when ICU resource files are
            // available, since currencyUnitPattern of "other" is always
            // defined in root.
            result = UnicodeString(gDefaultCurrencyPluralPattern);
            return result;
        }
    }
    result = *currencyPluralPattern;
    return result;
}

const Locale&
CurrencyPluralInfo::getLocale() const {
    return *fLocale;
}

void
CurrencyPluralInfo::setPluralRules(const UnicodeString& ruleDescription,
                                   UErrorCode& status) {
    if (U_SUCCESS(status)) {
        if (fPluralRules) {
            delete fPluralRules;
        }
        fPluralRules = PluralRules::createRules(ruleDescription, status);
    }
}


void
CurrencyPluralInfo::setCurrencyPluralPattern(const UnicodeString& pluralCount,
                                             const UnicodeString& pattern,
                                             UErrorCode& status) {
    if (U_SUCCESS(status)) {
        fPluralCountToCurrencyUnitPattern->put(pluralCount, new UnicodeString(pattern), status);
    }
}


void
CurrencyPluralInfo::setLocale(const Locale& loc, UErrorCode& status) {
    initialize(loc, status);
}


void 
CurrencyPluralInfo::initialize(const Locale& loc, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }
    delete fLocale;
    fLocale = loc.clone();
    if (fPluralRules) {
        delete fPluralRules;
    }
    fPluralRules = PluralRules::forLocale(loc, status);
    setupCurrencyPluralPattern(loc, status);
}

   
void
CurrencyPluralInfo::setupCurrencyPluralPattern(const Locale& loc, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return;
    }

    if (fPluralCountToCurrencyUnitPattern) {
        deleteHash(fPluralCountToCurrencyUnitPattern);
    }
    fPluralCountToCurrencyUnitPattern = initHash(status);
    if (U_FAILURE(status)) {
        return;
    }

    NumberingSystem *ns = NumberingSystem::createInstance(loc,status);
    UErrorCode ec = U_ZERO_ERROR;
    UResourceBundle *rb = ures_open(NULL, loc.getName(), &ec);
    UResourceBundle *numElements = ures_getByKeyWithFallback(rb, gNumberElementsTag, NULL, &ec);
    rb = ures_getByKeyWithFallback(numElements, ns->getName(), rb, &ec);
    rb = ures_getByKeyWithFallback(rb, gPatternsTag, rb, &ec);
    int32_t ptnLen;
    const UChar* numberStylePattern = ures_getStringByKeyWithFallback(rb, gDecimalFormatTag, &ptnLen, &ec);
    // Fall back to "latn" if num sys specific pattern isn't there.
    if ( ec == U_MISSING_RESOURCE_ERROR && uprv_strcmp(ns->getName(),gLatnTag)) {
        ec = U_ZERO_ERROR;
        rb = ures_getByKeyWithFallback(numElements, gLatnTag, rb, &ec);
        rb = ures_getByKeyWithFallback(rb, gPatternsTag, rb, &ec);
        numberStylePattern = ures_getStringByKeyWithFallback(rb, gDecimalFormatTag, &ptnLen, &ec);
    }
    int32_t numberStylePatternLen = ptnLen;
    const UChar* negNumberStylePattern = NULL;
    int32_t negNumberStylePatternLen = 0;
    // TODO: Java
    // parse to check whether there is ";" separator in the numberStylePattern
    UBool hasSeparator = false;
    if (U_SUCCESS(ec)) {
        for (int32_t styleCharIndex = 0; styleCharIndex < ptnLen; ++styleCharIndex) {
            if (numberStylePattern[styleCharIndex] == gNumberPatternSeparator) {
                hasSeparator = true;
                // split the number style pattern into positive and negative
                negNumberStylePattern = numberStylePattern + styleCharIndex + 1;
                negNumberStylePatternLen = ptnLen - styleCharIndex - 1;
                numberStylePatternLen = styleCharIndex;
            }
        }
    }

    ures_close(numElements);
    ures_close(rb);
    delete ns;

    if (U_FAILURE(ec)) {
        return;
    }

    UResourceBundle *currRb = ures_open(U_ICUDATA_CURR, loc.getName(), &ec);
    UResourceBundle *currencyRes = ures_getByKeyWithFallback(currRb, gCurrUnitPtnTag, NULL, &ec);
    
#ifdef CURRENCY_PLURAL_INFO_DEBUG
    std::cout << "in set up\n";
#endif
    StringEnumeration* keywords = fPluralRules->getKeywords(ec);
    if (U_SUCCESS(ec)) {
        const char* pluralCount;
        while ((pluralCount = keywords->next(NULL, ec)) != NULL) {
            if ( U_SUCCESS(ec) ) {
                int32_t ptnLen;
                UErrorCode err = U_ZERO_ERROR;
                const UChar* patternChars = ures_getStringByKeyWithFallback(
                    currencyRes, pluralCount, &ptnLen, &err);
                if (U_SUCCESS(err) && ptnLen > 0) {
                    UnicodeString* pattern = new UnicodeString(patternChars, ptnLen);
#ifdef CURRENCY_PLURAL_INFO_DEBUG
                    char result_1[1000];
                    pattern->extract(0, pattern->length(), result_1, "UTF-8");
                    std::cout << "pluralCount: " << pluralCount << "; pattern: " << result_1 << "\n";
#endif
                    pattern->findAndReplace(UnicodeString(TRUE, gPart0, 3), 
                      UnicodeString(numberStylePattern, numberStylePatternLen));
                    pattern->findAndReplace(UnicodeString(TRUE, gPart1, 3), UnicodeString(TRUE, gTripleCurrencySign, 3));

                    if (hasSeparator) {
                        UnicodeString negPattern(patternChars, ptnLen);
                        negPattern.findAndReplace(UnicodeString(TRUE, gPart0, 3), 
                          UnicodeString(negNumberStylePattern, negNumberStylePatternLen));
                        negPattern.findAndReplace(UnicodeString(TRUE, gPart1, 3), UnicodeString(TRUE, gTripleCurrencySign, 3));
                        pattern->append(gNumberPatternSeparator);
                        pattern->append(negPattern);
                    }
#ifdef CURRENCY_PLURAL_INFO_DEBUG
                    pattern->extract(0, pattern->length(), result_1, "UTF-8");
                    std::cout << "pluralCount: " << pluralCount << "; pattern: " << result_1 << "\n";
#endif

                    fPluralCountToCurrencyUnitPattern->put(UnicodeString(pluralCount, -1, US_INV), pattern, status);
                }
            }
        }
    }
    delete keywords;
    ures_close(currencyRes);
    ures_close(currRb);
}



void
CurrencyPluralInfo::deleteHash(Hashtable* hTable) 
{
    if ( hTable == NULL ) {
        return;
    }
    int32_t pos = UHASH_FIRST;
    const UHashElement* element = NULL;
    while ( (element = hTable->nextElement(pos)) != NULL ) {
        const UHashTok valueTok = element->value;
        const UnicodeString* value = (UnicodeString*)valueTok.pointer;
        delete value;
    }
    delete hTable;
    hTable = NULL;
}


Hashtable*
CurrencyPluralInfo::initHash(UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return NULL;
    }
    Hashtable* hTable;
    if ( (hTable = new Hashtable(TRUE, status)) == NULL ) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
    if ( U_FAILURE(status) ) {
        delete hTable; 
        return NULL;
    }
    hTable->setValueComparator(ValueComparator);
    return hTable;
}


void
CurrencyPluralInfo::copyHash(const Hashtable* source,
                           Hashtable* target,
                           UErrorCode& status) {
    if ( U_FAILURE(status) ) {
        return;
    }
    int32_t pos = UHASH_FIRST;
    const UHashElement* element = NULL;
    if ( source ) {
        while ( (element = source->nextElement(pos)) != NULL ) {
            const UHashTok keyTok = element->key;
            const UnicodeString* key = (UnicodeString*)keyTok.pointer;
            const UHashTok valueTok = element->value;
            const UnicodeString* value = (UnicodeString*)valueTok.pointer;
            UnicodeString* copy = new UnicodeString(*value);
            target->put(UnicodeString(*key), copy, status);
            if ( U_FAILURE(status) ) {
                return;
            }
        }
    }
}


U_NAMESPACE_END

#endif
