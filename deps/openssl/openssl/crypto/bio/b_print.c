/* crypto/bio/b_print.c */
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

/* disable assert() unless BIO_DEBUG has been defined */
#ifndef BIO_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif

/*
 * Stolen from tjh's ssl/ssl_trc.c stuff.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "cryptlib.h"
#ifndef NO_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <openssl/bn.h>         /* To get BN_LLONG properly defined */
#include <openssl/bio.h>

#if defined(BN_LLONG) || defined(SIXTY_FOUR_BIT)
# ifndef HAVE_LONG_LONG
#  define HAVE_LONG_LONG 1
# endif
#endif

/***************************************************************************/

/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell <papowell@astart.com>
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions.
 */

/*-
 * This code contains numerious changes and enhancements which were
 * made by lots of contributors over the last years to Patrick Powell's
 * original code:
 *
 * o Patrick Powell <papowell@astart.com>      (1995)
 * o Brandon Long <blong@fiction.net>          (1996, for Mutt)
 * o Thomas Roessler <roessler@guug.de>        (1998, for Mutt)
 * o Michael Elkins <me@cs.hmc.edu>            (1998, for Mutt)
 * o Andrew Tridgell <tridge@samba.org>        (1998, for Samba)
 * o Luke Mewburn <lukem@netbsd.org>           (1999, for LukemFTP)
 * o Ralf S. Engelschall <rse@engelschall.com> (1999, for Pth)
 * o ...                                       (for OpenSSL)
 */

#ifdef HAVE_LONG_DOUBLE
# define LDOUBLE long double
#else
# define LDOUBLE double
#endif

#ifdef HAVE_LONG_LONG
# if defined(_WIN32) && !defined(__GNUC__)
#  define LLONG __int64
# else
#  define LLONG long long
# endif
#else
# define LLONG long
#endif

static void fmtstr(char **, char **, size_t *, size_t *,
                   const char *, int, int, int);
static void fmtint(char **, char **, size_t *, size_t *,
                   LLONG, int, int, int, int);
static void fmtfp(char **, char **, size_t *, size_t *,
                  LDOUBLE, int, int, int);
static void doapr_outch(char **, char **, size_t *, size_t *, int);
static void _dopr(char **sbuffer, char **buffer,
                  size_t *maxlen, size_t *retlen, int *truncated,
                  const char *format, va_list args);

/* format read states */
#define DP_S_DEFAULT    0
#define DP_S_FLAGS      1
#define DP_S_MIN        2
#define DP_S_DOT        3
#define DP_S_MAX        4
#define DP_S_MOD        5
#define DP_S_CONV       6
#define DP_S_DONE       7

/* format flags - Bits */
#define DP_F_MINUS      (1 << 0)
#define DP_F_PLUS       (1 << 1)
#define DP_F_SPACE      (1 << 2)
#define DP_F_NUM        (1 << 3)
#define DP_F_ZERO       (1 << 4)
#define DP_F_UP         (1 << 5)
#define DP_F_UNSIGNED   (1 << 6)

/* conversion flags */
#define DP_C_SHORT      1
#define DP_C_LONG       2
#define DP_C_LDOUBLE    3
#define DP_C_LLONG      4

/* some handy macros */
#define char_to_int(p) (p - '0')
#define OSSL_MAX(p,q) ((p >= q) ? p : q)

static void
_dopr(char **sbuffer,
      char **buffer,
      size_t *maxlen,
      size_t *retlen, int *truncated, const char *format, va_list args)
{
    char ch;
    LLONG value;
    LDOUBLE fvalue;
    char *strvalue;
    int min;
    int max;
    int state;
    int flags;
    int cflags;
    size_t currlen;

    state = DP_S_DEFAULT;
    flags = currlen = cflags = min = 0;
    max = -1;
    ch = *format++;

    while (state != DP_S_DONE) {
        if (ch == '\0' || (buffer == NULL && currlen >= *maxlen))
            state = DP_S_DONE;

        switch (state) {
        case DP_S_DEFAULT:
            if (ch == '%')
                state = DP_S_FLAGS;
            else
                doapr_outch(sbuffer, buffer, &currlen, maxlen, ch);
            ch = *format++;
            break;
        case DP_S_FLAGS:
            switch (ch) {
            case '-':
                flags |= DP_F_MINUS;
                ch = *format++;
                break;
            case '+':
                flags |= DP_F_PLUS;
                ch = *format++;
                break;
            case ' ':
                flags |= DP_F_SPACE;
                ch = *format++;
                break;
            case '#':
                flags |= DP_F_NUM;
                ch = *format++;
                break;
            case '0':
                flags |= DP_F_ZERO;
                ch = *format++;
                break;
            default:
                state = DP_S_MIN;
                break;
            }
            break;
        case DP_S_MIN:
            if (isdigit((unsigned char)ch)) {
                min = 10 * min + char_to_int(ch);
                ch = *format++;
            } else if (ch == '*') {
                min = va_arg(args, int);
                ch = *format++;
                state = DP_S_DOT;
            } else
                state = DP_S_DOT;
            break;
        case DP_S_DOT:
            if (ch == '.') {
                state = DP_S_MAX;
                ch = *format++;
            } else
                state = DP_S_MOD;
            break;
        case DP_S_MAX:
            if (isdigit((unsigned char)ch)) {
                if (max < 0)
                    max = 0;
                max = 10 * max + char_to_int(ch);
                ch = *format++;
            } else if (ch == '*') {
                max = va_arg(args, int);
                ch = *format++;
                state = DP_S_MOD;
            } else
                state = DP_S_MOD;
            break;
        case DP_S_MOD:
            switch (ch) {
            case 'h':
                cflags = DP_C_SHORT;
                ch = *format++;
                break;
            case 'l':
                if (*format == 'l') {
                    cflags = DP_C_LLONG;
                    format++;
                } else
                    cflags = DP_C_LONG;
                ch = *format++;
                break;
            case 'q':
                cflags = DP_C_LLONG;
                ch = *format++;
                break;
            case 'L':
                cflags = DP_C_LDOUBLE;
                ch = *format++;
                break;
            default:
                break;
            }
            state = DP_S_CONV;
            break;
        case DP_S_CONV:
            switch (ch) {
            case 'd':
            case 'i':
                switch (cflags) {
                case DP_C_SHORT:
                    value = (short int)va_arg(args, int);
                    break;
                case DP_C_LONG:
                    value = va_arg(args, long int);
                    break;
                case DP_C_LLONG:
                    value = va_arg(args, LLONG);
                    break;
                default:
                    value = va_arg(args, int);
                    break;
                }
                fmtint(sbuffer, buffer, &currlen, maxlen,
                       value, 10, min, max, flags);
                break;
            case 'X':
                flags |= DP_F_UP;
                /* FALLTHROUGH */
            case 'x':
            case 'o':
            case 'u':
                flags |= DP_F_UNSIGNED;
                switch (cflags) {
                case DP_C_SHORT:
                    value = (unsigned short int)va_arg(args, unsigned int);
                    break;
                case DP_C_LONG:
                    value = (LLONG) va_arg(args, unsigned long int);
                    break;
                case DP_C_LLONG:
                    value = va_arg(args, unsigned LLONG);
                    break;
                default:
                    value = (LLONG) va_arg(args, unsigned int);
                    break;
                }
                fmtint(sbuffer, buffer, &currlen, maxlen, value,
                       ch == 'o' ? 8 : (ch == 'u' ? 10 : 16),
                       min, max, flags);
                break;
            case 'f':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                fmtfp(sbuffer, buffer, &currlen, maxlen,
                      fvalue, min, max, flags);
                break;
            case 'E':
                flags |= DP_F_UP;
            case 'e':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                break;
            case 'G':
                flags |= DP_F_UP;
            case 'g':
                if (cflags == DP_C_LDOUBLE)
                    fvalue = va_arg(args, LDOUBLE);
                else
                    fvalue = va_arg(args, double);
                break;
            case 'c':
                doapr_outch(sbuffer, buffer, &currlen, maxlen,
                            va_arg(args, int));
                break;
            case 's':
                strvalue = va_arg(args, char *);
                if (max < 0) {
                    if (buffer)
                        max = INT_MAX;
                    else
                        max = *maxlen;
                }
                fmtstr(sbuffer, buffer, &currlen, maxlen, strvalue,
                       flags, min, max);
                break;
            case 'p':
                value = (long)va_arg(args, void *);
                fmtint(sbuffer, buffer, &currlen, maxlen,
                       value, 16, min, max, flags | DP_F_NUM);
                break;
            case 'n':          /* XXX */
                if (cflags == DP_C_SHORT) {
                    short int *num;
                    num = va_arg(args, short int *);
                    *num = currlen;
                } else if (cflags == DP_C_LONG) { /* XXX */
                    long int *num;
                    num = va_arg(args, long int *);
                    *num = (long int)currlen;
                } else if (cflags == DP_C_LLONG) { /* XXX */
                    LLONG *num;
                    num = va_arg(args, LLONG *);
                    *num = (LLONG) currlen;
                } else {
                    int *num;
                    num = va_arg(args, int *);
                    *num = currlen;
                }
                break;
            case '%':
                doapr_outch(sbuffer, buffer, &currlen, maxlen, ch);
                break;
            case 'w':
                /* not supported yet, treat as next char */
                ch = *format++;
                break;
            default:
                /* unknown, skip */
                break;
            }
            ch = *format++;
            state = DP_S_DEFAULT;
            flags = cflags = min = 0;
            max = -1;
            break;
        case DP_S_DONE:
            break;
        default:
            break;
        }
    }
    *truncated = (currlen > *maxlen - 1);
    if (*truncated)
        currlen = *maxlen - 1;
    doapr_outch(sbuffer, buffer, &currlen, maxlen, '\0');
    *retlen = currlen - 1;
    return;
}

static void
fmtstr(char **sbuffer,
       char **buffer,
       size_t *currlen,
       size_t *maxlen, const char *value, int flags, int min, int max)
{
    int padlen, strln;
    int cnt = 0;

    if (value == 0)
        value = "<NULL>";
    for (strln = 0; value[strln]; ++strln) ;
    padlen = min - strln;
    if (padlen < 0)
        padlen = 0;
    if (flags & DP_F_MINUS)
        padlen = -padlen;

    while ((padlen > 0) && (cnt < max)) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        --padlen;
        ++cnt;
    }
    while (*value && (cnt < max)) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, *value++);
        ++cnt;
    }
    while ((padlen < 0) && (cnt < max)) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        ++padlen;
        ++cnt;
    }
}

static void
fmtint(char **sbuffer,
       char **buffer,
       size_t *currlen,
       size_t *maxlen, LLONG value, int base, int min, int max, int flags)
{
    int signvalue = 0;
    const char *prefix = "";
    unsigned LLONG uvalue;
    char convert[DECIMAL_SIZE(value) + 3];
    int place = 0;
    int spadlen = 0;
    int zpadlen = 0;
    int caps = 0;

    if (max < 0)
        max = 0;
    uvalue = value;
    if (!(flags & DP_F_UNSIGNED)) {
        if (value < 0) {
            signvalue = '-';
            uvalue = -value;
        } else if (flags & DP_F_PLUS)
            signvalue = '+';
        else if (flags & DP_F_SPACE)
            signvalue = ' ';
    }
    if (flags & DP_F_NUM) {
        if (base == 8)
            prefix = "0";
        if (base == 16)
            prefix = "0x";
    }
    if (flags & DP_F_UP)
        caps = 1;
    do {
        convert[place++] = (caps ? "0123456789ABCDEF" : "0123456789abcdef")
            [uvalue % (unsigned)base];
        uvalue = (uvalue / (unsigned)base);
    } while (uvalue && (place < (int)sizeof(convert)));
    if (place == sizeof(convert))
        place--;
    convert[place] = 0;

    zpadlen = max - place;
    spadlen =
        min - OSSL_MAX(max, place) - (signvalue ? 1 : 0) - strlen(prefix);
    if (zpadlen < 0)
        zpadlen = 0;
    if (spadlen < 0)
        spadlen = 0;
    if (flags & DP_F_ZERO) {
        zpadlen = OSSL_MAX(zpadlen, spadlen);
        spadlen = 0;
    }
    if (flags & DP_F_MINUS)
        spadlen = -spadlen;

    /* spaces */
    while (spadlen > 0) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        --spadlen;
    }

    /* sign */
    if (signvalue)
        doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);

    /* prefix */
    while (*prefix) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, *prefix);
        prefix++;
    }

    /* zeros */
    if (zpadlen > 0) {
        while (zpadlen > 0) {
            doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
            --zpadlen;
        }
    }
    /* digits */
    while (place > 0)
        doapr_outch(sbuffer, buffer, currlen, maxlen, convert[--place]);

    /* left justified spaces */
    while (spadlen < 0) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        ++spadlen;
    }
    return;
}

static LDOUBLE abs_val(LDOUBLE value)
{
    LDOUBLE result = value;
    if (value < 0)
        result = -value;
    return result;
}

static LDOUBLE pow_10(int in_exp)
{
    LDOUBLE result = 1;
    while (in_exp) {
        result *= 10;
        in_exp--;
    }
    return result;
}

static long roundv(LDOUBLE value)
{
    long intpart;
    intpart = (long)value;
    value = value - intpart;
    if (value >= 0.5)
        intpart++;
    return intpart;
}

static void
fmtfp(char **sbuffer,
      char **buffer,
      size_t *currlen,
      size_t *maxlen, LDOUBLE fvalue, int min, int max, int flags)
{
    int signvalue = 0;
    LDOUBLE ufvalue;
    char iconvert[20];
    char fconvert[20];
    int iplace = 0;
    int fplace = 0;
    int padlen = 0;
    int zpadlen = 0;
    long intpart;
    long fracpart;
    long max10;

    if (max < 0)
        max = 6;
    ufvalue = abs_val(fvalue);
    if (fvalue < 0)
        signvalue = '-';
    else if (flags & DP_F_PLUS)
        signvalue = '+';
    else if (flags & DP_F_SPACE)
        signvalue = ' ';

    intpart = (long)ufvalue;

    /*
     * sorry, we only support 9 digits past the decimal because of our
     * conversion method
     */
    if (max > 9)
        max = 9;

    /*
     * we "cheat" by converting the fractional part to integer by multiplying
     * by a factor of 10
     */
    max10 = roundv(pow_10(max));
    fracpart = roundv(pow_10(max) * (ufvalue - intpart));

    if (fracpart >= max10) {
        intpart++;
        fracpart -= max10;
    }

    /* convert integer part */
    do {
        iconvert[iplace++] = "0123456789"[intpart % 10];
        intpart = (intpart / 10);
    } while (intpart && (iplace < (int)sizeof(iconvert)));
    if (iplace == sizeof iconvert)
        iplace--;
    iconvert[iplace] = 0;

    /* convert fractional part */
    do {
        fconvert[fplace++] = "0123456789"[fracpart % 10];
        fracpart = (fracpart / 10);
    } while (fplace < max);
    if (fplace == sizeof fconvert)
        fplace--;
    fconvert[fplace] = 0;

    /* -1 for decimal point, another -1 if we are printing a sign */
    padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
    zpadlen = max - fplace;
    if (zpadlen < 0)
        zpadlen = 0;
    if (padlen < 0)
        padlen = 0;
    if (flags & DP_F_MINUS)
        padlen = -padlen;

    if ((flags & DP_F_ZERO) && (padlen > 0)) {
        if (signvalue) {
            doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);
            --padlen;
            signvalue = 0;
        }
        while (padlen > 0) {
            doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
            --padlen;
        }
    }
    while (padlen > 0) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        --padlen;
    }
    if (signvalue)
        doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);

    while (iplace > 0)
        doapr_outch(sbuffer, buffer, currlen, maxlen, iconvert[--iplace]);

    /*
     * Decimal point. This should probably use locale to find the correct
     * char to print out.
     */
    if (max > 0 || (flags & DP_F_NUM)) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, '.');

        while (fplace > 0)
            doapr_outch(sbuffer, buffer, currlen, maxlen, fconvert[--fplace]);
    }
    while (zpadlen > 0) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
        --zpadlen;
    }

    while (padlen < 0) {
        doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
        ++padlen;
    }
}

static void
doapr_outch(char **sbuffer,
            char **buffer, size_t *currlen, size_t *maxlen, int c)
{
    /* If we haven't at least one buffer, someone has doe a big booboo */
    assert(*sbuffer != NULL || buffer != NULL);

    if (buffer) {
        while (*currlen >= *maxlen) {
            if (*buffer == NULL) {
                if (*maxlen == 0)
                    *maxlen = 1024;
                *buffer = OPENSSL_malloc(*maxlen);
                if(!*buffer) {
                    /* Panic! Can't really do anything sensible. Just return */
                    return;
                }
                if (*currlen > 0) {
                    assert(*sbuffer != NULL);
                    memcpy(*buffer, *sbuffer, *currlen);
                }
                *sbuffer = NULL;
            } else {
                *maxlen += 1024;
                *buffer = OPENSSL_realloc(*buffer, *maxlen);
                if(!*buffer) {
                    /* Panic! Can't really do anything sensible. Just return */
                    return;
                }
            }
        }
        /* What to do if *buffer is NULL? */
        assert(*sbuffer != NULL || *buffer != NULL);
    }

    if (*currlen < *maxlen) {
        if (*sbuffer)
            (*sbuffer)[(*currlen)++] = (char)c;
        else
            (*buffer)[(*currlen)++] = (char)c;
    }

    return;
}

/***************************************************************************/

int BIO_printf(BIO *bio, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);

    ret = BIO_vprintf(bio, format, args);

    va_end(args);
    return (ret);
}

int BIO_vprintf(BIO *bio, const char *format, va_list args)
{
    int ret;
    size_t retlen;
    char hugebuf[1024 * 2];     /* Was previously 10k, which is unreasonable
                                 * in small-stack environments, like threads
                                 * or DOS programs. */
    char *hugebufp = hugebuf;
    size_t hugebufsize = sizeof(hugebuf);
    char *dynbuf = NULL;
    int ignored;

    dynbuf = NULL;
    CRYPTO_push_info("doapr()");
    _dopr(&hugebufp, &dynbuf, &hugebufsize, &retlen, &ignored, format, args);
    if (dynbuf) {
        ret = BIO_write(bio, dynbuf, (int)retlen);
        OPENSSL_free(dynbuf);
    } else {
        ret = BIO_write(bio, hugebuf, (int)retlen);
    }
    CRYPTO_pop_info();
    return (ret);
}

/*
 * As snprintf is not available everywhere, we provide our own
 * implementation. This function has nothing to do with BIOs, but it's
 * closely related to BIO_printf, and we need *some* name prefix ... (XXX the
 * function should be renamed, but to what?)
 */
int BIO_snprintf(char *buf, size_t n, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);

    ret = BIO_vsnprintf(buf, n, format, args);

    va_end(args);
    return (ret);
}

int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
    size_t retlen;
    int truncated;

    _dopr(&buf, NULL, &n, &retlen, &truncated, format, args);

    if (truncated)
        /*
         * In case of truncation, return -1 like traditional snprintf.
         * (Current drafts for ISO/IEC 9899 say snprintf should return the
         * number of characters that would have been written, had the buffer
         * been large enough.)
         */
        return -1;
    else
        return (retlen <= INT_MAX) ? (int)retlen : -1;
}
