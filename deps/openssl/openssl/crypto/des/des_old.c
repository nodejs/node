/* crypto/des/des_old.c */

/*-
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * The function names in here are deprecated and are only present to
 * provide an interface compatible with libdes.  OpenSSL now provides
 * functions where "des_" has been replaced with "DES_" in the names,
 * to make it possible to make incompatible changes that are needed
 * for C type security and other stuff.
 *
 * Please consider starting to use the DES_ functions rather than the
 * des_ ones.  The des_ functions will dissapear completely before
 * OpenSSL 1.0!
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */

/*
 * Written by Richard Levitte (richard@levitte.org) for the OpenSSL project
 * 2001.
 */
/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
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

#define OPENSSL_DES_LIBDES_COMPATIBILITY
#include <openssl/des.h>
#include <openssl/rand.h>

const char *_ossl_old_des_options(void)
{
    return DES_options();
}

void _ossl_old_des_ecb3_encrypt(_ossl_old_des_cblock *input,
                                _ossl_old_des_cblock *output,
                                des_key_schedule ks1, des_key_schedule ks2,
                                des_key_schedule ks3, int enc)
{
    DES_ecb3_encrypt((const_DES_cblock *)input, output,
                     (DES_key_schedule *)ks1, (DES_key_schedule *)ks2,
                     (DES_key_schedule *)ks3, enc);
}

DES_LONG _ossl_old_des_cbc_cksum(_ossl_old_des_cblock *input,
                                 _ossl_old_des_cblock *output, long length,
                                 des_key_schedule schedule,
                                 _ossl_old_des_cblock *ivec)
{
    return DES_cbc_cksum((unsigned char *)input, output, length,
                         (DES_key_schedule *)schedule, ivec);
}

void _ossl_old_des_cbc_encrypt(_ossl_old_des_cblock *input,
                               _ossl_old_des_cblock *output, long length,
                               des_key_schedule schedule,
                               _ossl_old_des_cblock *ivec, int enc)
{
    DES_cbc_encrypt((unsigned char *)input, (unsigned char *)output,
                    length, (DES_key_schedule *)schedule, ivec, enc);
}

void _ossl_old_des_ncbc_encrypt(_ossl_old_des_cblock *input,
                                _ossl_old_des_cblock *output, long length,
                                des_key_schedule schedule,
                                _ossl_old_des_cblock *ivec, int enc)
{
    DES_ncbc_encrypt((unsigned char *)input, (unsigned char *)output,
                     length, (DES_key_schedule *)schedule, ivec, enc);
}

void _ossl_old_des_xcbc_encrypt(_ossl_old_des_cblock *input,
                                _ossl_old_des_cblock *output, long length,
                                des_key_schedule schedule,
                                _ossl_old_des_cblock *ivec,
                                _ossl_old_des_cblock *inw,
                                _ossl_old_des_cblock *outw, int enc)
{
    DES_xcbc_encrypt((unsigned char *)input, (unsigned char *)output,
                     length, (DES_key_schedule *)schedule, ivec, inw, outw,
                     enc);
}

void _ossl_old_des_cfb_encrypt(unsigned char *in, unsigned char *out,
                               int numbits, long length,
                               des_key_schedule schedule,
                               _ossl_old_des_cblock *ivec, int enc)
{
    DES_cfb_encrypt(in, out, numbits, length,
                    (DES_key_schedule *)schedule, ivec, enc);
}

void _ossl_old_des_ecb_encrypt(_ossl_old_des_cblock *input,
                               _ossl_old_des_cblock *output,
                               des_key_schedule ks, int enc)
{
    DES_ecb_encrypt(input, output, (DES_key_schedule *)ks, enc);
}

void _ossl_old_des_encrypt(DES_LONG *data, des_key_schedule ks, int enc)
{
    DES_encrypt1(data, (DES_key_schedule *)ks, enc);
}

void _ossl_old_des_encrypt2(DES_LONG *data, des_key_schedule ks, int enc)
{
    DES_encrypt2(data, (DES_key_schedule *)ks, enc);
}

void _ossl_old_des_encrypt3(DES_LONG *data, des_key_schedule ks1,
                            des_key_schedule ks2, des_key_schedule ks3)
{
    DES_encrypt3(data, (DES_key_schedule *)ks1, (DES_key_schedule *)ks2,
                 (DES_key_schedule *)ks3);
}

void _ossl_old_des_decrypt3(DES_LONG *data, des_key_schedule ks1,
                            des_key_schedule ks2, des_key_schedule ks3)
{
    DES_decrypt3(data, (DES_key_schedule *)ks1, (DES_key_schedule *)ks2,
                 (DES_key_schedule *)ks3);
}

void _ossl_old_des_ede3_cbc_encrypt(_ossl_old_des_cblock *input,
                                    _ossl_old_des_cblock *output, long length,
                                    des_key_schedule ks1,
                                    des_key_schedule ks2,
                                    des_key_schedule ks3,
                                    _ossl_old_des_cblock *ivec, int enc)
{
    DES_ede3_cbc_encrypt((unsigned char *)input, (unsigned char *)output,
                         length, (DES_key_schedule *)ks1,
                         (DES_key_schedule *)ks2, (DES_key_schedule *)ks3,
                         ivec, enc);
}

void _ossl_old_des_ede3_cfb64_encrypt(unsigned char *in, unsigned char *out,
                                      long length, des_key_schedule ks1,
                                      des_key_schedule ks2,
                                      des_key_schedule ks3,
                                      _ossl_old_des_cblock *ivec, int *num,
                                      int enc)
{
    DES_ede3_cfb64_encrypt(in, out, length,
                           (DES_key_schedule *)ks1, (DES_key_schedule *)ks2,
                           (DES_key_schedule *)ks3, ivec, num, enc);
}

void _ossl_old_des_ede3_ofb64_encrypt(unsigned char *in, unsigned char *out,
                                      long length, des_key_schedule ks1,
                                      des_key_schedule ks2,
                                      des_key_schedule ks3,
                                      _ossl_old_des_cblock *ivec, int *num)
{
    DES_ede3_ofb64_encrypt(in, out, length,
                           (DES_key_schedule *)ks1, (DES_key_schedule *)ks2,
                           (DES_key_schedule *)ks3, ivec, num);
}

#if 0                           /* broken code, preserved just in case anyone
                                 * specifically looks for this */
void _ossl_old_des_xwhite_in2out(_ossl_old_des_cblock (*des_key),
                                 _ossl_old_des_cblock (*in_white),
                                 _ossl_old_des_cblock (*out_white))
{
    DES_xwhite_in2out(des_key, in_white, out_white);
}
#endif

int _ossl_old_des_enc_read(int fd, char *buf, int len, des_key_schedule sched,
                           _ossl_old_des_cblock *iv)
{
    return DES_enc_read(fd, buf, len, (DES_key_schedule *)sched, iv);
}

int _ossl_old_des_enc_write(int fd, char *buf, int len,
                            des_key_schedule sched, _ossl_old_des_cblock *iv)
{
    return DES_enc_write(fd, buf, len, (DES_key_schedule *)sched, iv);
}

char *_ossl_old_des_fcrypt(const char *buf, const char *salt, char *ret)
{
    return DES_fcrypt(buf, salt, ret);
}

char *_ossl_old_des_crypt(const char *buf, const char *salt)
{
    return DES_crypt(buf, salt);
}

char *_ossl_old_crypt(const char *buf, const char *salt)
{
    return DES_crypt(buf, salt);
}

void _ossl_old_des_ofb_encrypt(unsigned char *in, unsigned char *out,
                               int numbits, long length,
                               des_key_schedule schedule,
                               _ossl_old_des_cblock *ivec)
{
    DES_ofb_encrypt(in, out, numbits, length, (DES_key_schedule *)schedule,
                    ivec);
}

void _ossl_old_des_pcbc_encrypt(_ossl_old_des_cblock *input,
                                _ossl_old_des_cblock *output, long length,
                                des_key_schedule schedule,
                                _ossl_old_des_cblock *ivec, int enc)
{
    DES_pcbc_encrypt((unsigned char *)input, (unsigned char *)output,
                     length, (DES_key_schedule *)schedule, ivec, enc);
}

DES_LONG _ossl_old_des_quad_cksum(_ossl_old_des_cblock *input,
                                  _ossl_old_des_cblock *output, long length,
                                  int out_count, _ossl_old_des_cblock *seed)
{
    return DES_quad_cksum((unsigned char *)input, output, length,
                          out_count, seed);
}

void _ossl_old_des_random_seed(_ossl_old_des_cblock key)
{
    RAND_seed(key, sizeof(_ossl_old_des_cblock));
}

void _ossl_old_des_random_key(_ossl_old_des_cblock ret)
{
    DES_random_key((DES_cblock *)ret);
}

int _ossl_old_des_read_password(_ossl_old_des_cblock *key, const char *prompt,
                                int verify)
{
    return DES_read_password(key, prompt, verify);
}

int _ossl_old_des_read_2passwords(_ossl_old_des_cblock *key1,
                                  _ossl_old_des_cblock *key2,
                                  const char *prompt, int verify)
{
    return DES_read_2passwords(key1, key2, prompt, verify);
}

void _ossl_old_des_set_odd_parity(_ossl_old_des_cblock *key)
{
    DES_set_odd_parity(key);
}

int _ossl_old_des_is_weak_key(_ossl_old_des_cblock *key)
{
    return DES_is_weak_key(key);
}

int _ossl_old_des_set_key(_ossl_old_des_cblock *key,
                          des_key_schedule schedule)
{
    return DES_set_key(key, (DES_key_schedule *)schedule);
}

int _ossl_old_des_key_sched(_ossl_old_des_cblock *key,
                            des_key_schedule schedule)
{
    return DES_key_sched(key, (DES_key_schedule *)schedule);
}

void _ossl_old_des_string_to_key(char *str, _ossl_old_des_cblock *key)
{
    DES_string_to_key(str, key);
}

void _ossl_old_des_string_to_2keys(char *str, _ossl_old_des_cblock *key1,
                                   _ossl_old_des_cblock *key2)
{
    DES_string_to_2keys(str, key1, key2);
}

void _ossl_old_des_cfb64_encrypt(unsigned char *in, unsigned char *out,
                                 long length, des_key_schedule schedule,
                                 _ossl_old_des_cblock *ivec, int *num,
                                 int enc)
{
    DES_cfb64_encrypt(in, out, length, (DES_key_schedule *)schedule,
                      ivec, num, enc);
}

void _ossl_old_des_ofb64_encrypt(unsigned char *in, unsigned char *out,
                                 long length, des_key_schedule schedule,
                                 _ossl_old_des_cblock *ivec, int *num)
{
    DES_ofb64_encrypt(in, out, length, (DES_key_schedule *)schedule,
                      ivec, num);
}
