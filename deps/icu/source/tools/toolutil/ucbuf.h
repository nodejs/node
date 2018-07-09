// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File ucbuf.h
*
* Modification History:
*
*   Date        Name        Description
*   05/10/01    Ram         Creation.
*
* This API reads in files and returns UChars
*******************************************************************************
*/

#include "unicode/localpointer.h"
#include "unicode/ucnv.h"
#include "filestrm.h"

#if !UCONFIG_NO_CONVERSION

#ifndef UCBUF_H
#define UCBUF_H 1

typedef struct UCHARBUF UCHARBUF;
/**
 * End of file value
 */
#define U_EOF 0xFFFFFFFF
/**
 * Error value if a sequence cannot be unescaped
 */
#define U_ERR 0xFFFFFFFE

typedef struct ULine ULine;

struct  ULine {
    UChar     *name;
    int32_t   len;
};

/**
 * Opens the UCHARBUF with the given file stream and code page for conversion
 * @param fileName  Name of the file to open.
 * @param codepage  The encoding of the file stream to convert to Unicode.
 *                  If *codepoge is NULL on input the API will try to autodetect
 *                  popular Unicode encodings
 * @param showWarning Flag to print out warnings to STDOUT
 * @param buffered  If TRUE performs a buffered read of the input file. If FALSE reads
 *                  the whole file into memory and converts it.
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 * @return pointer to the newly opened UCHARBUF
 */
U_CAPI UCHARBUF* U_EXPORT2
ucbuf_open(const char* fileName,const char** codepage,UBool showWarning, UBool buffered, UErrorCode* err);

/**
 * Gets a UTF-16 code unit at the current position from the converted buffer
 * and increments the current position
 * @param buf Pointer to UCHARBUF structure
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 */
U_CAPI int32_t U_EXPORT2
ucbuf_getc(UCHARBUF* buf,UErrorCode* err);

/**
 * Gets a UTF-32 code point at the current position from the converted buffer
 * and increments the current position
 * @param buf Pointer to UCHARBUF structure
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 */
U_CAPI int32_t U_EXPORT2
ucbuf_getc32(UCHARBUF* buf,UErrorCode* err);

/**
 * Gets a UTF-16 code unit at the current position from the converted buffer after
 * unescaping and increments the current position. If the escape sequence is for UTF-32
 * code point (\\Uxxxxxxxx) then a UTF-32 codepoint is returned
 * @param buf Pointer to UCHARBUF structure
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 */
U_CAPI int32_t U_EXPORT2
ucbuf_getcx32(UCHARBUF* buf,UErrorCode* err);

/**
 * Gets a pointer to the current position in the internal buffer and length of the line.
 * It imperative to make a copy of the returned buffer before performing operations on it.
 * @param buf Pointer to UCHARBUF structure
 * @param len Output param to receive the len of the buffer returned till end of the line
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 *        Error: U_TRUNCATED_CHAR_FOUND
 * @return Pointer to the internal buffer, NULL if EOF
 */
U_CAPI const UChar* U_EXPORT2
ucbuf_readline(UCHARBUF* buf,int32_t* len, UErrorCode* err);


/**
 * Resets the buffers and the underlying file stream.
 * @param buf Pointer to UCHARBUF structure
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 */
U_CAPI void U_EXPORT2
ucbuf_rewind(UCHARBUF* buf,UErrorCode* err);

/**
 * Returns a pointer to the internal converted buffer
 * @param buf Pointer to UCHARBUF structure
 * @param len Pointer to int32_t to receive the lenth of buffer
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 * @return Pointer to internal UChar buffer
 */
U_CAPI const UChar* U_EXPORT2
ucbuf_getBuffer(UCHARBUF* buf,int32_t* len,UErrorCode* err);

/**
 * Closes the UCHARBUF structure members and cleans up the malloc'ed memory
 * @param buf Pointer to UCHARBUF structure
 */
U_CAPI void U_EXPORT2
ucbuf_close(UCHARBUF* buf);

#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * \class LocalUCHARBUFPointer
 * "Smart pointer" class, closes a UCHARBUF via ucbuf_close().
 * For most methods see the LocalPointerBase base class.
 *
 * @see LocalPointerBase
 * @see LocalPointer
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUCHARBUFPointer, UCHARBUF, ucbuf_close);

U_NAMESPACE_END

#endif

/**
 * Rewinds the buffer by one codepoint. Does not rewind over escaped characters.
 */
U_CAPI void U_EXPORT2
ucbuf_ungetc(int32_t ungetChar,UCHARBUF* buf);


/**
 * Autodetects the encoding of the file stream. Only Unicode charsets are autodectected.
 * Some Unicode charsets are stateful and need byte identifiers to be converted also to bring
 * the converter to correct state for converting the rest of the stream. So the UConverter parameter
 * is necessary.
 * If the charset was autodetected, the caller must close both the input FileStream
 * and the converter.
 *
 * @param fileName The file name to be opened and encoding autodected
 * @param conv  Output param to receive the opened converter if autodetected; NULL otherwise.
 * @param cp Output param to receive the detected encoding
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 * @return The input FileStream if its charset was autodetected; NULL otherwise.
 */
U_CAPI FileStream * U_EXPORT2
ucbuf_autodetect(const char* fileName, const char** cp,UConverter** conv,
int32_t* signatureLength, UErrorCode* status);

/**
 * Autodetects the encoding of the file stream. Only Unicode charsets are autodectected.
 * Some Unicode charsets are stateful and need byte identifiers to be converted also to bring
 * the converter to correct state for converting the rest of the stream. So the UConverter parameter
 * is necessary.
 * If the charset was autodetected, the caller must close the converter.
 *
 * @param fileStream The file stream whose encoding is to be detected
 * @param conv  Output param to receive the opened converter if autodetected; NULL otherwise.
 * @param cp Output param to receive the detected encoding
 * @param err is a pointer to a valid <code>UErrorCode</code> value. If this value
 *        indicates a failure on entry, the function will immediately return.
 *        On exit the value will indicate the success of the operation.
 * @return Boolean whether the Unicode charset was autodetected.
 */

U_CAPI UBool U_EXPORT2
ucbuf_autodetect_fs(FileStream* in, const char** cp, UConverter** conv, int32_t* signatureLength, UErrorCode* status);

/**
 * Returns the approximate size in UChars required for converting the file to UChars
 */
U_CAPI int32_t U_EXPORT2
ucbuf_size(UCHARBUF* buf);

U_CAPI const char* U_EXPORT2
ucbuf_resolveFileName(const char* inputDir, const char* fileName, char* target, int32_t* len, UErrorCode* status);

#endif
#endif

