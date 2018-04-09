/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"

#if !defined(OPENSSL_NO_STDIO)

# include <stdio.h>

FILE *openssl_fopen(const char *filename, const char *mode)
{
    FILE *file = NULL;
# if defined(_WIN32) && defined(CP_UTF8)
    int sz, len_0 = (int)strlen(filename) + 1;
    DWORD flags;

    /*
     * Basically there are three cases to cover: a) filename is
     * pure ASCII string; b) actual UTF-8 encoded string and
     * c) locale-ized string, i.e. one containing 8-bit
     * characters that are meaningful in current system locale.
     * If filename is pure ASCII or real UTF-8 encoded string,
     * MultiByteToWideChar succeeds and _wfopen works. If
     * filename is locale-ized string, chances are that
     * MultiByteToWideChar fails reporting
     * ERROR_NO_UNICODE_TRANSLATION, in which case we fall
     * back to fopen...
     */
    if ((sz = MultiByteToWideChar(CP_UTF8, (flags = MB_ERR_INVALID_CHARS),
                                  filename, len_0, NULL, 0)) > 0 ||
        (GetLastError() == ERROR_INVALID_FLAGS &&
         (sz = MultiByteToWideChar(CP_UTF8, (flags = 0),
                                   filename, len_0, NULL, 0)) > 0)
        ) {
        WCHAR wmode[8];
        WCHAR *wfilename = _alloca(sz * sizeof(WCHAR));

        if (MultiByteToWideChar(CP_UTF8, flags,
                                filename, len_0, wfilename, sz) &&
            MultiByteToWideChar(CP_UTF8, 0, mode, strlen(mode) + 1,
                                wmode, OSSL_NELEM(wmode)) &&
            (file = _wfopen(wfilename, wmode)) == NULL &&
            (errno == ENOENT || errno == EBADF)
            ) {
            /*
             * UTF-8 decode succeeded, but no file, filename
             * could still have been locale-ized...
             */
            file = fopen(filename, mode);
        }
    } else if (GetLastError() == ERROR_NO_UNICODE_TRANSLATION) {
        file = fopen(filename, mode);
    }
# elif defined(__DJGPP__)
    {
        char *newname = NULL;

        if (!HAS_LFN_SUPPORT(filename)) {
            char *iterator;
            char lastchar;

            newname = OPENSSL_malloc(strlen(filename) + 1);
            if (newname == NULL)
                return NULL;

            for (iterator = newname, lastchar = '\0';
                *filename; filename++, iterator++) {
                if (lastchar == '/' && filename[0] == '.'
                    && filename[1] != '.' && filename[1] != '/') {
                    /* Leading dots are not permitted in plain DOS. */
                    *iterator = '_';
                } else {
                    *iterator = *filename;
                }
                lastchar = *filename;
            }
            *iterator = '\0';
            filename = newname;
        }
        file = fopen(filename, mode);

        OPENSSL_free(newname);
    }
# else
    file = fopen(filename, mode);
# endif
    return file;
}

#else

void *openssl_fopen(const char *filename, const char *mode)
{
    return NULL;
}

#endif
