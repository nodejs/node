// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2010-2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
*
* File NUMSYS.CPP
*
* Modification History:*
*   Date        Name        Description
*
********************************************************************************
*/

#include "unicode/utypes.h"
#include "unicode/localpointer.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "unicode/ures.h"
#include "unicode/ustring.h"
#include "unicode/uloc.h"
#include "unicode/schriter.h"
#include "unicode/numsys.h"
#include "cstring.h"
#include "uresimp.h"
#include "numsys_impl.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

// Useful constants

#define DEFAULT_DIGITS UNICODE_STRING_SIMPLE("0123456789");
static const char gNumberingSystems[] = "numberingSystems";
static const char gNumberElements[] = "NumberElements";
static const char gDefault[] = "default";
static const char gNative[] = "native";
static const char gTraditional[] = "traditional";
static const char gFinance[] = "finance";
static const char gDesc[] = "desc";
static const char gRadix[] = "radix";
static const char gAlgorithmic[] = "algorithmic";
static const char gLatn[] = "latn";


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NumberingSystem)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(NumsysNameEnumeration)

    /**
     * Default Constructor.
     *
     * @draft ICU 4.2
     */

NumberingSystem::NumberingSystem() {
     radix = 10;
     algorithmic = FALSE;
     UnicodeString defaultDigits = DEFAULT_DIGITS;
     desc.setTo(defaultDigits);
     uprv_strcpy(name,gLatn);
}

    /**
     * Copy constructor.
     * @draft ICU 4.2
     */

NumberingSystem::NumberingSystem(const NumberingSystem& other)
:  UObject(other) {
    *this=other;
}

NumberingSystem* U_EXPORT2
NumberingSystem::createInstance(int32_t radix_in, UBool isAlgorithmic_in, const UnicodeString & desc_in, UErrorCode &status) {

    if (U_FAILURE(status)) {
        return NULL;
    }

    if ( radix_in < 2 ) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if ( !isAlgorithmic_in ) {
       if ( desc_in.countChar32() != radix_in ) {
           status = U_ILLEGAL_ARGUMENT_ERROR;
           return NULL;
       }
    }

    NumberingSystem *ns = new NumberingSystem();

    ns->setRadix(radix_in);
    ns->setDesc(desc_in);
    ns->setAlgorithmic(isAlgorithmic_in);
    ns->setName(NULL);
    return ns;

}


NumberingSystem* U_EXPORT2
NumberingSystem::createInstance(const Locale & inLocale, UErrorCode& status) {

    if (U_FAILURE(status)) {
        return NULL;
    }

    UBool nsResolved = TRUE;
    UBool usingFallback = FALSE;
    char buffer[ULOC_KEYWORDS_CAPACITY];
    int32_t count = inLocale.getKeywordValue("numbers",buffer, sizeof(buffer),status);
    if ( count > 0 ) { // @numbers keyword was specified in the locale
        buffer[count] = '\0'; // Make sure it is null terminated.
        if ( !uprv_strcmp(buffer,gDefault) || !uprv_strcmp(buffer,gNative) ||
             !uprv_strcmp(buffer,gTraditional) || !uprv_strcmp(buffer,gFinance)) {
            nsResolved = FALSE;
        }
    } else {
        uprv_strcpy(buffer,gDefault);
        nsResolved = FALSE;
    }

    if (!nsResolved) { // Resolve the numbering system ( default, native, traditional or finance ) into a "real" numbering system
        UErrorCode localStatus = U_ZERO_ERROR;
        UResourceBundle *resource = ures_open(NULL, inLocale.getName(), &localStatus);
        UResourceBundle *numberElementsRes = ures_getByKey(resource,gNumberElements,NULL,&localStatus);
        while (!nsResolved) {
            localStatus = U_ZERO_ERROR;
            count = 0;
            const UChar *nsName = ures_getStringByKeyWithFallback(numberElementsRes, buffer, &count, &localStatus);
            if ( count > 0 && count < ULOC_KEYWORDS_CAPACITY ) { // numbering system found
                u_UCharsToChars(nsName,buffer,count);
                buffer[count] = '\0'; // Make sure it is null terminated.
                nsResolved = TRUE;
            }

            if (!nsResolved) { // Fallback behavior per TR35 - traditional falls back to native, finance and native fall back to default
                if (!uprv_strcmp(buffer,gNative) || !uprv_strcmp(buffer,gFinance)) {
                    uprv_strcpy(buffer,gDefault);
                } else if (!uprv_strcmp(buffer,gTraditional)) {
                    uprv_strcpy(buffer,gNative);
                } else { // If we get here we couldn't find even the default numbering system
                    usingFallback = TRUE;
                    nsResolved = TRUE;
                }
            }
        }
        ures_close(numberElementsRes);
        ures_close(resource);
    }

    if (usingFallback) {
        status = U_USING_FALLBACK_WARNING;
        NumberingSystem *ns = new NumberingSystem();
        return ns;
    } else {
        return NumberingSystem::createInstanceByName(buffer,status);
    }
 }

NumberingSystem* U_EXPORT2
NumberingSystem::createInstance(UErrorCode& status) {
    return NumberingSystem::createInstance(Locale::getDefault(), status);
}

NumberingSystem* U_EXPORT2
NumberingSystem::createInstanceByName(const char *name, UErrorCode& status) {
    UResourceBundle *numberingSystemsInfo = NULL;
    UResourceBundle *nsTop, *nsCurrent;
    int32_t radix = 10;
    int32_t algorithmic = 0;

    numberingSystemsInfo = ures_openDirect(NULL,gNumberingSystems, &status);
    nsCurrent = ures_getByKey(numberingSystemsInfo,gNumberingSystems,NULL,&status);
    nsTop = ures_getByKey(nsCurrent,name,NULL,&status);
    UnicodeString nsd = ures_getUnicodeStringByKey(nsTop,gDesc,&status);

    ures_getByKey(nsTop,gRadix,nsCurrent,&status);
    radix = ures_getInt(nsCurrent,&status);

    ures_getByKey(nsTop,gAlgorithmic,nsCurrent,&status);
    algorithmic = ures_getInt(nsCurrent,&status);

    UBool isAlgorithmic = ( algorithmic == 1 );

    ures_close(nsCurrent);
    ures_close(nsTop);
    ures_close(numberingSystemsInfo);

    if (U_FAILURE(status)) {
        status = U_UNSUPPORTED_ERROR;
        return NULL;
    }

    NumberingSystem* ns = NumberingSystem::createInstance(radix,isAlgorithmic,nsd,status);
    ns->setName(name);
    return ns;
}

    /**
     * Destructor.
     * @draft ICU 4.2
     */
NumberingSystem::~NumberingSystem() {
}

int32_t NumberingSystem::getRadix() const {
    return radix;
}

UnicodeString NumberingSystem::getDescription() const {
    return desc;
}

const char * NumberingSystem::getName() const {
    return name;
}

void NumberingSystem::setRadix(int32_t r) {
    radix = r;
}

void NumberingSystem::setAlgorithmic(UBool c) {
    algorithmic = c;
}

void NumberingSystem::setDesc(UnicodeString d) {
    desc.setTo(d);
}
void NumberingSystem::setName(const char *n) {
    if ( n == NULL ) {
        name[0] = (char) 0;
    } else {
        uprv_strncpy(name,n,NUMSYS_NAME_CAPACITY);
        name[NUMSYS_NAME_CAPACITY] = (char)0; // Make sure it is null terminated.
    }
}
UBool NumberingSystem::isAlgorithmic() const {
    return ( algorithmic );
}

StringEnumeration* NumberingSystem::getAvailableNames(UErrorCode &status) {
    // TODO(ticket #11908): Init-once static cache, with u_cleanup() callback.
    static StringEnumeration* availableNames = NULL;

    if (U_FAILURE(status)) {
        return NULL;
    }

    if ( availableNames == NULL ) {
        // TODO: Simple array of UnicodeString objects, based on length of table resource?
        LocalPointer<UVector> numsysNames(new UVector(uprv_deleteUObject, NULL, status), status);
        if (U_FAILURE(status)) {
            return NULL;
        }

        UErrorCode rbstatus = U_ZERO_ERROR;
        UResourceBundle *numberingSystemsInfo = ures_openDirect(NULL, "numberingSystems", &rbstatus);
        numberingSystemsInfo = ures_getByKey(numberingSystemsInfo,"numberingSystems",numberingSystemsInfo,&rbstatus);
        if(U_FAILURE(rbstatus)) {
            status = U_MISSING_RESOURCE_ERROR;
            ures_close(numberingSystemsInfo);
            return NULL;
        }

        while ( ures_hasNext(numberingSystemsInfo) ) {
            UResourceBundle *nsCurrent = ures_getNextResource(numberingSystemsInfo,NULL,&rbstatus);
            const char *nsName = ures_getKey(nsCurrent);
            numsysNames->addElement(new UnicodeString(nsName, -1, US_INV),status);
            ures_close(nsCurrent);
        }

        ures_close(numberingSystemsInfo);
        if (U_FAILURE(status)) {
            return NULL;
        }
        availableNames = new NumsysNameEnumeration(numsysNames.getAlias(), status);
        if (availableNames == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        numsysNames.orphan();  // The names got adopted.
    }

    return availableNames;
}

NumsysNameEnumeration::NumsysNameEnumeration(UVector *numsysNames, UErrorCode& /*status*/) {
    pos=0;
    fNumsysNames = numsysNames;
}

const UnicodeString*
NumsysNameEnumeration::snext(UErrorCode& status) {
    if (U_SUCCESS(status) && pos < fNumsysNames->size()) {
        return (const UnicodeString*)fNumsysNames->elementAt(pos++);
    }
    return NULL;
}

void
NumsysNameEnumeration::reset(UErrorCode& /*status*/) {
    pos=0;
}

int32_t
NumsysNameEnumeration::count(UErrorCode& /*status*/) const {
    return (fNumsysNames==NULL) ? 0 : fNumsysNames->size();
}

NumsysNameEnumeration::~NumsysNameEnumeration() {
    delete fNumsysNames;
}
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
