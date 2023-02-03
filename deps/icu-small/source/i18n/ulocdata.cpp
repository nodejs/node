// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*                                                                            *
* Copyright (C) 2003-2016, International Business Machines                   *
*                Corporation and others. All Rights Reserved.                *
*                                                                            *
******************************************************************************
*   file name:  ulocdata.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003Oct21
*   created by: Ram Viswanadha,John Emmons
*/

#include "cmemory.h"
#include "unicode/ustring.h"
#include "unicode/ures.h"
#include "unicode/uloc.h"
#include "unicode/ulocdata.h"
#include "uresimp.h"
#include "ureslocs.h"
#include "ulocimp.h"

#define MEASUREMENT_SYSTEM  "MeasurementSystem"
#define PAPER_SIZE          "PaperSize"

/** A locale data object.
 *  For usage in C programs.
 *  @draft ICU 3.4
 */
struct ULocaleData {
    /**
     * Controls the "No Substitute" behavior of this locale data object
     */
    UBool noSubstitute;

    /**
     * Pointer to the resource bundle associated with this locale data object
     */
    UResourceBundle *bundle;

    /**
     * Pointer to the lang resource bundle associated with this locale data object
     */
    UResourceBundle *langBundle;
};

U_CAPI ULocaleData* U_EXPORT2
ulocdata_open(const char *localeID, UErrorCode *status)
{
   ULocaleData *uld;

   if (U_FAILURE(*status)) {
       return NULL;
   }

   uld = (ULocaleData *)uprv_malloc(sizeof(ULocaleData));
   if (uld == NULL) {
      *status = U_MEMORY_ALLOCATION_ERROR;
      return(NULL);
   }

   uld->langBundle = NULL;

   uld->noSubstitute = false;
   uld->bundle = ures_open(NULL, localeID, status);
   uld->langBundle = ures_open(U_ICUDATA_LANG, localeID, status);

   if (U_FAILURE(*status)) {
      uprv_free(uld);
      return NULL;
   }

   return uld;
}

U_CAPI void U_EXPORT2
ulocdata_close(ULocaleData *uld)
{
    if ( uld != NULL ) {
       ures_close(uld->langBundle);
       ures_close(uld->bundle);
       uprv_free(uld);
    }
}

U_CAPI void U_EXPORT2
ulocdata_setNoSubstitute(ULocaleData *uld, UBool setting)
{
   uld->noSubstitute = setting;
}

U_CAPI UBool U_EXPORT2
ulocdata_getNoSubstitute(ULocaleData *uld)
{
   return uld->noSubstitute;
}

U_CAPI USet* U_EXPORT2
ulocdata_getExemplarSet(ULocaleData *uld, USet *fillIn,
                        uint32_t options, ULocaleDataExemplarSetType extype, UErrorCode *status){

    static const char* const exemplarSetTypes[] = { "ExemplarCharacters", 
                                                    "AuxExemplarCharacters", 
                                                    "ExemplarCharactersIndex",
                                                    "ExemplarCharactersPunctuation"};
    const UChar *exemplarChars = NULL;
    int32_t len = 0;
    UErrorCode localStatus = U_ZERO_ERROR;

    if (U_FAILURE(*status))
        return NULL;

    exemplarChars = ures_getStringByKey(uld->bundle, exemplarSetTypes[extype], &len, &localStatus);
    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status))
        return NULL;

    if(fillIn != NULL)
        uset_applyPattern(fillIn, exemplarChars, len,
                          USET_IGNORE_SPACE | options, status);
    else
        fillIn = uset_openPatternOptions(exemplarChars, len,
                                         USET_IGNORE_SPACE | options, status);

    return fillIn;

}

U_CAPI int32_t U_EXPORT2
ulocdata_getDelimiter(ULocaleData *uld, ULocaleDataDelimiterType type,
                      UChar *result, int32_t resultLength, UErrorCode *status){

    static const char* const delimiterKeys[] =  {
        "quotationStart",
        "quotationEnd",
        "alternateQuotationStart",
        "alternateQuotationEnd"
    };

    UResourceBundle *delimiterBundle;
    int32_t len = 0;
    const UChar *delimiter = NULL;
    UErrorCode localStatus = U_ZERO_ERROR;

    if (U_FAILURE(*status))
        return 0;

    delimiterBundle = ures_getByKey(uld->bundle, "delimiters", NULL, &localStatus);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        ures_close(delimiterBundle);
        return 0;
    }

    delimiter = ures_getStringByKeyWithFallback(delimiterBundle, delimiterKeys[type], &len, &localStatus);
    ures_close(delimiterBundle);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        return 0;
    }

    u_strncpy(result,delimiter, resultLength);
    return len;
}

static UResourceBundle * measurementTypeBundleForLocale(const char *localeID, const char *measurementType, UErrorCode *status){
    char region[ULOC_COUNTRY_CAPACITY];
    UResourceBundle *rb;
    UResourceBundle *measTypeBundle = NULL;
    
    ulocimp_getRegionForSupplementalData(localeID, true, region, ULOC_COUNTRY_CAPACITY, status);
    
    rb = ures_openDirect(NULL, "supplementalData", status);
    ures_getByKey(rb, "measurementData", rb, status);
    if (rb != NULL) {
        UResourceBundle *measDataBundle = ures_getByKey(rb, region, NULL, status);
        if (U_SUCCESS(*status)) {
        	measTypeBundle = ures_getByKey(measDataBundle, measurementType, NULL, status);
        }
        if (*status == U_MISSING_RESOURCE_ERROR) {
            *status = U_ZERO_ERROR;
            if (measDataBundle != NULL) {
                ures_close(measDataBundle);
            }
            measDataBundle = ures_getByKey(rb, "001", NULL, status);
            measTypeBundle = ures_getByKey(measDataBundle, measurementType, NULL, status);
        }
        ures_close(measDataBundle);
    }
    ures_close(rb);
    return measTypeBundle;
}

U_CAPI UMeasurementSystem U_EXPORT2
ulocdata_getMeasurementSystem(const char *localeID, UErrorCode *status){

    UResourceBundle* measurement=NULL;
    UMeasurementSystem system = UMS_LIMIT;

    if(status == NULL || U_FAILURE(*status)){
        return system;
    }

    measurement = measurementTypeBundleForLocale(localeID, MEASUREMENT_SYSTEM, status);
    int32_t result = ures_getInt(measurement, status);
    if (U_SUCCESS(*status)) {
         system = static_cast<UMeasurementSystem>(result);
    }

    ures_close(measurement);

    return system;

}

U_CAPI void U_EXPORT2
ulocdata_getPaperSize(const char* localeID, int32_t *height, int32_t *width, UErrorCode *status){
    UResourceBundle* paperSizeBundle = NULL;
    const int32_t* paperSize=NULL;
    int32_t len = 0;

    if(status == NULL || U_FAILURE(*status)){
        return;
    }

    paperSizeBundle = measurementTypeBundleForLocale(localeID, PAPER_SIZE, status);
    paperSize = ures_getIntVector(paperSizeBundle, &len,  status);

    if(U_SUCCESS(*status)){
        if(len < 2){
            *status = U_INTERNAL_PROGRAM_ERROR;
        }else{
            *height = paperSize[0];
            *width  = paperSize[1];
        }
    }

    ures_close(paperSizeBundle);

}

U_CAPI void U_EXPORT2
ulocdata_getCLDRVersion(UVersionInfo versionArray, UErrorCode *status) {
    UResourceBundle *rb = NULL;
    rb = ures_openDirect(NULL, "supplementalData", status);
    ures_getVersionByKey(rb, "cldrVersion", versionArray, status);
    ures_close(rb);
}

U_CAPI int32_t U_EXPORT2
ulocdata_getLocaleDisplayPattern(ULocaleData *uld,
                                 UChar *result,
                                 int32_t resultCapacity,
                                 UErrorCode *status) {
    UResourceBundle *patternBundle;
    int32_t len = 0;
    const UChar *pattern = NULL;
    UErrorCode localStatus = U_ZERO_ERROR;

    if (U_FAILURE(*status))
        return 0;

    patternBundle = ures_getByKey(uld->langBundle, "localeDisplayPattern", NULL, &localStatus);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        ures_close(patternBundle);
        return 0;
    }

    pattern = ures_getStringByKey(patternBundle, "pattern", &len, &localStatus);
    ures_close(patternBundle);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        return 0;
    }

    u_strncpy(result, pattern, resultCapacity);
    return len;
}


U_CAPI int32_t U_EXPORT2
ulocdata_getLocaleSeparator(ULocaleData *uld,
                            UChar *result,
                            int32_t resultCapacity,
                            UErrorCode *status)  {
    UResourceBundle *separatorBundle;
    int32_t len = 0;
    const UChar *separator = NULL;
    UErrorCode localStatus = U_ZERO_ERROR;
    UChar *p0, *p1;
    static const UChar sub0[4] = { 0x007b, 0x0030, 0x007d , 0x0000 }; /* {0} */
    static const UChar sub1[4] = { 0x007b, 0x0031, 0x007d , 0x0000 }; /* {1} */
    static const int32_t subLen = 3;

    if (U_FAILURE(*status))
        return 0;

    separatorBundle = ures_getByKey(uld->langBundle, "localeDisplayPattern", NULL, &localStatus);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        ures_close(separatorBundle);
        return 0;
    }

    separator = ures_getStringByKey(separatorBundle, "separator", &len, &localStatus);
    ures_close(separatorBundle);

    if ( (localStatus == U_USING_DEFAULT_WARNING) && uld->noSubstitute ) {
        localStatus = U_MISSING_RESOURCE_ERROR;
    }

    if (localStatus != U_ZERO_ERROR) {
        *status = localStatus;
    }

    if (U_FAILURE(*status)){
        return 0;
    }

    /* For backwards compatibility, if we have a pattern, return the portion between {0} and {1} */
    p0=u_strstr(separator, sub0);
    p1=u_strstr(separator, sub1);
    if (p0!=NULL && p1!=NULL && p0<=p1) {
        separator = (const UChar *)p0 + subLen;
        len = static_cast<int32_t>(p1 - separator);
        /* Desired separator is no longer zero-terminated; handle that if necessary */
        if (len < resultCapacity) {
            u_strncpy(result, separator, len);
            result[len] = 0;
            return len;
        }
    }

    u_strncpy(result, separator, resultCapacity);
    return len;
}
