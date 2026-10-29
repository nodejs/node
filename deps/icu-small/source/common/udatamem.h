// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2010, International Business Machines
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
#ifndef __UDATAMEM_H__
#define __UDATAMEM_H__

#include "unicode/udata.h"
#include "ucmndata.h"

struct UDataMemory {
    const commonDataFuncs  *vFuncs;      /* Function Pointers for accessing TOC             */

    const DataHeader *pHeader;     /* Header of the memory being described by this    */
                                   /*   UDataMemory object.                           */
    const void       *toc;         /* For common memory, table of contents for        */
                                   /*   the pieces within.                            */
    UBool             heapAllocated;  /* True if this UDataMemory Object is on the    */
                                   /*  heap and thus needs to be deleted when closed. */

    void             *mapAddr;     /* For mapped or allocated memory, the start addr. */
                                   /* Only non-null if a close operation should unmap */
                                   /*  the associated data.                           */
    void             *map;         /* Handle, or other data, OS dependent.            */
                                   /* Only non-null if a close operation should unmap */
                                   /*  the associated data, and additional info       */
                                   /*   beyond the mapAddr is needed to do that.      */
    int32_t           length;      /* Length of the data in bytes; -1 if unknown.     */
};

U_CAPI  UDataMemory* U_EXPORT2 UDataMemory_createNewInstance(UErrorCode *pErr);
U_CFUNC void         UDatamemory_assign  (UDataMemory *dest, UDataMemory *source);
U_CFUNC void         UDataMemory_init    (UDataMemory *This);
U_CFUNC UBool        UDataMemory_isLoaded(const UDataMemory *This);
U_CFUNC void         UDataMemory_setData (UDataMemory *This, const void *dataAddr);

U_CFUNC const DataHeader *UDataMemory_normalizeDataPointer(const void *p);

U_CAPI int32_t U_EXPORT2
udata_getLength(const UDataMemory *pData);

U_CAPI const void * U_EXPORT2
udata_getRawMemory(const UDataMemory *pData);

#endif
