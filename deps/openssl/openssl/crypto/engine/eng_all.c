/* crypto/engine/eng_all.c -*- mode: C; c-file-style: "eay" -*- */
/*
 * Written by Richard Levitte <richard@levitte.org> for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 2000-2001 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#include "cryptlib.h"
#include "eng_int.h"

void ENGINE_load_builtin_engines(void)
{
    /* Some ENGINEs need this */
    OPENSSL_cpuid_setup();
#if 0
    /*
     * There's no longer any need for an "openssl" ENGINE unless, one day, it
     * is the *only* way for standard builtin implementations to be be
     * accessed (ie. it would be possible to statically link binaries with
     * *no* builtin implementations).
     */
    ENGINE_load_openssl();
#endif
#if !defined(OPENSSL_NO_HW) && (defined(__OpenBSD__) || defined(__FreeBSD__) || defined(HAVE_CRYPTODEV))
    ENGINE_load_cryptodev();
#endif
#ifndef OPENSSL_NO_RSAX
    ENGINE_load_rsax();
#endif
#ifndef OPENSSL_NO_RDRAND
    ENGINE_load_rdrand();
#endif
    ENGINE_load_dynamic();
#ifndef OPENSSL_NO_STATIC_ENGINE
# ifndef OPENSSL_NO_HW
#  ifndef OPENSSL_NO_HW_4758_CCA
    ENGINE_load_4758cca();
#  endif
#  ifndef OPENSSL_NO_HW_AEP
    ENGINE_load_aep();
#  endif
#  ifndef OPENSSL_NO_HW_ATALLA
    ENGINE_load_atalla();
#  endif
#  ifndef OPENSSL_NO_HW_CSWIFT
    ENGINE_load_cswift();
#  endif
#  ifndef OPENSSL_NO_HW_NCIPHER
    ENGINE_load_chil();
#  endif
#  ifndef OPENSSL_NO_HW_NURON
    ENGINE_load_nuron();
#  endif
#  ifndef OPENSSL_NO_HW_SUREWARE
    ENGINE_load_sureware();
#  endif
#  ifndef OPENSSL_NO_HW_UBSEC
    ENGINE_load_ubsec();
#  endif
#  ifndef OPENSSL_NO_HW_PADLOCK
    ENGINE_load_padlock();
#  endif
# endif
# ifndef OPENSSL_NO_GOST
    ENGINE_load_gost();
# endif
# ifndef OPENSSL_NO_GMP
    ENGINE_load_gmp();
# endif
# if defined(OPENSSL_SYS_WIN32) && !defined(OPENSSL_NO_CAPIENG)
    ENGINE_load_capi();
# endif
#endif
    ENGINE_register_all_complete();
}

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(HAVE_CRYPTODEV)
void ENGINE_setup_bsd_cryptodev(void)
{
    static int bsd_cryptodev_default_loaded = 0;
    if (!bsd_cryptodev_default_loaded) {
        ENGINE_load_cryptodev();
        ENGINE_register_all_complete();
    }
    bsd_cryptodev_default_loaded = 1;
}
#endif
