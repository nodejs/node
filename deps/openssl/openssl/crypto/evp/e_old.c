/* crypto/evp/e_old.c -*- mode:C; c-file-style: "eay" -*- */
/* Written by Richard Levitte (richard@levitte.org) for the OpenSSL
 * project 2004.
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved.
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

#ifdef OPENSSL_NO_DEPRECATED
static void *dummy = &dummy;
#else

#include <openssl/evp.h>

/* Define some deprecated functions, so older programs
   don't crash and burn too quickly.  On Windows and VMS,
   these will never be used, since functions and variables
   in shared libraries are selected by entry point location,
   not by name.  */

#ifndef OPENSSL_NO_BF
#undef EVP_bf_cfb
const EVP_CIPHER *EVP_bf_cfb(void);
const EVP_CIPHER *EVP_bf_cfb(void) { return EVP_bf_cfb64(); }
#endif

#ifndef OPENSSL_NO_DES
#undef EVP_des_cfb
const EVP_CIPHER *EVP_des_cfb(void);
const EVP_CIPHER *EVP_des_cfb(void) { return EVP_des_cfb64(); }
#undef EVP_des_ede3_cfb
const EVP_CIPHER *EVP_des_ede3_cfb(void);
const EVP_CIPHER *EVP_des_ede3_cfb(void) { return EVP_des_ede3_cfb64(); }
#undef EVP_des_ede_cfb
const EVP_CIPHER *EVP_des_ede_cfb(void);
const EVP_CIPHER *EVP_des_ede_cfb(void) { return EVP_des_ede_cfb64(); }
#endif

#ifndef OPENSSL_NO_IDEA
#undef EVP_idea_cfb
const EVP_CIPHER *EVP_idea_cfb(void);
const EVP_CIPHER *EVP_idea_cfb(void) { return EVP_idea_cfb64(); }
#endif

#ifndef OPENSSL_NO_RC2
#undef EVP_rc2_cfb
const EVP_CIPHER *EVP_rc2_cfb(void);
const EVP_CIPHER *EVP_rc2_cfb(void) { return EVP_rc2_cfb64(); }
#endif

#ifndef OPENSSL_NO_CAST
#undef EVP_cast5_cfb
const EVP_CIPHER *EVP_cast5_cfb(void);
const EVP_CIPHER *EVP_cast5_cfb(void) { return EVP_cast5_cfb64(); }
#endif

#ifndef OPENSSL_NO_RC5
#undef EVP_rc5_32_12_16_cfb
const EVP_CIPHER *EVP_rc5_32_12_16_cfb(void);
const EVP_CIPHER *EVP_rc5_32_12_16_cfb(void) { return EVP_rc5_32_12_16_cfb64(); }
#endif

#ifndef OPENSSL_NO_AES
#undef EVP_aes_128_cfb
const EVP_CIPHER *EVP_aes_128_cfb(void);
const EVP_CIPHER *EVP_aes_128_cfb(void) { return EVP_aes_128_cfb128(); }
#undef EVP_aes_192_cfb
const EVP_CIPHER *EVP_aes_192_cfb(void);
const EVP_CIPHER *EVP_aes_192_cfb(void) { return EVP_aes_192_cfb128(); }
#undef EVP_aes_256_cfb
const EVP_CIPHER *EVP_aes_256_cfb(void);
const EVP_CIPHER *EVP_aes_256_cfb(void) { return EVP_aes_256_cfb128(); }
#endif

#endif
