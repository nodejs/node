/*
**********************************************************************
*   Copyright (C) 1999-2015 International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*
*
*  ucnv_bld.h:
*  Contains internal data structure definitions
* Created by Bertrand A. Damiba
*
*   Change history:
*
*   06/29/2000  helena      Major rewrite of the callback APIs.
*/

#ifndef UCNV_BLD_H
#define UCNV_BLD_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv.h"
#include "unicode/ucnv_err.h"
#include "unicode/utf16.h"
#include "ucnv_cnv.h"
#include "ucnvmbcs.h"
#include "ucnv_ext.h"
#include "udataswp.h"

/* size of the overflow buffers in UConverter, enough for escaping callbacks */
#define UCNV_ERROR_BUFFER_LENGTH 32

/* at most 4 bytes per substitution character (part of .cnv file format! see UConverterStaticData) */
#define UCNV_MAX_SUBCHAR_LEN 4

/* at most 8 bytes per character in toUBytes[] (UTF-8 uses up to 6) */
#define UCNV_MAX_CHAR_LEN 8

/* converter options bits */
#define UCNV_OPTION_VERSION     0xf
#define UCNV_OPTION_SWAP_LFNL   0x10

#define UCNV_GET_VERSION(cnv) ((cnv)->options&UCNV_OPTION_VERSION)

U_CDECL_BEGIN /* We must declare the following as 'extern "C"' so that if ucnv
                 itself is compiled under C++, the linkage of the funcptrs will
                 work.
              */

union UConverterTable {
    UConverterMBCSTable mbcs;
};

typedef union UConverterTable UConverterTable;

struct UConverterImpl;
typedef struct UConverterImpl UConverterImpl;

/** values for the unicodeMask */
#define UCNV_HAS_SUPPLEMENTARY 1
#define UCNV_HAS_SURROGATES    2

typedef struct UConverterStaticData {   /* +offset: size */
    uint32_t structSize;                /* +0: 4 Size of this structure */

    char name
      [UCNV_MAX_CONVERTER_NAME_LENGTH]; /* +4: 60  internal name of the converter- invariant chars */

    int32_t codepage;               /* +64: 4 codepage # (now IBM-$codepage) */

    int8_t platform;                /* +68: 1 platform of the converter (only IBM now) */
    int8_t conversionType;          /* +69: 1 conversion type */

    int8_t minBytesPerChar;         /* +70: 1 Minimum # bytes per char in this codepage */
    int8_t maxBytesPerChar;         /* +71: 1 Maximum # bytes output per UChar in this codepage */

    uint8_t subChar[UCNV_MAX_SUBCHAR_LEN]; /* +72: 4  [note:  4 and 8 byte boundary] */
    int8_t subCharLen;              /* +76: 1 */

    uint8_t hasToUnicodeFallback;   /* +77: 1 UBool needs to be changed to UBool to be consistent across platform */
    uint8_t hasFromUnicodeFallback; /* +78: 1 */
    uint8_t unicodeMask;            /* +79: 1  bit 0: has supplementary  bit 1: has single surrogates */
    uint8_t subChar1;               /* +80: 1  single-byte substitution character for IBM MBCS (0 if none) */
    uint8_t reserved[19];           /* +81: 19 to round out the structure */
                                    /* total size: 100 */
} UConverterStaticData;

/*
 * Defines the UConverterSharedData struct,
 * the immutable, shared part of UConverter.
 */
struct UConverterSharedData {
    uint32_t structSize;            /* Size of this structure */
    uint32_t referenceCounter;      /* used to count number of clients, unused for static/immutable SharedData */

    const void *dataMemory;         /* from udata_openChoice() - for cleanup */

    const UConverterStaticData *staticData; /* pointer to the static (non changing) data. */

    UBool                sharedDataCached;   /* TRUE:  shared data is in cache, don't destroy on ucnv_close() if 0 ref.  FALSE: shared data isn't in the cache, do attempt to clean it up if the ref is 0 */
    /** If FALSE, then referenceCounter is not used. Must not change after initialization. */
    UBool isReferenceCounted;

    const UConverterImpl *impl;     /* vtable-style struct of mostly function pointers */

    /*initial values of some members of the mutable part of object */
    uint32_t toUnicodeStatus;

    /*
     * Shared data structures currently come in two flavors:
     * - readonly for built-in algorithmic converters
     * - allocated for MBCS, with a pointer to an allocated UConverterTable
     *   which always has a UConverterMBCSTable
     *
     * To eliminate one allocation, I am making the UConverterMBCSTable
     * a member of the shared data.
     *
     * markus 2003-nov-07
     */
    UConverterMBCSTable mbcs;
};

/** UConverterSharedData initializer for static, non-reference-counted converters. */
#define UCNV_IMMUTABLE_SHARED_DATA_INITIALIZER(pStaticData, pImpl) \
    { \
        sizeof(UConverterSharedData), ~((uint32_t)0), \
        NULL, pStaticData, FALSE, FALSE, pImpl, \
        0, UCNV_MBCS_TABLE_INITIALIZER \
    }

/* Defines a UConverter, the lightweight mutable part the user sees */

struct UConverter {
    /*
     * Error function pointer called when conversion issues
     * occur during a ucnv_fromUnicode call
     */
    void (U_EXPORT2 *fromUCharErrorBehaviour) (const void *context,
                                     UConverterFromUnicodeArgs *args,
                                     const UChar *codeUnits,
                                     int32_t length,
                                     UChar32 codePoint,
                                     UConverterCallbackReason reason,
                                     UErrorCode *);
    /*
     * Error function pointer called when conversion issues
     * occur during a ucnv_toUnicode call
     */
    void (U_EXPORT2 *fromCharErrorBehaviour) (const void *context,
                                    UConverterToUnicodeArgs *args,
                                    const char *codeUnits,
                                    int32_t length,
                                    UConverterCallbackReason reason,
                                    UErrorCode *);

    /*
     * Pointer to additional data that depends on the converter type.
     * Used by ISO 2022, SCSU, GB 18030 converters, possibly more.
     */
    void *extraInfo;

    const void *fromUContext;
    const void *toUContext;

    /*
     * Pointer to charset bytes for substitution string if subCharLen>0,
     * or pointer to Unicode string (UChar *) if subCharLen<0.
     * subCharLen==0 is equivalent to using a skip callback.
     * If the pointer is !=subUChars then it is allocated with
     * UCNV_ERROR_BUFFER_LENGTH * U_SIZEOF_UCHAR bytes.
     * The subUChars field is declared as UChar[] not uint8_t[] to
     * guarantee alignment for UChars.
     */
    uint8_t *subChars;

    UConverterSharedData *sharedData;   /* Pointer to the shared immutable part of the converter object */

    uint32_t options; /* options flags from UConverterOpen, may contain additional bits */

    UBool sharedDataIsCached;  /* TRUE:  shared data is in cache, don't destroy on ucnv_close() if 0 ref.  FALSE: shared data isn't in the cache, do attempt to clean it up if the ref is 0 */
    UBool isCopyLocal;  /* TRUE if UConverter is not owned and not released in ucnv_close() (stack-allocated, safeClone(), etc.) */
    UBool isExtraLocal; /* TRUE if extraInfo is not owned and not released in ucnv_close() (stack-allocated, safeClone(), etc.) */

    UBool  useFallback;
    int8_t toULength;                   /* number of bytes in toUBytes */
    uint8_t toUBytes[UCNV_MAX_CHAR_LEN-1];/* more "toU status"; keeps the bytes of the current character */
    uint32_t toUnicodeStatus;           /* Used to internalize stream status information */
    int32_t mode;
    uint32_t fromUnicodeStatus;

    /*
     * More fromUnicode() status. Serves 3 purposes:
     * - keeps a lead surrogate between buffers (similar to toUBytes[])
     * - keeps a lead surrogate at the end of the stream,
     *   which the framework handles as truncated input
     * - if the fromUnicode() implementation returns to the framework
     *   (ucnv.c ucnv_fromUnicode()), then the framework calls the callback
     *   for this code point
     */
    UChar32 fromUChar32;

    /*
     * value for ucnv_getMaxCharSize()
     *
     * usually simply copied from the static data, but ucnvmbcs.c modifies
     * the value depending on the converter type and options
     */
    int8_t maxBytesPerUChar;

    int8_t subCharLen;                  /* length of the codepage specific character sequence */
    int8_t invalidCharLength;
    int8_t charErrorBufferLength;       /* number of valid bytes in charErrorBuffer */

    int8_t invalidUCharLength;
    int8_t UCharErrorBufferLength;      /* number of valid UChars in charErrorBuffer */

    uint8_t subChar1;                                   /* single-byte substitution character if different from subChar */
    UBool useSubChar1;
    char invalidCharBuffer[UCNV_MAX_CHAR_LEN];          /* bytes from last error/callback situation */
    uint8_t charErrorBuffer[UCNV_ERROR_BUFFER_LENGTH];  /* codepage output from Error functions */
    UChar subUChars[UCNV_MAX_SUBCHAR_LEN/U_SIZEOF_UCHAR]; /* see subChars documentation */

    UChar invalidUCharBuffer[U16_MAX_LENGTH];           /* UChars from last error/callback situation */
    UChar UCharErrorBuffer[UCNV_ERROR_BUFFER_LENGTH];   /* unicode output from Error functions */

    /* fields for conversion extension */

    /* store previous UChars/chars to continue partial matches */
    UChar32 preFromUFirstCP;                /* >=0: partial match */
    UChar preFromU[UCNV_EXT_MAX_UCHARS];
    char preToU[UCNV_EXT_MAX_BYTES];
    int8_t preFromULength, preToULength;    /* negative: replay */
    int8_t preToUFirstLength;               /* length of first character */

    /* new fields for ICU 4.0 */
    UConverterCallbackReason toUCallbackReason; /* (*fromCharErrorBehaviour) reason, set when error is detected */
};

U_CDECL_END /* end of UConverter */

#define CONVERTER_FILE_EXTENSION ".cnv"


/**
 * Return the number of all converter names.
 * @param pErrorCode The error code
 * @return the number of all converter names
 */
U_CFUNC uint16_t
ucnv_bld_countAvailableConverters(UErrorCode *pErrorCode);

/**
 * Return the (n)th converter name in mixed case, or NULL
 * if there is none (typically, if the data cannot be loaded).
 * 0<=index<ucnv_io_countAvailableConverters().
 * @param n The number specifies which converter name to get
 * @param pErrorCode The error code
 * @return the (n)th converter name in mixed case, or NULL if there is none.
 */
U_CFUNC const char *
ucnv_bld_getAvailableConverter(uint16_t n, UErrorCode *pErrorCode);

/**
 * Load a non-algorithmic converter.
 * If pkg==NULL, then this function must be called inside umtx_lock(&cnvCacheMutex).
 */
U_CAPI UConverterSharedData *
ucnv_load(UConverterLoadArgs *pArgs, UErrorCode *err);

/**
 * Unload a non-algorithmic converter.
 * It must be sharedData->isReferenceCounted
 * and this function must be called inside umtx_lock(&cnvCacheMutex).
 */
U_CAPI void
ucnv_unload(UConverterSharedData *sharedData);

/**
 * Swap ICU .cnv conversion tables. See udataswp.h.
 * @internal
 */
U_CAPI int32_t U_EXPORT2
ucnv_swap(const UDataSwapper *ds,
          const void *inData, int32_t length, void *outData,
          UErrorCode *pErrorCode);

#endif

#endif /* _UCNV_BLD */
