// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/******************************************************************************
*
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  stubdata.h
*
*   This header file is intended to be internal and only included in the 
*   accompanying implementation file. This file declares a single entry
*   point for visibility of tools like TAPI.
*
*   Define initialized data that will build into a valid, but empty
*   ICU data library.  Used to bootstrap the ICU build, which has these
*   dependencies:
*       ICU Common library depends on ICU data
*       ICU data requires data building tools.
*       ICU data building tools require the ICU common library.
*
*   The stub data library (for which this file is the source) is sufficient
*   for running the data building tools.
*
*/

#ifndef __STUBDATA_H__
#define __STUBDATA_H__

#include "unicode/utypes.h"
#include "unicode/udata.h"
#include "unicode/uversion.h"

typedef struct {
    uint16_t headerSize;
    uint8_t magic1, magic2;
    UDataInfo info;
    char padding[8];
    uint32_t count, reserved;
    /*
    const struct {
    const char *const name;
    const void *const data;
    } toc[1];
    */
   int   fakeNameAndData[4];       /* TODO:  Change this header type from */
                                   /*        pointerTOC to OffsetTOC.     */
} ICU_Data_Header;

extern "C" U_EXPORT const ICU_Data_Header U_ICUDATA_ENTRY_POINT;

#endif /* __STUBDATA_H__ */
