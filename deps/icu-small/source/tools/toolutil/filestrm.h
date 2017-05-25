// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2005, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File FILESTRM.H
*
* Contains FileStream interface
*
* @author       Glenn Marcy
*
* Modification History:
*
*   Date        Name        Description
*   5/8/98      gm          Created.
*  03/02/99     stephen     Reordered params in ungetc to match stdio
*                           Added wopen
*
******************************************************************************
*/

#ifndef FILESTRM_H
#define FILESTRM_H

#include "unicode/utypes.h"

typedef struct _FileStream FileStream;

U_CAPI FileStream* U_EXPORT2
T_FileStream_open(const char* filename, const char* mode);

/*
U_CAPI FileStream* U_EXPORT2
T_FileStream_wopen(const wchar_t* filename, const wchar_t* mode);
*/
U_CAPI void U_EXPORT2
T_FileStream_close(FileStream* fileStream);

U_CAPI UBool U_EXPORT2
T_FileStream_file_exists(const char* filename);

/*
U_CAPI FileStream* U_EXPORT2
T_FileStream_tmpfile(void);
*/

U_CAPI int32_t U_EXPORT2
T_FileStream_read(FileStream* fileStream, void* addr, int32_t len);

U_CAPI int32_t U_EXPORT2
T_FileStream_write(FileStream* fileStream, const void* addr, int32_t len);

U_CAPI void U_EXPORT2
T_FileStream_rewind(FileStream* fileStream);

/*Added by Bertrand A. D. */
U_CAPI char * U_EXPORT2
T_FileStream_readLine(FileStream* fileStream, char* buffer, int32_t length);

U_CAPI int32_t U_EXPORT2
T_FileStream_writeLine(FileStream* fileStream, const char* buffer);

U_CAPI int32_t U_EXPORT2
T_FileStream_putc(FileStream* fileStream, int32_t ch);

U_CAPI int U_EXPORT2
T_FileStream_getc(FileStream* fileStream);

U_CAPI int32_t U_EXPORT2
T_FileStream_ungetc(int32_t ch, FileStream *fileStream);

U_CAPI int32_t U_EXPORT2
T_FileStream_peek(FileStream* fileStream);

U_CAPI int32_t U_EXPORT2
T_FileStream_size(FileStream* fileStream);

U_CAPI int U_EXPORT2
T_FileStream_eof(FileStream* fileStream);

U_CAPI int U_EXPORT2
T_FileStream_error(FileStream* fileStream);

/*
U_CAPI void U_EXPORT2
T_FileStream_setError(FileStream* fileStream);
*/

U_CAPI FileStream* U_EXPORT2
T_FileStream_stdin(void);

U_CAPI FileStream* U_EXPORT2
T_FileStream_stdout(void);

U_CAPI FileStream* U_EXPORT2
T_FileStream_stderr(void);

U_CAPI UBool U_EXPORT2
T_FileStream_remove(const char* fileName);

#endif /* _FILESTRM*/
