// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1998-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File ufile.c
*
* Modification History:
*
*   Date        Name        Description
*   11/19/98    stephen     Creation.
*   03/12/99    stephen     Modified for new C API.
*   06/16/99    stephen     Changed T_LocaleBundle to u_locbund
*   07/19/99    stephen     Fixed to use ucnv's default codepage.
******************************************************************************
*/

/*
 * fileno is not declared when building with GCC in strict mode.
 */
#if defined(__GNUC__) && defined(__STRICT_ANSI__)
#undef __STRICT_ANSI__
#endif

#include "locmap.h"
#include "unicode/ustdio.h"

#if !UCONFIG_NO_CONVERSION

#include "ufile.h"
#include "unicode/uloc.h"
#include "unicode/ures.h"
#include "unicode/ucnv.h"
#include "unicode/ustring.h"
#include "cstring.h"
#include "cmemory.h"

#if U_PLATFORM_USES_ONLY_WIN32_API && !defined(fileno)
/* Windows likes to rename Unix-like functions */
#define fileno _fileno
#endif

static UFILE*
finit_owner(FILE         *f,
              const char *locale,
              const char *codepage,
              UBool       takeOwnership
              )
{
    UErrorCode status = U_ZERO_ERROR;
    UFILE     *result;
    if(f == NULL) {
        return 0;
    }
    result = (UFILE*) uprv_malloc(sizeof(UFILE));
    if(result == NULL) {
        return 0;
    }

    uprv_memset(result, 0, sizeof(UFILE));
    result->fFileno = fileno(f);

#if U_PLATFORM_USES_ONLY_WIN32_API && _MSC_VER < 1900
    /*
     * Below is a very old workaround (ICU ticket:231).
     *
     * Previously, 'FILE*' from inside and outside ICU's DLL
     * were different, because they pointed into local copies
     * of the io block. At least by VS 2015 the implementation
     * is something like:
     *    stdio = _acrt_iob_func(0)
     * .. which is a function call, so should return the same pointer
     * regardless of call site.
     * As of _MSC_VER 1900 this patch is retired, at 16 years old.
     */
    if (0 <= result->fFileno && result->fFileno <= 2) {
        /* stdin, stdout and stderr need to be special cased for Windows 98 */
#if _MSC_VER >= 1400
        result->fFile = &__iob_func()[_fileno(f)];
#else
        result->fFile = &_iob[_fileno(f)];
#endif
    }
    else
#endif
    {
        result->fFile = f;
    }

    result->str.fBuffer = result->fUCBuffer;
    result->str.fPos    = result->fUCBuffer;
    result->str.fLimit  = result->fUCBuffer;

#if !UCONFIG_NO_FORMATTING
        /* if locale is 0, use the default */
        if(u_locbund_init(&result->str.fBundle, locale) == 0) {
            /* DO NOT FCLOSE HERE! */
            uprv_free(result);
            return 0;
        }
#endif

    /* If the codepage is not "" use the ucnv_open default behavior */
    if(codepage == NULL || *codepage != '\0') {
        result->fConverter = ucnv_open(codepage, &status);
    }
    /* else result->fConverter is already memset'd to NULL. */

    if(U_SUCCESS(status)) {
        result->fOwnFile = takeOwnership;
    }
    else {
#if !UCONFIG_NO_FORMATTING
        u_locbund_close(&result->str.fBundle);
#endif
        /* DO NOT fclose here!!!!!! */
        uprv_free(result);
        result = NULL;
    }

    return result;
}

U_CAPI UFILE* U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_finit(FILE          *f,
        const char    *locale,
        const char    *codepage)
{
    return finit_owner(f, locale, codepage, FALSE);
}

U_CAPI UFILE* U_EXPORT2
u_fadopt(FILE         *f,
        const char    *locale,
        const char    *codepage)
{
    return finit_owner(f, locale, codepage, TRUE);
}

U_CAPI UFILE* U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fopen(const char    *filename,
        const char    *perm,
        const char    *locale,
        const char    *codepage)
{
    UFILE     *result;
    FILE     *systemFile = fopen(filename, perm);
    if(systemFile == 0) {
        return 0;
    }

    result = finit_owner(systemFile, locale, codepage, TRUE);

    if (!result) {
        /* Something bad happened.
           Maybe the converter couldn't be opened. */
        fclose(systemFile);
    }

    return result; /* not a file leak */
}

U_CAPI UFILE* U_EXPORT2
u_fopen_u(const UChar   *filename,
        const char    *perm,
        const char    *locale,
        const char    *codepage)
{
    UFILE     *result;
    char buffer[256];

    u_austrcpy(buffer, filename);

    result = u_fopen(buffer, perm, locale, codepage);
#if U_PLATFORM_USES_ONLY_WIN32_API
    /* Try Windows API _wfopen if the above fails. */
    if (!result) {
        FILE *systemFile = _wfopen(filename, (UChar*)perm);
        if (systemFile) {
            result = finit_owner(systemFile, locale, codepage, TRUE);
        }
        if (!result) {
            /* Something bad happened.
               Maybe the converter couldn't be opened. */
            fclose(systemFile);
        }
    }
#endif
    return result; /* not a file leak */
}

U_CAPI UFILE* U_EXPORT2
u_fstropen(UChar *stringBuf,
           int32_t      capacity,
           const char  *locale)
{
    UFILE *result;

    if (capacity < 0) {
        return NULL;
    }

    result = (UFILE*) uprv_malloc(sizeof(UFILE));
    /* Null pointer test */
    if (result == NULL) {
    	return NULL; /* Just get out. */
    }
    uprv_memset(result, 0, sizeof(UFILE));
    result->str.fBuffer = stringBuf;
    result->str.fPos    = stringBuf;
    result->str.fLimit  = stringBuf+capacity;

#if !UCONFIG_NO_FORMATTING
    /* if locale is 0, use the default */
    if(u_locbund_init(&result->str.fBundle, locale) == 0) {
        /* DO NOT FCLOSE HERE! */
        uprv_free(result);
        return 0;
    }
#endif

    return result;
}

U_CAPI UBool U_EXPORT2
u_feof(UFILE  *f)
{
    UBool endOfBuffer;
    if (f == NULL) {
        return TRUE;
    }
    endOfBuffer = (UBool)(f->str.fPos >= f->str.fLimit);
    if (f->fFile != NULL) {
        return endOfBuffer && feof(f->fFile);
    }
    return endOfBuffer;
}

U_CAPI void U_EXPORT2
u_fflush(UFILE *file)
{
    ufile_flush_translit(file);
    ufile_flush_io(file);
    if (file->fFile) {
        fflush(file->fFile);
    }
    else if (file->str.fPos < file->str.fLimit) {
        *(file->str.fPos++) = 0;
    }
    /* TODO: flush input */
}

U_CAPI void
u_frewind(UFILE *file)
{
    u_fflush(file);
    ucnv_reset(file->fConverter);
    if (file->fFile) {
        rewind(file->fFile);
        file->str.fLimit = file->fUCBuffer;
        file->str.fPos   = file->fUCBuffer;
    }
    else {
        file->str.fPos = file->str.fBuffer;
    }
}

U_CAPI void U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fclose(UFILE *file)
{
    if (file) {
        u_fflush(file);
        ufile_close_translit(file);

        if(file->fOwnFile)
            fclose(file->fFile);

#if !UCONFIG_NO_FORMATTING
        u_locbund_close(&file->str.fBundle);
#endif

        ucnv_close(file->fConverter);
        uprv_free(file);
    }
}

U_CAPI FILE* U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fgetfile(    UFILE         *f)
{
    return f->fFile;
}

#if !UCONFIG_NO_FORMATTING

U_CAPI const char*  U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fgetlocale(    UFILE        *file)
{
    return file->str.fBundle.fLocale;
}

U_CAPI int32_t U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fsetlocale(UFILE      *file,
             const char *locale)
{
    u_locbund_close(&file->str.fBundle);

    return u_locbund_init(&file->str.fBundle, locale) == 0 ? -1 : 0;
}

#endif

U_CAPI const char* U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fgetcodepage(UFILE        *file)
{
    UErrorCode     status = U_ZERO_ERROR;
    const char     *codepage = NULL;

    if (file->fConverter) {
        codepage = ucnv_getName(file->fConverter, &status);
        if(U_FAILURE(status))
            return 0;
    }
    return codepage;
}

U_CAPI int32_t U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fsetcodepage(    const char    *codepage,
               UFILE        *file)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t retVal = -1;

    /* We use the normal default codepage for this system, and not the one for the locale. */
    if ((file->str.fPos == file->str.fBuffer) && (file->str.fLimit == file->str.fBuffer)) {
        ucnv_close(file->fConverter);
        file->fConverter = ucnv_open(codepage, &status);
        if(U_SUCCESS(status)) {
            retVal = 0;
        }
    }
    return retVal;
}


U_CAPI UConverter * U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_fgetConverter(UFILE *file)
{
    return file->fConverter;
}
#if !UCONFIG_NO_FORMATTING
U_CAPI const UNumberFormat* U_EXPORT2 u_fgetNumberFormat(UFILE *file)
{
    return u_locbund_getNumberFormat(&file->str.fBundle, UNUM_DECIMAL);
}
#endif

#endif
