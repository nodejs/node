/* p12_npas.c */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 1999.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>

/* PKCS#12 password change routine */

static int newpass_p12(PKCS12 *p12, const char *oldpass, const char *newpass);
static int newpass_bags(STACK_OF(PKCS12_SAFEBAG) *bags, const char *oldpass,
                        const char *newpass);
static int newpass_bag(PKCS12_SAFEBAG *bag, const char *oldpass,
                        const char *newpass);
static int alg_get(X509_ALGOR *alg, int *pnid, int *piter, int *psaltlen);

/*
 * Change the password on a PKCS#12 structure.
 */

int PKCS12_newpass(PKCS12 *p12, const char *oldpass, const char *newpass)
{
    /* Check for NULL PKCS12 structure */

    if (!p12) {
        PKCS12err(PKCS12_F_PKCS12_NEWPASS,
                  PKCS12_R_INVALID_NULL_PKCS12_POINTER);
        return 0;
    }

    /* Check the mac */

    if (!PKCS12_verify_mac(p12, oldpass, -1)) {
        PKCS12err(PKCS12_F_PKCS12_NEWPASS, PKCS12_R_MAC_VERIFY_FAILURE);
        return 0;
    }

    if (!newpass_p12(p12, oldpass, newpass)) {
        PKCS12err(PKCS12_F_PKCS12_NEWPASS, PKCS12_R_PARSE_ERROR);
        return 0;
    }

    return 1;
}

/* Parse the outer PKCS#12 structure */

static int newpass_p12(PKCS12 *p12, const char *oldpass, const char *newpass)
{
    STACK_OF(PKCS7) *asafes = NULL, *newsafes = NULL;
    STACK_OF(PKCS12_SAFEBAG) *bags = NULL;
    int i, bagnid, pbe_nid = 0, pbe_iter = 0, pbe_saltlen = 0;
    PKCS7 *p7, *p7new;
    ASN1_OCTET_STRING *p12_data_tmp = NULL;
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int maclen;
    int rv = 0;

    if ((asafes = PKCS12_unpack_authsafes(p12)) == NULL)
        goto err;
    if ((newsafes = sk_PKCS7_new_null()) == NULL)
        goto err;
    for (i = 0; i < sk_PKCS7_num(asafes); i++) {
        p7 = sk_PKCS7_value(asafes, i);
        bagnid = OBJ_obj2nid(p7->type);
        if (bagnid == NID_pkcs7_data) {
            bags = PKCS12_unpack_p7data(p7);
        } else if (bagnid == NID_pkcs7_encrypted) {
            bags = PKCS12_unpack_p7encdata(p7, oldpass, -1);
            if (!alg_get(p7->d.encrypted->enc_data->algorithm,
                         &pbe_nid, &pbe_iter, &pbe_saltlen))
                goto err;
        } else {
            continue;
        }
        if (bags == NULL)
            goto err;
        if (!newpass_bags(bags, oldpass, newpass))
            goto err;
        /* Repack bag in same form with new password */
        if (bagnid == NID_pkcs7_data)
            p7new = PKCS12_pack_p7data(bags);
        else
            p7new = PKCS12_pack_p7encdata(pbe_nid, newpass, -1, NULL,
                                          pbe_saltlen, pbe_iter, bags);
        if (!p7new || !sk_PKCS7_push(newsafes, p7new))
            goto err;
        sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
        bags = NULL;
    }

    /* Repack safe: save old safe in case of error */

    p12_data_tmp = p12->authsafes->d.data;
    if ((p12->authsafes->d.data = ASN1_OCTET_STRING_new()) == NULL)
        goto err;
    if (!PKCS12_pack_authsafes(p12, newsafes))
        goto err;
    if (!PKCS12_gen_mac(p12, newpass, -1, mac, &maclen))
        goto err;
    if (!ASN1_OCTET_STRING_set(p12->mac->dinfo->digest, mac, maclen))
        goto err;

    rv = 1;

err:
    /* Restore old safe if necessary */
    if (rv == 1) {
        ASN1_OCTET_STRING_free(p12_data_tmp);
    } else if (p12_data_tmp != NULL) {
        ASN1_OCTET_STRING_free(p12->authsafes->d.data);
        p12->authsafes->d.data = p12_data_tmp;
    }
    sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
    sk_PKCS7_pop_free(asafes, PKCS7_free);
    sk_PKCS7_pop_free(newsafes, PKCS7_free);
    return rv;
}

static int newpass_bags(STACK_OF(PKCS12_SAFEBAG) *bags, const char *oldpass,
                        const char *newpass)
{
    int i;
    for (i = 0; i < sk_PKCS12_SAFEBAG_num(bags); i++) {
        if (!newpass_bag(sk_PKCS12_SAFEBAG_value(bags, i), oldpass, newpass))
            return 0;
    }
    return 1;
}

/* Change password of safebag: only needs handle shrouded keybags */

static int newpass_bag(PKCS12_SAFEBAG *bag, const char *oldpass,
                       const char *newpass)
{
    PKCS8_PRIV_KEY_INFO *p8;
    X509_SIG *p8new;
    int p8_nid, p8_saltlen, p8_iter;

    if (M_PKCS12_bag_type(bag) != NID_pkcs8ShroudedKeyBag)
        return 1;

    if (!(p8 = PKCS8_decrypt(bag->value.shkeybag, oldpass, -1)))
        return 0;
    if (!alg_get(bag->value.shkeybag->algor, &p8_nid, &p8_iter, &p8_saltlen))
        return 0;
    p8new = PKCS8_encrypt(p8_nid, NULL, newpass, -1, NULL, p8_saltlen,
                          p8_iter, p8);
    PKCS8_PRIV_KEY_INFO_free(p8);
    if (p8new == NULL)
        return 0;
    X509_SIG_free(bag->value.shkeybag);
    bag->value.shkeybag = p8new;
    return 1;
}

static int alg_get(X509_ALGOR *alg, int *pnid, int *piter, int *psaltlen)
{
    PBEPARAM *pbe;
    const unsigned char *p;

    p = alg->parameter->value.sequence->data;
    pbe = d2i_PBEPARAM(NULL, &p, alg->parameter->value.sequence->length);
    if (!pbe)
        return 0;
    *pnid = OBJ_obj2nid(alg->algorithm);
    *piter = ASN1_INTEGER_get(pbe->iter);
    *psaltlen = pbe->salt->length;
    PBEPARAM_free(pbe);
    return 1;
}
