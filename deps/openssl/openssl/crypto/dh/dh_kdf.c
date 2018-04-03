/* crypto/dh/dh_kdf.c */
/*
 * Written by Stephen Henson for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 2013 The OpenSSL Project.  All rights reserved.
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
 */

#include <e_os.h>

#ifndef OPENSSL_NO_CMS
#include <string.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/asn1.h>
#include <openssl/cms.h>

/* Key derivation from X9.42/RFC2631 */

#define DH_KDF_MAX      (1L << 30)

/* Skip past an ASN1 structure: for OBJECT skip content octets too */

static int skip_asn1(unsigned char **pp, long *plen, int exptag)
{
    const unsigned char *q = *pp;
    int i, tag, xclass;
    long tmplen;
    i = ASN1_get_object(&q, &tmplen, &tag, &xclass, *plen);
    if (i & 0x80)
        return 0;
    if (tag != exptag || xclass != V_ASN1_UNIVERSAL)
        return 0;
    if (tag == V_ASN1_OBJECT)
        q += tmplen;
    *plen -= q - *pp;
    *pp = (unsigned char *)q;
    return 1;
}

/*
 * Encode the DH shared info structure, return an offset to the counter value
 * so we can update the structure without reencoding it.
 */

static int dh_sharedinfo_encode(unsigned char **pder, unsigned char **pctr,
                                ASN1_OBJECT *key_oid, size_t outlen,
                                const unsigned char *ukm, size_t ukmlen)
{
    unsigned char *p;
    int derlen;
    long tlen;
    /* "magic" value to check offset is sane */
    static unsigned char ctr[4] = { 0xF3, 0x17, 0x22, 0x53 };
    X509_ALGOR atmp;
    ASN1_OCTET_STRING ctr_oct, ukm_oct, *pukm_oct;
    ASN1_TYPE ctr_atype;
    if (ukmlen > DH_KDF_MAX || outlen > DH_KDF_MAX)
        return 0;
    ctr_oct.data = ctr;
    ctr_oct.length = 4;
    ctr_oct.flags = 0;
    ctr_oct.type = V_ASN1_OCTET_STRING;
    ctr_atype.type = V_ASN1_OCTET_STRING;
    ctr_atype.value.octet_string = &ctr_oct;
    atmp.algorithm = key_oid;
    atmp.parameter = &ctr_atype;
    if (ukm) {
        ukm_oct.type = V_ASN1_OCTET_STRING;
        ukm_oct.flags = 0;
        ukm_oct.data = (unsigned char *)ukm;
        ukm_oct.length = ukmlen;
        pukm_oct = &ukm_oct;
    } else
        pukm_oct = NULL;
    derlen = CMS_SharedInfo_encode(pder, &atmp, pukm_oct, outlen);
    if (derlen <= 0)
        return 0;
    p = *pder;
    tlen = derlen;
    if (!skip_asn1(&p, &tlen, V_ASN1_SEQUENCE))
        return 0;
    if (!skip_asn1(&p, &tlen, V_ASN1_SEQUENCE))
        return 0;
    if (!skip_asn1(&p, &tlen, V_ASN1_OBJECT))
        return 0;
    if (!skip_asn1(&p, &tlen, V_ASN1_OCTET_STRING))
        return 0;
    if (CRYPTO_memcmp(p, ctr, 4))
        return 0;
    *pctr = p;
    return derlen;
}

int DH_KDF_X9_42(unsigned char *out, size_t outlen,
                 const unsigned char *Z, size_t Zlen,
                 ASN1_OBJECT *key_oid,
                 const unsigned char *ukm, size_t ukmlen, const EVP_MD *md)
{
    EVP_MD_CTX mctx;
    int rv = 0;
    unsigned int i;
    size_t mdlen;
    unsigned char *der = NULL, *ctr;
    int derlen;
    if (Zlen > DH_KDF_MAX)
        return 0;
    mdlen = EVP_MD_size(md);
    EVP_MD_CTX_init(&mctx);
    derlen = dh_sharedinfo_encode(&der, &ctr, key_oid, outlen, ukm, ukmlen);
    if (derlen == 0)
        goto err;
    for (i = 1;; i++) {
        unsigned char mtmp[EVP_MAX_MD_SIZE];
        EVP_DigestInit_ex(&mctx, md, NULL);
        if (!EVP_DigestUpdate(&mctx, Z, Zlen))
            goto err;
        ctr[3] = i & 0xFF;
        ctr[2] = (i >> 8) & 0xFF;
        ctr[1] = (i >> 16) & 0xFF;
        ctr[0] = (i >> 24) & 0xFF;
        if (!EVP_DigestUpdate(&mctx, der, derlen))
            goto err;
        if (outlen >= mdlen) {
            if (!EVP_DigestFinal(&mctx, out, NULL))
                goto err;
            outlen -= mdlen;
            if (outlen == 0)
                break;
            out += mdlen;
        } else {
            if (!EVP_DigestFinal(&mctx, mtmp, NULL))
                goto err;
            memcpy(out, mtmp, outlen);
            OPENSSL_cleanse(mtmp, mdlen);
            break;
        }
    }
    rv = 1;
 err:
    if (der)
        OPENSSL_free(der);
    EVP_MD_CTX_cleanup(&mctx);
    return rv;
}
#endif
