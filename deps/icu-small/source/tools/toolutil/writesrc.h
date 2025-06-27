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
#include "unicode/ucpmap.h"
#include "unicode/ucptrie.h"
#include "unicode/umutablecptrie.h"
#include "unicode/uset.h"
#include "utrie2.h"

/**
 * An input to some of the functions in this file specifying whether to write data
 * as C/C++ code initializers or as TOML.
 */
typedef enum UTargetSyntax {
    UPRV_TARGET_SYNTAX_CCODE = 0,
    UPRV_TARGET_SYNTAX_TOML = 1,
} UTargetSyntax;

/**
 * Creates a source text file and writes a header comment with the ICU copyright.
 * Writes a C/Java-style comment with the generator name.
 */
U_CAPI FILE * U_EXPORT2
usrc_create(const char *path, const char *filename, int32_t copyrightYear, const char *generator);

/**
 * Creates a source text file and writes a header comment with the ICU copyright.
 * Writes the comment with # lines, as used in scripts and text data.
 */
U_CAPI FILE * U_EXPORT2
usrc_createTextData(const char *path, const char *filename, int32_t copyrightYear, const char *generator);

/**
 * Writes the ICU copyright to a file stream, with configurable year and comment style.
 */
U_CAPI void U_EXPORT2
usrc_writeCopyrightHeader(FILE *f, const char *prefix, int32_t copyrightYear);

/**
 * Writes information about the file being machine-generated.
 */
U_CAPI void U_EXPORT2
usrc_writeFileNameGeneratedBy(
        FILE *f,
        const char *prefix,
        const char *filename,
        const char *generator);

/**
 * Writes the contents of an array of 8/16/32/64-bit words.
 * The prefix and postfix are optional (can be NULL) and are written first/last.
 * The prefix may contain a %ld or similar field for the array length.
 * The {} and declaration etc. need to be included in prefix/postfix or
 * printed before and after the array contents.
 */
U_CAPI void U_EXPORT2
usrc_writeArray(FILE *f,
                const char *prefix,
                const void *p, int32_t width, int32_t length,
                const char *indent,
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
 * Calls usrc_writeArray() for the index and data arrays of a UCPTrie.
 */
U_CAPI void U_EXPORT2
usrc_writeUCPTrieArrays(FILE *f,
                        const char *indexPrefix, const char *dataPrefix,
                        const UCPTrie *pTrie,
                        const char *postfix,
                        UTargetSyntax syntax);

/**
 * Writes the UCPTrie struct values.
 * The {} and declaration etc. need to be included in prefix/postfix or
 * printed before and after the array contents.
 */
U_CAPI void U_EXPORT2
usrc_writeUCPTrieStruct(FILE *f,
                        const char *prefix,
                        const UCPTrie *pTrie,
                        const char *indexName, const char *dataName,
                        const char *postfix,
                        UTargetSyntax syntax);

/**
 * Writes the UCPTrie arrays and struct values.
 */
U_CAPI void U_EXPORT2
usrc_writeUCPTrie(FILE *f, const char *name, const UCPTrie *pTrie, UTargetSyntax syntax);

/**
 * Writes the UnicodeSet range and string lists.
 */
U_CAPI void U_EXPORT2
usrc_writeUnicodeSet(
    FILE *f,
    const USet *pSet,
    UTargetSyntax syntax);

#ifdef __cplusplus

U_NAMESPACE_BEGIN

class U_TOOLUTIL_API ValueNameGetter {
public:
    virtual ~ValueNameGetter();
    virtual const char *getName(uint32_t value) = 0;
};

U_NAMESPACE_END

/**
 * Writes the UCPMap ranges list.
 *
 * The "valueNameGetter" argument is optional; ignored if nullptr.
 * If present, it will be used to look up value name strings.
 */
U_CAPI void U_EXPORT2
usrc_writeUCPMap(
    FILE *f,
    const UCPMap *pMap,
    icu::ValueNameGetter *valueNameGetter,
    UTargetSyntax syntax);

#endif  // __cplusplus

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

/**
 * Writes a syntactically valid Unicode string in all ASCII, escaping quotes
 * and non-ASCII characters.
 */
U_CAPI void U_EXPORT2
usrc_writeStringAsASCII(FILE *f,
                        const UChar* ptr, int32_t length,
                        UTargetSyntax syntax);

#endif
