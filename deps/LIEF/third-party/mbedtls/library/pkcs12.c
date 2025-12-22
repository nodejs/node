/*
 *  PKCS#12 Personal Information Exchange Syntax
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 *  The PKCS #12 Personal Information Exchange Syntax Standard v1.1
 *
 *  http://www.rsa.com/rsalabs/pkcs/files/h11301-wp-pkcs-12v1-1-personal-information-exchange-syntax.pdf
 *  ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-12/pkcs-12v1-1.asn
 */

#include "common.h"

#if defined(MBEDTLS_PKCS12_C)

#include "mbedtls/pkcs12.h"
#include "mbedtls/asn1.h"
#if defined(MBEDTLS_CIPHER_C)
#include "mbedtls/cipher.h"
#endif /* MBEDTLS_CIPHER_C */
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#if defined(MBEDTLS_DES_C)
#include "mbedtls/des.h"
#endif

#include "psa_util_internal.h"

#if defined(MBEDTLS_ASN1_PARSE_C) && defined(MBEDTLS_CIPHER_C)

static int pkcs12_parse_pbe_params(mbedtls_asn1_buf *params,
                                   mbedtls_asn1_buf *salt, int *iterations)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char **p = &params->p;
    const unsigned char *end = params->p + params->len;

    /*
     *  pkcs-12PbeParams ::= SEQUENCE {
     *    salt          OCTET STRING,
     *    iterations    INTEGER
     *  }
     *
     */
    if (params->tag != (MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_UNEXPECTED_TAG);
    }

    if ((ret = mbedtls_asn1_get_tag(p, end, &salt->len, MBEDTLS_ASN1_OCTET_STRING)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT, ret);
    }

    salt->p = *p;
    *p += salt->len;

    if ((ret = mbedtls_asn1_get_int(p, end, iterations)) != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT, ret);
    }

    if (*p != end) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PKCS12_PBE_INVALID_FORMAT,
                                 MBEDTLS_ERR_ASN1_LENGTH_MISMATCH);
    }

    return 0;
}

#define PKCS12_MAX_PWDLEN 128

static int pkcs12_pbe_derive_key_iv(mbedtls_asn1_buf *pbe_params, mbedtls_md_type_t md_type,
                                    const unsigned char *pwd,  size_t pwdlen,
                                    unsigned char *key, size_t keylen,
                                    unsigned char *iv,  size_t ivlen)
{
    int ret, iterations = 0;
    mbedtls_asn1_buf salt;
    size_t i;
    unsigned char unipwd[PKCS12_MAX_PWDLEN * 2 + 2];

    if (pwdlen > PKCS12_MAX_PWDLEN) {
        return MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA;
    }

    memset(&salt, 0, sizeof(mbedtls_asn1_buf));
    memset(&unipwd, 0, sizeof(unipwd));

    if ((ret = pkcs12_parse_pbe_params(pbe_params, &salt,
                                       &iterations)) != 0) {
        return ret;
    }

    for (i = 0; i < pwdlen; i++) {
        unipwd[i * 2 + 1] = pwd[i];
    }

    if ((ret = mbedtls_pkcs12_derivation(key, keylen, unipwd, pwdlen * 2 + 2,
                                         salt.p, salt.len, md_type,
                                         MBEDTLS_PKCS12_DERIVE_KEY, iterations)) != 0) {
        return ret;
    }

    if (iv == NULL || ivlen == 0) {
        return 0;
    }

    if ((ret = mbedtls_pkcs12_derivation(iv, ivlen, unipwd, pwdlen * 2 + 2,
                                         salt.p, salt.len, md_type,
                                         MBEDTLS_PKCS12_DERIVE_IV, iterations)) != 0) {
        return ret;
    }
    return 0;
}

#undef PKCS12_MAX_PWDLEN

#if !defined(MBEDTLS_CIPHER_PADDING_PKCS7)
int mbedtls_pkcs12_pbe_ext(mbedtls_asn1_buf *pbe_params, int mode,
                           mbedtls_cipher_type_t cipher_type, mbedtls_md_type_t md_type,
                           const unsigned char *pwd,  size_t pwdlen,
                           const unsigned char *data, size_t len,
                           unsigned char *output, size_t output_size,
                           size_t *output_len);
#endif

#if !defined(MBEDTLS_DEPRECATED_REMOVED)
int mbedtls_pkcs12_pbe(mbedtls_asn1_buf *pbe_params, int mode,
                       mbedtls_cipher_type_t cipher_type, mbedtls_md_type_t md_type,
                       const unsigned char *pwd,  size_t pwdlen,
                       const unsigned char *data, size_t len,
                       unsigned char *output)
{
    size_t output_len = 0;

    /* We assume caller of the function is providing a big enough output buffer
     * so we pass output_size as SIZE_MAX to pass checks, However, no guarantees
     * for the output size actually being correct.
     */
    return mbedtls_pkcs12_pbe_ext(pbe_params, mode, cipher_type, md_type,
                                  pwd, pwdlen, data, len, output, SIZE_MAX,
                                  &output_len);
}
#endif

int mbedtls_pkcs12_pbe_ext(mbedtls_asn1_buf *pbe_params, int mode,
                           mbedtls_cipher_type_t cipher_type, mbedtls_md_type_t md_type,
                           const unsigned char *pwd,  size_t pwdlen,
                           const unsigned char *data, size_t len,
                           unsigned char *output, size_t output_size,
                           size_t *output_len)
{
    int ret, keylen = 0;
    unsigned char key[32];
    unsigned char iv[16];
    const mbedtls_cipher_info_t *cipher_info;
    mbedtls_cipher_context_t cipher_ctx;
    size_t iv_len = 0;
    size_t finish_olen = 0;
    unsigned int padlen = 0;

    if (pwd == NULL && pwdlen != 0) {
        return MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA;
    }

    cipher_info = mbedtls_cipher_info_from_type(cipher_type);
    if (cipher_info == NULL) {
        return MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE;
    }

    keylen = (int) mbedtls_cipher_info_get_key_bitlen(cipher_info) / 8;

    if (mode == MBEDTLS_PKCS12_PBE_DECRYPT) {
        if (output_size < len) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
    }

    if (mode == MBEDTLS_PKCS12_PBE_ENCRYPT) {
        padlen = cipher_info->block_size - (len % cipher_info->block_size);
        if (output_size < (len + padlen)) {
            return MBEDTLS_ERR_ASN1_BUF_TOO_SMALL;
        }
    }

    iv_len = mbedtls_cipher_info_get_iv_size(cipher_info);
    if ((ret = pkcs12_pbe_derive_key_iv(pbe_params, md_type, pwd, pwdlen,
                                        key, keylen,
                                        iv, iv_len)) != 0) {
        return ret;
    }

    mbedtls_cipher_init(&cipher_ctx);

    if ((ret = mbedtls_cipher_setup(&cipher_ctx, cipher_info)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_cipher_setkey(&cipher_ctx, key, 8 * keylen,
                                     (mbedtls_operation_t) mode)) != 0) {
        goto exit;
    }

#if defined(MBEDTLS_CIPHER_MODE_WITH_PADDING)
    {
        /* PKCS12 uses CBC with PKCS7 padding */
        mbedtls_cipher_padding_t padding = MBEDTLS_PADDING_PKCS7;
#if !defined(MBEDTLS_CIPHER_PADDING_PKCS7)
        /* For historical reasons, when decrypting, this function works when
         * decrypting even when support for PKCS7 padding is disabled. In this
         * case, it ignores the padding, and so will never report a
         * password mismatch.
         */
        if (mode == MBEDTLS_PKCS12_PBE_DECRYPT) {
            padding = MBEDTLS_PADDING_NONE;
        }
#endif
        if ((ret = mbedtls_cipher_set_padding_mode(&cipher_ctx, padding)) != 0) {
            goto exit;
        }
    }
#endif /* MBEDTLS_CIPHER_MODE_WITH_PADDING */

    ret = mbedtls_cipher_crypt(&cipher_ctx, iv, iv_len, data, len, output, &finish_olen);
    if (ret == MBEDTLS_ERR_CIPHER_INVALID_PADDING) {
        ret = MBEDTLS_ERR_PKCS12_PASSWORD_MISMATCH;
    }

    *output_len += finish_olen;

exit:
    mbedtls_platform_zeroize(key, sizeof(key));
    mbedtls_platform_zeroize(iv,  sizeof(iv));
    mbedtls_cipher_free(&cipher_ctx);

    return ret;
}

#endif /* MBEDTLS_ASN1_PARSE_C && MBEDTLS_CIPHER_C */

static void pkcs12_fill_buffer(unsigned char *data, size_t data_len,
                               const unsigned char *filler, size_t fill_len)
{
    unsigned char *p = data;
    size_t use_len;

    if (filler != NULL && fill_len != 0) {
        while (data_len > 0) {
            use_len = (data_len > fill_len) ? fill_len : data_len;
            memcpy(p, filler, use_len);
            p += use_len;
            data_len -= use_len;
        }
    } else {
        /* If either of the above are not true then clearly there is nothing
         * that this function can do. The function should *not* be called
         * under either of those circumstances, as you could end up with an
         * incorrect output but for safety's sake, leaving the check in as
         * otherwise we could end up with memory corruption.*/
    }
}


static int calculate_hashes(mbedtls_md_type_t md_type, int iterations,
                            unsigned char *diversifier, unsigned char *salt_block,
                            unsigned char *pwd_block, unsigned char *hash_output, int use_salt,
                            int use_password, size_t hlen, size_t v)
{
    int ret = -1;
    size_t i;
    const mbedtls_md_info_t *md_info;
    mbedtls_md_context_t md_ctx;
    md_info = mbedtls_md_info_from_type(md_type);
    if (md_info == NULL) {
        return MBEDTLS_ERR_PKCS12_FEATURE_UNAVAILABLE;
    }

    mbedtls_md_init(&md_ctx);

    if ((ret = mbedtls_md_setup(&md_ctx, md_info, 0)) != 0) {
        return ret;
    }
    // Calculate hash( diversifier || salt_block || pwd_block )
    if ((ret = mbedtls_md_starts(&md_ctx)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_md_update(&md_ctx, diversifier, v)) != 0) {
        goto exit;
    }

    if (use_salt != 0) {
        if ((ret = mbedtls_md_update(&md_ctx, salt_block, v)) != 0) {
            goto exit;
        }
    }

    if (use_password != 0) {
        if ((ret = mbedtls_md_update(&md_ctx, pwd_block, v)) != 0) {
            goto exit;
        }
    }

    if ((ret = mbedtls_md_finish(&md_ctx, hash_output)) != 0) {
        goto exit;
    }

    // Perform remaining ( iterations - 1 ) recursive hash calculations
    for (i = 1; i < (size_t) iterations; i++) {
        if ((ret = mbedtls_md(md_info, hash_output, hlen, hash_output))
            != 0) {
            goto exit;
        }
    }

exit:
    mbedtls_md_free(&md_ctx);
    return ret;
}


int mbedtls_pkcs12_derivation(unsigned char *data, size_t datalen,
                              const unsigned char *pwd, size_t pwdlen,
                              const unsigned char *salt, size_t saltlen,
                              mbedtls_md_type_t md_type, int id, int iterations)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned int j;

    unsigned char diversifier[128];
    unsigned char salt_block[128], pwd_block[128], hash_block[128] = { 0 };
    unsigned char hash_output[MBEDTLS_MD_MAX_SIZE];
    unsigned char *p;
    unsigned char c;
    int           use_password = 0;
    int           use_salt = 0;

    size_t hlen, use_len, v, i;

    // This version only allows max of 64 bytes of password or salt
    if (datalen > 128 || pwdlen > 64 || saltlen > 64) {
        return MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA;
    }

    if (pwd == NULL && pwdlen != 0) {
        return MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA;
    }

    if (salt == NULL && saltlen != 0) {
        return MBEDTLS_ERR_PKCS12_BAD_INPUT_DATA;
    }

    use_password = (pwd && pwdlen != 0);
    use_salt = (salt && saltlen != 0);

    hlen = mbedtls_md_get_size_from_type(md_type);

    if (hlen <= 32) {
        v = 64;
    } else {
        v = 128;
    }

    memset(diversifier, (unsigned char) id, v);

    if (use_salt != 0) {
        pkcs12_fill_buffer(salt_block, v, salt, saltlen);
    }

    if (use_password != 0) {
        pkcs12_fill_buffer(pwd_block,  v, pwd,  pwdlen);
    }

    p = data;
    while (datalen > 0) {
        if (calculate_hashes(md_type, iterations, diversifier, salt_block,
                             pwd_block, hash_output, use_salt, use_password, hlen,
                             v) != 0) {
            goto exit;
        }

        use_len = (datalen > hlen) ? hlen : datalen;
        memcpy(p, hash_output, use_len);
        datalen -= use_len;
        p += use_len;

        if (datalen == 0) {
            break;
        }

        // Concatenating copies of hash_output into hash_block (B)
        pkcs12_fill_buffer(hash_block, v, hash_output, hlen);

        // B += 1
        for (i = v; i > 0; i--) {
            if (++hash_block[i - 1] != 0) {
                break;
            }
        }

        if (use_salt != 0) {
            // salt_block += B
            c = 0;
            for (i = v; i > 0; i--) {
                j = salt_block[i - 1] + hash_block[i - 1] + c;
                c = MBEDTLS_BYTE_1(j);
                salt_block[i - 1] = MBEDTLS_BYTE_0(j);
            }
        }

        if (use_password != 0) {
            // pwd_block  += B
            c = 0;
            for (i = v; i > 0; i--) {
                j = pwd_block[i - 1] + hash_block[i - 1] + c;
                c = MBEDTLS_BYTE_1(j);
                pwd_block[i - 1] = MBEDTLS_BYTE_0(j);
            }
        }
    }

    ret = 0;

exit:
    mbedtls_platform_zeroize(salt_block, sizeof(salt_block));
    mbedtls_platform_zeroize(pwd_block, sizeof(pwd_block));
    mbedtls_platform_zeroize(hash_block, sizeof(hash_block));
    mbedtls_platform_zeroize(hash_output, sizeof(hash_output));

    return ret;
}

#endif /* MBEDTLS_PKCS12_C */
