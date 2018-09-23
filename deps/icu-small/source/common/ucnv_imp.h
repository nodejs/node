// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
*
*  ucnv_imp.h:
*  Contains all internal and external data structure definitions
* Created & Maitained by Bertrand A. Damiba
*
*
*
* ATTENTION:
* ---------
* Although the data structures in this file are open and stack allocatable
* we reserve the right to hide them in further releases.
*/

#ifndef UCNV_IMP_H
#define UCNV_IMP_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/uloc.h"
#include "ucnv_bld.h"

/*
 * Fast check for whether a charset name is "UTF-8".
 * This does not recognize all of the variations that ucnv_open()
 * and other functions recognize, but it covers most cases.
 * @param name const char * charset name
 * @return
 */
#define UCNV_FAST_IS_UTF8(name) \
    (((name[0]=='U' ? \
      (                name[1]=='T' && name[2]=='F') : \
      (name[0]=='u' && name[1]=='t' && name[2]=='f'))) \
  && (name[3]=='-' ? \
     (name[4]=='8' && name[5]==0) : \
     (name[3]=='8' && name[4]==0)))

typedef struct {
    char cnvName[UCNV_MAX_CONVERTER_NAME_LENGTH];
    char locale[ULOC_FULLNAME_CAPACITY];
    uint32_t options;
} UConverterNamePieces;

U_CFUNC UBool
ucnv_canCreateConverter(const char *converterName, UErrorCode *err);

/* figures out if we need to go to file to read in the data tables.
 * @param converterName The name of the converter
 * @param err The error code
 * @return the newly created converter
 */
U_CAPI UConverter *
ucnv_createConverter(UConverter *myUConverter, const char *converterName, UErrorCode * err);

/*
 * Open a purely algorithmic converter, specified by a type constant.
 * @param myUConverter  NULL, or pre-allocated UConverter structure to avoid
 *                      a memory allocation
 * @param type          requested converter type
 * @param locale        locale parameter, or ""
 * @param options       converter options bit set (default 0)
 * @param err           ICU error code, not tested for U_FAILURE on input
 *                      because this is an internal function
 * @internal
 */
U_CFUNC UConverter *
ucnv_createAlgorithmicConverter(UConverter *myUConverter,
                                UConverterType type,
                                const char *locale, uint32_t options,
                                UErrorCode *err);

/*
 * Creates a converter from shared data.
 * Adopts mySharedConverterData: No matter what happens, the caller must not
 * unload mySharedConverterData, except via ucnv_close(return value)
 * if this function is successful.
 */
U_CFUNC UConverter *
ucnv_createConverterFromSharedData(UConverter *myUConverter,
                                   UConverterSharedData *mySharedConverterData,
                                   UConverterLoadArgs *pArgs,
                                   UErrorCode *err);

U_CFUNC UConverter *
ucnv_createConverterFromPackage(const char *packageName, const char *converterName, UErrorCode *err);

/**
 * Load a converter but do not create a UConverter object.
 * Simply return the UConverterSharedData.
 * Performs alias lookup etc.
 * The UConverterNamePieces need not be initialized
 * before calling this function.
 * The UConverterLoadArgs must be initialized
 * before calling this function.
 * If the args are passed in, then the pieces must be passed in too.
 * In other words, the following combinations are allowed:
 * - pieces==NULL && args==NULL
 * - pieces!=NULL && args==NULL
 * - pieces!=NULL && args!=NULL
 * @internal
 */
U_CFUNC UConverterSharedData *
ucnv_loadSharedData(const char *converterName,
                    UConverterNamePieces *pieces,
                    UConverterLoadArgs *pArgs,
                    UErrorCode * err);

/**
 * This may unload the shared data in a thread safe manner.
 * This will only unload the data if no other converters are sharing it.
 */
U_CFUNC void
ucnv_unloadSharedDataIfReady(UConverterSharedData *sharedData);

/**
 * This is a thread safe way to increment the reference count.
 */
U_CFUNC void
ucnv_incrementRefCount(UConverterSharedData *sharedData);

/**
 * These are the default error handling callbacks for the charset conversion framework.
 * For performance reasons, they are only called to handle an error (not normally called for a reset or close).
 */
#define UCNV_TO_U_DEFAULT_CALLBACK ((UConverterToUCallback) UCNV_TO_U_CALLBACK_SUBSTITUTE)
#define UCNV_FROM_U_DEFAULT_CALLBACK ((UConverterFromUCallback) UCNV_FROM_U_CALLBACK_SUBSTITUTE)

#endif

#endif /* _UCNV_IMP */
