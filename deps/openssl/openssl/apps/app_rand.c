/* apps/app_rand.c */
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
/* ====================================================================
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#define NON_MAIN
#include "apps.h"
#undef NON_MAIN
#include <openssl/bio.h>
#include <openssl/rand.h>

static int seeded = 0;
static int egdsocket = 0;

int app_RAND_load_file(const char *file, BIO *bio_e, int dont_warn)
{
    int consider_randfile = (file == NULL);
    char buffer[200];

#ifdef OPENSSL_SYS_WINDOWS
    BIO_printf(bio_e, "Loading 'screen' into random state -");
    BIO_flush(bio_e);
    RAND_screen();
    BIO_printf(bio_e, " done\n");
#endif

    if (file == NULL)
        file = RAND_file_name(buffer, sizeof buffer);
    else if (RAND_egd(file) > 0) {
        /*
         * we try if the given filename is an EGD socket. if it is, we don't
         * write anything back to the file.
         */
        egdsocket = 1;
        return 1;
    }
    if (file == NULL || !RAND_load_file(file, -1)) {
        if (RAND_status() == 0) {
            if (!dont_warn) {
                BIO_printf(bio_e, "unable to load 'random state'\n");
                BIO_printf(bio_e,
                           "This means that the random number generator has not been seeded\n");
                BIO_printf(bio_e, "with much random data.\n");
                if (consider_randfile) { /* explanation does not apply when a
                                          * file is explicitly named */
                    BIO_printf(bio_e,
                               "Consider setting the RANDFILE environment variable to point at a file that\n");
                    BIO_printf(bio_e,
                               "'random' data can be kept in (the file will be overwritten).\n");
                }
            }
            return 0;
        }
    }
    seeded = 1;
    return 1;
}

long app_RAND_load_files(char *name)
{
    char *p, *n;
    int last;
    long tot = 0;
    int egd;

    for (;;) {
        last = 0;
        for (p = name; ((*p != '\0') && (*p != LIST_SEPARATOR_CHAR)); p++) ;
        if (*p == '\0')
            last = 1;
        *p = '\0';
        n = name;
        name = p + 1;
        if (*n == '\0')
            break;

        egd = RAND_egd(n);
        if (egd > 0)
            tot += egd;
        else
            tot += RAND_load_file(n, -1);
        if (last)
            break;
    }
    if (tot > 512)
        app_RAND_allow_write_file();
    return (tot);
}

int app_RAND_write_file(const char *file, BIO *bio_e)
{
    char buffer[200];

    if (egdsocket || !seeded)
        /*
         * If we did not manage to read the seed file, we should not write a
         * low-entropy seed file back -- it would suppress a crucial warning
         * the next time we want to use it.
         */
        return 0;

    if (file == NULL)
        file = RAND_file_name(buffer, sizeof buffer);
    if (file == NULL || !RAND_write_file(file)) {
        BIO_printf(bio_e, "unable to write 'random state'\n");
        return 0;
    }
    return 1;
}

void app_RAND_allow_write_file(void)
{
    seeded = 1;
}
