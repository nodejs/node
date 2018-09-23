/* crypto/bio/bss_file.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/*-
 * 03-Dec-1997  rdenny@dc3.com  Fix bug preventing use of stdin/stdout
 *              with binary data (e.g. asn1parse -inform DER < xxx) under
 *              Windows
 */

#ifndef HEADER_BSS_FILE_C
# define HEADER_BSS_FILE_C

# if defined(__linux) || defined(__sun) || defined(__hpux)
/*
 * Following definition aliases fopen to fopen64 on above mentioned
 * platforms. This makes it possible to open and sequentially access files
 * larger than 2GB from 32-bit application. It does not allow to traverse
 * them beyond 2GB with fseek/ftell, but on the other hand *no* 32-bit
 * platform permits that, not with fseek/ftell. Not to mention that breaking
 * 2GB limit for seeking would require surgery to *our* API. But sequential
 * access suffices for practical cases when you can run into large files,
 * such as fingerprinting, so we can let API alone. For reference, the list
 * of 32-bit platforms which allow for sequential access of large files
 * without extra "magic" comprise *BSD, Darwin, IRIX...
 */
#  ifndef _FILE_OFFSET_BITS
#   define _FILE_OFFSET_BITS 64
#  endif
# endif

# include <stdio.h>
# include <errno.h>
# include "cryptlib.h"
# include "bio_lcl.h"
# include <openssl/err.h>

# if defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_CLIB)
#  include <nwfileio.h>
# endif

# if !defined(OPENSSL_NO_STDIO)

static int MS_CALLBACK file_write(BIO *h, const char *buf, int num);
static int MS_CALLBACK file_read(BIO *h, char *buf, int size);
static int MS_CALLBACK file_puts(BIO *h, const char *str);
static int MS_CALLBACK file_gets(BIO *h, char *str, int size);
static long MS_CALLBACK file_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int MS_CALLBACK file_new(BIO *h);
static int MS_CALLBACK file_free(BIO *data);
static BIO_METHOD methods_filep = {
    BIO_TYPE_FILE,
    "FILE pointer",
    file_write,
    file_read,
    file_puts,
    file_gets,
    file_ctrl,
    file_new,
    file_free,
    NULL,
};

BIO *BIO_new_file(const char *filename, const char *mode)
{
    BIO *ret;
    FILE *file = NULL;

#  if defined(_WIN32) && defined(CP_UTF8)
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
                                wmode, sizeof(wmode) / sizeof(wmode[0])) &&
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
#  else
    file = fopen(filename, mode);
#  endif
    if (file == NULL) {
        SYSerr(SYS_F_FOPEN, get_last_sys_error());
        ERR_add_error_data(5, "fopen('", filename, "','", mode, "')");
        if (errno == ENOENT)
            BIOerr(BIO_F_BIO_NEW_FILE, BIO_R_NO_SUCH_FILE);
        else
            BIOerr(BIO_F_BIO_NEW_FILE, ERR_R_SYS_LIB);
        return (NULL);
    }
    if ((ret = BIO_new(BIO_s_file())) == NULL) {
        fclose(file);
        return (NULL);
    }

    BIO_clear_flags(ret, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                             * UPLINK */
    BIO_set_fp(ret, file, BIO_CLOSE);
    return (ret);
}

BIO *BIO_new_fp(FILE *stream, int close_flag)
{
    BIO *ret;

    if ((ret = BIO_new(BIO_s_file())) == NULL)
        return (NULL);

    BIO_set_flags(ret, BIO_FLAGS_UPLINK); /* redundant, left for
                                           * documentation puposes */
    BIO_set_fp(ret, stream, close_flag);
    return (ret);
}

BIO_METHOD *BIO_s_file(void)
{
    return (&methods_filep);
}

static int MS_CALLBACK file_new(BIO *bi)
{
    bi->init = 0;
    bi->num = 0;
    bi->ptr = NULL;
    bi->flags = BIO_FLAGS_UPLINK; /* default to UPLINK */
    return (1);
}

static int MS_CALLBACK file_free(BIO *a)
{
    if (a == NULL)
        return (0);
    if (a->shutdown) {
        if ((a->init) && (a->ptr != NULL)) {
            if (a->flags & BIO_FLAGS_UPLINK)
                UP_fclose(a->ptr);
            else
                fclose(a->ptr);
            a->ptr = NULL;
            a->flags = BIO_FLAGS_UPLINK;
        }
        a->init = 0;
    }
    return (1);
}

static int MS_CALLBACK file_read(BIO *b, char *out, int outl)
{
    int ret = 0;

    if (b->init && (out != NULL)) {
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = UP_fread(out, 1, (int)outl, b->ptr);
        else
            ret = fread(out, 1, (int)outl, (FILE *)b->ptr);
        if (ret == 0
            && (b->flags & BIO_FLAGS_UPLINK) ? UP_ferror((FILE *)b->ptr) :
            ferror((FILE *)b->ptr)) {
            SYSerr(SYS_F_FREAD, get_last_sys_error());
            BIOerr(BIO_F_FILE_READ, ERR_R_SYS_LIB);
            ret = -1;
        }
    }
    return (ret);
}

static int MS_CALLBACK file_write(BIO *b, const char *in, int inl)
{
    int ret = 0;

    if (b->init && (in != NULL)) {
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = UP_fwrite(in, (int)inl, 1, b->ptr);
        else
            ret = fwrite(in, (int)inl, 1, (FILE *)b->ptr);
        if (ret)
            ret = inl;
        /* ret=fwrite(in,1,(int)inl,(FILE *)b->ptr); */
        /*
         * according to Tim Hudson <tjh@cryptsoft.com>, the commented out
         * version above can cause 'inl' write calls under some stupid stdio
         * implementations (VMS)
         */
    }
    return (ret);
}

static long MS_CALLBACK file_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    long ret = 1;
    FILE *fp = (FILE *)b->ptr;
    FILE **fpp;
    char p[4];

    switch (cmd) {
    case BIO_C_FILE_SEEK:
    case BIO_CTRL_RESET:
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = (long)UP_fseek(b->ptr, num, 0);
        else
            ret = (long)fseek(fp, num, 0);
        break;
    case BIO_CTRL_EOF:
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = (long)UP_feof(fp);
        else
            ret = (long)feof(fp);
        break;
    case BIO_C_FILE_TELL:
    case BIO_CTRL_INFO:
        if (b->flags & BIO_FLAGS_UPLINK)
            ret = UP_ftell(b->ptr);
        else
            ret = ftell(fp);
        break;
    case BIO_C_SET_FILE_PTR:
        file_free(b);
        b->shutdown = (int)num & BIO_CLOSE;
        b->ptr = ptr;
        b->init = 1;
#  if BIO_FLAGS_UPLINK!=0
#   if defined(__MINGW32__) && defined(__MSVCRT__) && !defined(_IOB_ENTRIES)
#    define _IOB_ENTRIES 20
#   endif
#   if defined(_IOB_ENTRIES)
        /* Safety net to catch purely internal BIO_set_fp calls */
        if ((size_t)ptr >= (size_t)stdin &&
            (size_t)ptr < (size_t)(stdin + _IOB_ENTRIES))
            BIO_clear_flags(b, BIO_FLAGS_UPLINK);
#   endif
#  endif
#  ifdef UP_fsetmod
        if (b->flags & BIO_FLAGS_UPLINK)
            UP_fsetmod(b->ptr, (char)((num & BIO_FP_TEXT) ? 't' : 'b'));
        else
#  endif
        {
#  if defined(OPENSSL_SYS_WINDOWS)
            int fd = _fileno((FILE *)ptr);
            if (num & BIO_FP_TEXT)
                _setmode(fd, _O_TEXT);
            else
                _setmode(fd, _O_BINARY);
#  elif defined(OPENSSL_SYS_NETWARE) && defined(NETWARE_CLIB)
            int fd = fileno((FILE *)ptr);
            /* Under CLib there are differences in file modes */
            if (num & BIO_FP_TEXT)
                setmode(fd, O_TEXT);
            else
                setmode(fd, O_BINARY);
#  elif defined(OPENSSL_SYS_MSDOS)
            int fd = fileno((FILE *)ptr);
            /* Set correct text/binary mode */
            if (num & BIO_FP_TEXT)
                _setmode(fd, _O_TEXT);
            /* Dangerous to set stdin/stdout to raw (unless redirected) */
            else {
                if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
                    if (isatty(fd) <= 0)
                        _setmode(fd, _O_BINARY);
                } else
                    _setmode(fd, _O_BINARY);
            }
#  elif defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_WIN32_CYGWIN)
            int fd = fileno((FILE *)ptr);
            if (num & BIO_FP_TEXT)
                setmode(fd, O_TEXT);
            else
                setmode(fd, O_BINARY);
#  endif
        }
        break;
    case BIO_C_SET_FILENAME:
        file_free(b);
        b->shutdown = (int)num & BIO_CLOSE;
        if (num & BIO_FP_APPEND) {
            if (num & BIO_FP_READ)
                BUF_strlcpy(p, "a+", sizeof p);
            else
                BUF_strlcpy(p, "a", sizeof p);
        } else if ((num & BIO_FP_READ) && (num & BIO_FP_WRITE))
            BUF_strlcpy(p, "r+", sizeof p);
        else if (num & BIO_FP_WRITE)
            BUF_strlcpy(p, "w", sizeof p);
        else if (num & BIO_FP_READ)
            BUF_strlcpy(p, "r", sizeof p);
        else {
            BIOerr(BIO_F_FILE_CTRL, BIO_R_BAD_FOPEN_MODE);
            ret = 0;
            break;
        }
#  if defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_OS2) || defined(OPENSSL_SYS_WIN32_CYGWIN)
        if (!(num & BIO_FP_TEXT))
            strcat(p, "b");
        else
            strcat(p, "t");
#  endif
#  if defined(OPENSSL_SYS_NETWARE)
        if (!(num & BIO_FP_TEXT))
            strcat(p, "b");
        else
            strcat(p, "t");
#  endif
        fp = fopen(ptr, p);
        if (fp == NULL) {
            SYSerr(SYS_F_FOPEN, get_last_sys_error());
            ERR_add_error_data(5, "fopen('", ptr, "','", p, "')");
            BIOerr(BIO_F_FILE_CTRL, ERR_R_SYS_LIB);
            ret = 0;
            break;
        }
        b->ptr = fp;
        b->init = 1;
        BIO_clear_flags(b, BIO_FLAGS_UPLINK); /* we did fopen -> we disengage
                                               * UPLINK */
        break;
    case BIO_C_GET_FILE_PTR:
        /* the ptr parameter is actually a FILE ** in this case. */
        if (ptr != NULL) {
            fpp = (FILE **)ptr;
            *fpp = (FILE *)b->ptr;
        }
        break;
    case BIO_CTRL_GET_CLOSE:
        ret = (long)b->shutdown;
        break;
    case BIO_CTRL_SET_CLOSE:
        b->shutdown = (int)num;
        break;
    case BIO_CTRL_FLUSH:
        if (b->flags & BIO_FLAGS_UPLINK)
            UP_fflush(b->ptr);
        else
            fflush((FILE *)b->ptr);
        break;
    case BIO_CTRL_DUP:
        ret = 1;
        break;

    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
    default:
        ret = 0;
        break;
    }
    return (ret);
}

static int MS_CALLBACK file_gets(BIO *bp, char *buf, int size)
{
    int ret = 0;

    buf[0] = '\0';
    if (bp->flags & BIO_FLAGS_UPLINK) {
        if (!UP_fgets(buf, size, bp->ptr))
            goto err;
    } else {
        if (!fgets(buf, size, (FILE *)bp->ptr))
            goto err;
    }
    if (buf[0] != '\0')
        ret = strlen(buf);
 err:
    return (ret);
}

static int MS_CALLBACK file_puts(BIO *bp, const char *str)
{
    int n, ret;

    n = strlen(str);
    ret = file_write(bp, str, n);
    return (ret);
}

# endif                         /* OPENSSL_NO_STDIO */

#endif                          /* HEADER_BSS_FILE_C */
