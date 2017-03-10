/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "internal/cryptlib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/buffer.h>

#ifdef OPENSSL_SYS_VMS
# include <unixio.h>
#endif
#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifndef OPENSSL_NO_POSIX_IO
# include <sys/stat.h>
# include <fcntl.h>
/*
 * Following should not be needed, and we could have been stricter
 * and demand S_IS*. But some systems just don't comply... Formally
 * below macros are "anatomically incorrect", because normally they
 * would look like ((m) & MASK == TYPE), but since MASK availability
 * is as questionable, we settle for this poor-man fallback...
 */
# if !defined(S_ISBLK)
#  if defined(_S_IFBLK)
#   define S_ISBLK(m) ((m) & _S_IFBLK)
#  elif defined(S_IFBLK)
#   define S_ISBLK(m) ((m) & S_IFBLK)
#  elif defined(_WIN32)
#   define S_ISBLK(m) 0 /* no concept of block devices on Windows */
#  endif
# endif
# if !defined(S_ISCHR)
#  if defined(_S_IFCHR)
#   define S_ISCHR(m) ((m) & _S_IFCHR)
#  elif defined(S_IFCHR)
#   define S_ISCHR(m) ((m) & S_IFCHR)
#  endif
# endif
#endif

#ifdef _WIN32
# define stat    _stat
# define chmod   _chmod
# define open    _open
# define fdopen  _fdopen
# define fstat   _fstat
# define fileno  _fileno
#endif

#undef BUFSIZE
#define BUFSIZE 1024
#define RAND_DATA 1024

#ifdef OPENSSL_SYS_VMS
/*
 * Misc hacks needed for specific cases.
 *
 * __FILE_ptr32 is a type provided by DEC C headers (types.h specifically)
 * to make sure the FILE* is a 32-bit pointer no matter what.  We know that
 * stdio function return this type (a study of stdio.h proves it).
 * Additionally, we create a similar char pointer type for the sake of
 * vms_setbuf below.
 */
# if __INITIAL_POINTER_SIZE == 64
#  pragma pointer_size save
#  pragma pointer_size 32
typedef char *char_ptr32;
#  pragma pointer_size restore
/*
 * On VMS, setbuf() will only take 32-bit pointers, and a compilation
 * with /POINTER_SIZE=64 will give off a MAYLOSEDATA2 warning here.
 * Since we know that the FILE* really is a 32-bit pointer expanded to
 * 64 bits, we also know it's safe to convert it back to a 32-bit pointer.
 * As for the buffer parameter, we only use NULL here, so that passes as
 * well...
 */
#  define setbuf(fp,buf) (setbuf)((__FILE_ptr32)(fp), (char_ptr32)(buf))
# endif

/*
 * This declaration is a nasty hack to get around vms' extension to fopen for
 * passing in sharing options being disabled by /STANDARD=ANSI89
 */
static __FILE_ptr32 (*const vms_fopen)(const char *, const char *, ...) =
      (__FILE_ptr32 (*)(const char *, const char *, ...))fopen;
# define VMS_OPEN_ATTRS "shr=get,put,upd,del","ctx=bin,stm","rfm=stm","rat=none","mrs=0"

# define openssl_fopen(fname,mode) vms_fopen((fname), (mode), VMS_OPEN_ATTRS)
#endif

#define RFILE ".rnd"

/*
 * Note that these functions are intended for seed files only. Entropy
 * devices and EGD sockets are handled in rand_unix.c
 */

int RAND_load_file(const char *file, long bytes)
{
    /*-
     * If bytes >= 0, read up to 'bytes' bytes.
     * if bytes == -1, read complete file.
     */

    unsigned char buf[BUFSIZE];
#ifndef OPENSSL_NO_POSIX_IO
    struct stat sb;
#endif
    int i, ret = 0, n;
    FILE *in = NULL;

    if (file == NULL)
        return 0;

    if (bytes == 0)
        return ret;

    in = openssl_fopen(file, "rb");
    if (in == NULL)
        goto err;

#ifndef OPENSSL_NO_POSIX_IO
    /*
     * struct stat can have padding and unused fields that may not be
     * initialized in the call to stat(). We need to clear the entire
     * structure before calling RAND_add() to avoid complaints from
     * applications such as Valgrind.
     */
    memset(&sb, 0, sizeof(sb));
    if (fstat(fileno(in), &sb) < 0)
        goto err;
    RAND_add(&sb, sizeof(sb), 0.0);

# if defined(S_ISBLK) && defined(S_ISCHR)
    if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
        /*
         * this file is a device. we don't want read an infinite number of
         * bytes from a random device, nor do we want to use buffered I/O
         * because we will waste system entropy.
         */
        bytes = (bytes == -1) ? 2048 : bytes; /* ok, is 2048 enough? */
        setbuf(in, NULL); /* don't do buffered reads */
    }
# endif
#endif
    for (;;) {
        if (bytes > 0)
            n = (bytes < BUFSIZE) ? (int)bytes : BUFSIZE;
        else
            n = BUFSIZE;
        i = fread(buf, 1, n, in);
        if (i <= 0)
            break;

        RAND_add(buf, i, (double)i);
        ret += i;
        if (bytes > 0) {
            bytes -= n;
            if (bytes <= 0)
                break;
        }
    }
    OPENSSL_cleanse(buf, BUFSIZE);
 err:
    if (in != NULL)
        fclose(in);
    return ret;
}

int RAND_write_file(const char *file)
{
    unsigned char buf[BUFSIZE];
    int i, ret = 0, rand_err = 0;
    FILE *out = NULL;
    int n;
#ifndef OPENSSL_NO_POSIX_IO
    struct stat sb;

# if defined(S_ISBLK) && defined(S_ISCHR)
# ifdef _WIN32
    /*
     * Check for |file| being a driver as "ASCII-safe" on Windows,
     * because driver paths are always ASCII.
     */
# endif
    i = stat(file, &sb);
    if (i != -1) {
        if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
            /*
             * this file is a device. we don't write back to it. we
             * "succeed" on the assumption this is some sort of random
             * device. Otherwise attempting to write to and chmod the device
             * causes problems.
             */
            return 1;
        }
    }
# endif
#endif

#if defined(O_CREAT) && !defined(OPENSSL_NO_POSIX_IO) && \
    !defined(OPENSSL_SYS_VMS) && !defined(OPENSSL_SYS_WINDOWS)
    {
# ifndef O_BINARY
#  define O_BINARY 0
# endif
        /*
         * chmod(..., 0600) is too late to protect the file, permissions
         * should be restrictive from the start
         */
        int fd = open(file, O_WRONLY | O_CREAT | O_BINARY, 0600);
        if (fd != -1)
            out = fdopen(fd, "wb");
    }
#endif

#ifdef OPENSSL_SYS_VMS
    /*
     * VMS NOTE: Prior versions of this routine created a _new_ version of
     * the rand file for each call into this routine, then deleted all
     * existing versions named ;-1, and finally renamed the current version
     * as ';1'. Under concurrent usage, this resulted in an RMS race
     * condition in rename() which could orphan files (see vms message help
     * for RMS$_REENT). With the fopen() calls below, openssl/VMS now shares
     * the top-level version of the rand file. Note that there may still be
     * conditions where the top-level rand file is locked. If so, this code
     * will then create a new version of the rand file. Without the delete
     * and rename code, this can result in ascending file versions that stop
     * at version 32767, and this routine will then return an error. The
     * remedy for this is to recode the calling application to avoid
     * concurrent use of the rand file, or synchronize usage at the
     * application level. Also consider whether or not you NEED a persistent
     * rand file in a concurrent use situation.
     */

    out = openssl_fopen(file, "rb+");
#endif
    if (out == NULL)
        out = openssl_fopen(file, "wb");
    if (out == NULL)
        goto err;

#if !defined(NO_CHMOD) && !defined(OPENSSL_NO_POSIX_IO)
    chmod(file, 0600);
#endif
    n = RAND_DATA;
    for (;;) {
        i = (n > BUFSIZE) ? BUFSIZE : n;
        n -= BUFSIZE;
        if (RAND_bytes(buf, i) <= 0)
            rand_err = 1;
        i = fwrite(buf, 1, i, out);
        if (i <= 0) {
            ret = 0;
            break;
        }
        ret += i;
        if (n <= 0)
            break;
    }

    fclose(out);
    OPENSSL_cleanse(buf, BUFSIZE);
 err:
    return (rand_err ? -1 : ret);
}

const char *RAND_file_name(char *buf, size_t size)
{
    char *s = NULL;
    int use_randfile = 1;
#ifdef __OpenBSD__
    struct stat sb;
#endif

#if defined(_WIN32) && defined(CP_UTF8)
    DWORD len;
    WCHAR *var, *val;

    if ((var = L"RANDFILE",
         len = GetEnvironmentVariableW(var, NULL, 0)) == 0
        && (var = L"HOME", use_randfile = 0,
            len = GetEnvironmentVariableW(var, NULL, 0)) == 0
        && (var = L"USERPROFILE",
            len = GetEnvironmentVariableW(var, NULL, 0)) == 0) {
        var = L"SYSTEMROOT",
        len = GetEnvironmentVariableW(var, NULL, 0);
    }

    if (len != 0) {
        int sz;

        val = _alloca(len * sizeof(WCHAR));

        if (GetEnvironmentVariableW(var, val, len) < len
            && (sz = WideCharToMultiByte(CP_UTF8, 0, val, -1, NULL, 0,
                                         NULL, NULL)) != 0) {
            s = _alloca(sz);
            if (WideCharToMultiByte(CP_UTF8, 0, val, -1, s, sz,
                                    NULL, NULL) == 0)
                s = NULL;
        }
    }
#else
    if (OPENSSL_issetugid() != 0) {
        use_randfile = 0;
    } else {
        s = getenv("RANDFILE");
        if (s == NULL || *s == '\0') {
            use_randfile = 0;
            s = getenv("HOME");
        }
    }
#endif
#ifdef DEFAULT_HOME
    if (!use_randfile && s == NULL) {
        s = DEFAULT_HOME;
    }
#endif
    if (s != NULL && *s) {
        size_t len = strlen(s);

        if (use_randfile && len + 1 < size) {
            if (OPENSSL_strlcpy(buf, s, size) >= size)
                return NULL;
        } else if (len + strlen(RFILE) + 2 < size) {
            OPENSSL_strlcpy(buf, s, size);
#ifndef OPENSSL_SYS_VMS
            OPENSSL_strlcat(buf, "/", size);
#endif
            OPENSSL_strlcat(buf, RFILE, size);
        }
    } else {
        buf[0] = '\0';      /* no file name */
    }

#ifdef __OpenBSD__
    /*
     * given that all random loads just fail if the file can't be seen on a
     * stat, we stat the file we're returning, if it fails, use /dev/arandom
     * instead. this allows the user to use their own source for good random
     * data, but defaults to something hopefully decent if that isn't
     * available.
     */

    if (!buf[0] || stat(buf, &sb) == -1)
        if (OPENSSL_strlcpy(buf, "/dev/arandom", size) >= size) {
            return NULL;
        }
#endif
    return buf[0] ? buf : NULL;
}
