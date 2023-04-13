// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1999-2010, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  unewdata.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999oct25
*   created by: Markus W. Scherer
*/

#ifndef __UNEWDATA_H__
#define __UNEWDATA_H__

#include "unicode/utypes.h"
#include "unicode/udata.h"

/* API for writing data -----------------------------------------------------*/

/** @memo Forward declaration of the data memory creation type. */
typedef struct UNewDataMemory UNewDataMemory;

/**
 * Create a new binary data file.
 * The file-writing <code>udata_</code> functions facilitate writing
 * binary data files that can be read by ICU's <code>udata</code> API.
 * This function opens a new file with a filename determined from its
 * parameters - of the form "name.type".
 * It then writes a short header, followed by the <code>UDataInfo</code>
 * structure and, optionally, by the comment string.
 * It then writes padding bytes to round up to a multiple of 16 bytes.
 * Subsequent write operations will thus start at an offset in the file
 * that is a multiple of 16. <code>udata_getMemory()</code> will return
 * a pointer to this same starting offset.
 *
 * See udata.h .
 *
 * @param dir A string that specifies the directory where the data will be
 *            written. If <code>NULL</code>, then
 *            <code>u_getDataDirectory</code> is used.
 * @param type A string that specifies the type of data to be written.
 *             For example, resource bundles are written with type "res",
 *             conversion tables with type "cnv".
 *             This may be <code>NULL</code> or empty.
 * @param name A string that specifies the name of the data.
 * @param pInfo A pointer to a correctly filled <code>UDataInfo</code>
 *              structure that will be copied into the file.
 * @param comment A string (e.g., a copyright statement) that will be
 *                copied into the file if it is not <code>NULL</code>
 *                or empty. This string serves only as a comment in the binary
 *                file. It will not be accessible by any API.
 * @param pErrorCode An ICU UErrorCode parameter. It must not be <code>NULL</code>.
 */
U_CAPI UNewDataMemory * U_EXPORT2
udata_create(const char *dir, const char *type, const char *name,
             const UDataInfo *pInfo,
             const char *comment,
             UErrorCode *pErrorCode);

/** @memo Close a newly written binary file. */
U_CAPI uint32_t U_EXPORT2
udata_finish(UNewDataMemory *pData, UErrorCode *pErrorCode);

/** @memo Write a dummy data file. */
U_CAPI void U_EXPORT2
udata_createDummy(const char *dir, const char *type, const char *name, UErrorCode *pErrorCode);

/** @memo Write an 8-bit byte to the file. */
U_CAPI void U_EXPORT2
udata_write8(UNewDataMemory *pData, uint8_t byte);

/** @memo Write a 16-bit word to the file. */
U_CAPI void U_EXPORT2
udata_write16(UNewDataMemory *pData, uint16_t word);

/** @memo Write a 32-bit word to the file. */
U_CAPI void U_EXPORT2
udata_write32(UNewDataMemory *pData, uint32_t wyde);

/** @memo Write a block of bytes to the file. */
U_CAPI void U_EXPORT2
udata_writeBlock(UNewDataMemory *pData, const void *s, int32_t length);

/** @memo Write a block of arbitrary padding bytes to the file. */
U_CAPI void U_EXPORT2
udata_writePadding(UNewDataMemory *pData, int32_t length);

/** @memo Write a <code>char*</code> string of platform "invariant characters" to the file. */
U_CAPI void U_EXPORT2
udata_writeString(UNewDataMemory *pData, const char *s, int32_t length);

/** @memo Write a <code>UChar*</code> string of Unicode character code units to the file. */
U_CAPI void U_EXPORT2
udata_writeUString(UNewDataMemory *pData, const UChar *s, int32_t length);


/*
 * Hey, Emacs, please set the following:
 *
 * Local Variables:
 * indent-tabs-mode: nil
 * End:
 *
 */

#endif
