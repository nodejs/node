// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************/


/*----------------------------------------------------------------------------------
 *
 *  UDataMemory     A class-like struct that serves as a handle to a piece of memory
 *                  that contains some ICU data (resource, converters, whatever.)
 *
 *                  When an application opens ICU data (with udata_open, for example,
 *                  a UDataMemory * is returned.
 *
 *----------------------------------------------------------------------------------*/

#include "unicode/utypes.h"
#include "cmemory.h"
#include "unicode/udata.h"

#include "udatamem.h"

U_CFUNC void UDataMemory_init(UDataMemory *This) {
    uprv_memset(This, 0, sizeof(UDataMemory));
    This->length=-1;
}


U_CFUNC void UDatamemory_assign(UDataMemory *dest, UDataMemory *source) {
    /* UDataMemory Assignment.  Destination UDataMemory must be initialized first.  */
    UBool mallocedFlag = dest->heapAllocated;
    uprv_memcpy(dest, source, sizeof(UDataMemory));
    dest->heapAllocated = mallocedFlag;
}

U_CFUNC UDataMemory *UDataMemory_createNewInstance(UErrorCode *pErr) {
    UDataMemory *This;

    if (U_FAILURE(*pErr)) {
        return NULL;
    }
    This = (UDataMemory *)uprv_malloc(sizeof(UDataMemory));
    if (This == NULL) {
        *pErr = U_MEMORY_ALLOCATION_ERROR; }
    else {
        UDataMemory_init(This);
        This->heapAllocated = TRUE;
    }
    return This;
}


U_CFUNC const DataHeader *
UDataMemory_normalizeDataPointer(const void *p) {
    /* allow the data to be optionally prepended with an alignment-forcing double value */
    const DataHeader *pdh = (const DataHeader *)p;
    if(pdh==NULL || (pdh->dataHeader.magic1==0xda && pdh->dataHeader.magic2==0x27)) {
        return pdh;
    } else {
#if U_PLATFORM == U_PF_OS400
        /*
        TODO: Fix this once the compiler implements this feature. Keep in sync with genccode.c

        This is here because this platform can't currently put
        const data into the read-only pages of an object or
        shared library (service program). Only strings are allowed in read-only
        pages, so we use char * strings to store the data.

        In order to prevent the beginning of the data from ever matching the
        magic numbers we must skip the initial double.
        [grhoten 4/24/2003]
        */
        return (const DataHeader *)*((const void **)p+1);
#else
        return (const DataHeader *)((const double *)p+1);
#endif
    }
}


U_CFUNC void UDataMemory_setData (UDataMemory *This, const void *dataAddr) {
    This->pHeader = UDataMemory_normalizeDataPointer(dataAddr);
}


U_CAPI void U_EXPORT2
udata_close(UDataMemory *pData) {
    if(pData!=NULL) {
        uprv_unmapFile(pData);
        if(pData->heapAllocated ) {
            uprv_free(pData);
        } else {
            UDataMemory_init(pData);
        }
    }
}

U_CAPI const void * U_EXPORT2
udata_getMemory(UDataMemory *pData) {
    if(pData!=NULL && pData->pHeader!=NULL) {
        return (char *)(pData->pHeader)+udata_getHeaderSize(pData->pHeader);
    } else {
        return NULL;
    }
}

/**
 * Get the length of the data item if possible.
 * The length may be up to 15 bytes larger than the actual data.
 *
 * TODO Consider making this function public.
 * It would have to return the actual length in more cases.
 * For example, the length of the last item in a .dat package could be
 * computed from the size of the whole .dat package minus the offset of the
 * last item.
 * The size of a file that was directly memory-mapped could be determined
 * using some system API.
 *
 * In order to get perfect values for all data items, we may have to add a
 * length field to UDataInfo, but that complicates data generation
 * and may be overkill.
 *
 * @param pData The data item.
 * @return the length of the data item, or -1 if not known
 * @internal Currently used only in cintltst/udatatst.c
 */
U_CAPI int32_t U_EXPORT2
udata_getLength(const UDataMemory *pData) {
    if(pData!=NULL && pData->pHeader!=NULL && pData->length>=0) {
        /*
         * subtract the header size,
         * return only the size of the actual data starting at udata_getMemory()
         */
        return pData->length-udata_getHeaderSize(pData->pHeader);
    } else {
        return -1;
    }
}

/**
 * Get the memory including the data header.
 * Used in cintltst/udatatst.c
 * @internal
 */
U_CAPI const void * U_EXPORT2
udata_getRawMemory(const UDataMemory *pData) {
    if(pData!=NULL && pData->pHeader!=NULL) {
        return pData->pHeader;
    } else {
        return NULL;
    }
}

U_CFUNC UBool UDataMemory_isLoaded(const UDataMemory *This) {
    return This->pHeader != NULL;
}
