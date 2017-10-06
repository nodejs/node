// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  writesrc.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005apr23
*   created by: Markus W. Scherer
*
*   Helper functions for writing source code for data.
*/

#ifndef __WRITESRC_H__
#define __WRITESRC_H__

#include <stdio.h>
#include "unicode/utypes.h"
#include "utrie2.h"

/**
 * Creates a source text file and writes a header comment with the ICU copyright.
 * Writes a C/Java-style comment with the generator name.
 */
U_CAPI FILE * U_EXPORT2
usrc_create(const char *path, const char *filename, const char *generator);

/**
 * Creates a source text file and writes a header comment with the ICU copyright.
 * Writes the comment with # lines, as used in scripts and text data.
 */
U_CAPI FILE * U_EXPORT2
usrc_createTextData(const char *path, const char *filename, const char *generator);

/**
 * Writes the contents of an array of 8/16/32-bit words.
 * The prefix and postfix are optional (can be NULL) and are written first/last.
 * The prefix may contain a %ld or similar field for the array length.
 * The {} and declaration etc. need to be included in prefix/postfix or
 * printed before and after the array contents.
 */
U_CAPI void U_EXPORT2
usrc_writeArray(FILE *f,
                const char *prefix,
                const void *p, int32_t width, int32_t length,
                const char *postfix);

/**
 * Calls usrc_writeArray() for the index and data arrays of a frozen UTrie2.
 * Only the index array is written for a 16-bit UTrie2. In this case, dataPrefix
 * is ignored and can be NULL.
 */
U_CAPI void U_EXPORT2
usrc_writeUTrie2Arrays(FILE *f,
                       const char *indexPrefix, const char *dataPrefix,
                       const UTrie2 *pTrie,
                       const char *postfix);

/**
 * Writes the UTrie2 struct values.
 * The {} and declaration etc. need to be included in prefix/postfix or
 * printed before and after the array contents.
 */
U_CAPI void U_EXPORT2
usrc_writeUTrie2Struct(FILE *f,
                       const char *prefix,
                       const UTrie2 *pTrie,
                       const char *indexName, const char *dataName,
                       const char *postfix);

/**
 * Writes the contents of an array of mostly invariant characters.
 * Characters 0..0x1f are printed as numbers,
 * others as characters with single quotes: '%c'.
 *
 * The prefix and postfix are optional (can be NULL) and are written first/last.
 * The prefix may contain a %ld or similar field for the array length.
 * The {} and declaration etc. need to be included in prefix/postfix or
 * printed before and after the array contents.
 */
U_CAPI void U_EXPORT2
usrc_writeArrayOfMostlyInvChars(FILE *f,
                                const char *prefix,
                                const char *p, int32_t length,
                                const char *postfix);

#endif
