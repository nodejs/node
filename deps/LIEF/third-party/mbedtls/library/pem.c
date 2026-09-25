/*
 *  Privacy Enhanced Mail (PEM) decoding
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PEM_PARSE_C) || defined(MBEDTLS_PEM_WRITE_C)

#include "mbedtls/pem.h"
#include "mbedtls/base64.h"
#include "mbedtls/des.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "mbedtls/cipher.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

#include <string.h>

#include "mbedtls/platform.h"

#if defined(MBEDTLS_USE_PSA_CRYPTO)
#include "psa/crypto.h"
#endif

#if defined(MBEDTLS_MD_CAN_MD5) &&  \
    defined(MBEDTLS_CIPHER_MODE_CBC) &&                             \
    (defined(MBEDTLS_DES_C) || defined(MBEDTLS_AES_C))
#define PEM_RFC1421
#endif /* MBEDTLS_MD_CAN_MD5 &&
          MBEDTLS_CIPHER_MODE_CBC &&
          ( MBEDTLS_AES_C || MBEDTLS_DES_C ) */

#if defined(MBEDTLS_PEM_PARSE_C)
void mbedtls_pem_init(mbedtls_pem_context *ctx)
{
    memset(ctx, 0, sizeof(mbedtls_pem_context));
}

#if defined(PEM_RFC1421)
/*
 * Read a 16-byte hex string and convert it to binary
 */
static int pem_get_iv(const unsigned char *s, unsigned char *iv,
                      size_t iv_len)
{
    size_t i, j, k;

    memset(iv, 0, iv_len);

    for (i = 0; i < iv_len * 2; i++, s++) {
        if (*s >= '0' && *s <= '9') {
            j = *s - '0';
        } else
        if (*s >= 'A' && *s <= 'F') {
            j = *s - '7';
        } else
        if (*s >= 'a' && *s <= 'f') {
            j = *s - 'W';
        } else {
            return MBEDTLS_ERR_PEM_INVALID_ENC_IV;
        }

        k = ((i & 1) != 0) ? j : j << 4;

        iv[i >> 1] = (unsigned char) (iv[i >> 1] | k);
    }

    return 0;
}

static int pem_pbkdf1(unsigned char *key, size_t keylen,
                      unsigned char *iv,
                      const unsigned char *pwd, size_t pwdlen)
{
    mbedtls_md_context_t md5_ctx;
    const mbedtls_md_info_t *md5_info;
    unsigned char md5sum[16];
    size_t use_len;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_md_init(&md5_ctx);

    /* Prepare the context. (setup() errors gracefully on NULL info.) */
    md5_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
    if ((ret = mbedtls_md_setup(&md5_ctx, md5_info, 0)) != 0) {
        goto exit;
    }

    /*
     * key[ 0..15] = MD5(pwd || IV)
     */
    if ((ret = mbedtls_md_starts(&md5_ctx)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_update(&md5_ctx, pwd, pwdlen)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_update(&md5_ctx, iv,  8)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_finish(&md5_ctx, md5sum)) != 0) {
        goto exit;
    }

    if (keylen <= 16) {
        memcpy(key, md5sum, keylen);
        goto exit;
    }

    memcpy(key, md5sum, 16);

    /*
     * key[16..23] = MD5(key[ 0..15] || pwd || IV])
     */
    if ((ret = mbedtls_md_starts(&md5_ctx)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_update(&md5_ctx, md5sum, 16)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_update(&md5_ctx, pwd, pwdlen)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_update(&md5_ctx, iv, 8)) != 0) {
        goto exit;
    }
    if ((ret = mbedtls_md_finish(&md5_ctx, md5sum)) != 0) {
        goto exit;
    }

    use_len = 16;
    if (keylen < 32) {
        use_len = keylen - 16;
    }

    memcpy(key + 16, md5sum, use_len);

exit:
    mbedtls_md_free(&md5_ctx);
    mbedtls_platform_zeroize(md5sum, 16);

    return ret;
}

#if defined(MBEDTLS_DES_C)
/*
 * Decrypt with DES-CBC, using PBKDF1 for key derivation
 */
static int pem_des_decrypt(unsigned char des_iv[8],
                           unsigned char *buf, size_t buflen,
                           const unsigned char *pwd, size_t pwdlen)
{
    mbedtls_des_context des_ctx;
    unsigned char des_key[8];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_des_init(&des_ctx);

    if ((ret = pem_pbkdf1(des_key, 8, des_iv, pwd, pwdlen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_des_setkey_dec(&des_ctx, des_key)) != 0) {
        goto exit;
    }
    ret = mbedtls_des_crypt_cbc(&des_ctx, MBEDTLS_DES_DECRYPT, buflen,
                                des_iv, buf, buf);

exit:
    mbedtls_des_free(&des_ctx);
    mbedtls_platform_zeroize(des_key, 8);

    return ret;
}

/*
 * Decrypt with 3DES-CBC, using PBKDF1 for key derivation
 */
static int pem_des3_decrypt(unsigned char des3_iv[8],
                            unsigned char *buf, size_t buflen,
                            const unsigned char *pwd, size_t pwdlen)
{
    mbedtls_des3_context des3_ctx;
    unsigned char des3_key[24];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_des3_init(&des3_ctx);

    if ((ret = pem_pbkdf1(des3_key, 24, des3_iv, pwd, pwdlen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_des3_set3key_dec(&des3_ctx, des3_key)) != 0) {
        goto exit;
    }
    ret = mbedtls_des3_crypt_cbc(&des3_ctx, MBEDTLS_DES_DECRYPT, buflen,
                                 des3_iv, buf, buf);

exit:
    mbedtls_des3_free(&des3_ctx);
    mbedtls_platform_zeroize(des3_key, 24);

    return ret;
}
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_AES_C)
/*
 * Decrypt with AES-XXX-CBC, using PBKDF1 for key derivation
 */
static int pem_aes_decrypt(unsigned char aes_iv[16], unsigned int keylen,
                           unsigned char *buf, size_t buflen,
                           const unsigned char *pwd, size_t pwdlen)
{
    mbedtls_aes_context aes_ctx;
    unsigned char aes_key[32];
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_aes_init(&aes_ctx);

    if ((ret = pem_pbkdf1(aes_key, keylen, aes_iv, pwd, pwdlen)) != 0) {
        goto exit;
    }

    if ((ret = mbedtls_aes_setkey_dec(&aes_ctx, aes_key, keylen * 8)) != 0) {
        goto exit;
    }
    ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, buflen,
                                aes_iv, buf, buf);

exit:
    mbedtls_aes_free(&aes_ctx);
    mbedtls_platform_zeroize(aes_key, keylen);

    return ret;
}
#endif /* MBEDTLS_AES_C */

#if defined(MBEDTLS_DES_C) || defined(MBEDTLS_AES_C)
static int pem_check_pkcs_padding(unsigned char *input, size_t input_len, size_t *data_len)
{
    /* input_len > 0 is not guaranteed by mbedtls_pem_read_buffer(). */
    if (input_len < 1) {
        return MBEDTLS_ERR_PEM_INVALID_DATA;
    }
    size_t pad_len = input[input_len - 1];
    size_t i;

    if (pad_len > input_len) {
        return MBEDTLS_ERR_PEM_PASSWORD_MISMATCH;
    }

    *data_len = input_len - pad_len;

    for (i = *data_len; i < input_len; i++) {
        if (input[i] != pad_len) {
            return MBEDTLS_ERR_PEM_PASSWORD_MISMATCH;
        }
    }

    return 0;
}
#endif /* MBEDTLS_DES_C || MBEDTLS_AES_C */

#endif /* PEM_RFC1421 */

int mbedtls_pem_read_buffer(mbedtls_pem_context *ctx, const char *header, const char *footer,
                            const unsigned char *data, const unsigned char *pwd,
                            size_t pwdlen, size_t *use_len)
{
    int ret, enc;
    size_t len;
    unsigned char *buf;
    const unsigned char *s1, *s2, *end;
#if defined(PEM_RFC1421)
    unsigned char pem_iv[16];
    mbedtls_cipher_type_t enc_alg = MBEDTLS_CIPHER_NONE;
#else
    ((void) pwd);
    ((void) pwdlen);
#endif /* PEM_RFC1421 */

    if (ctx == NULL) {
        return MBEDTLS_ERR_PEM_BAD_INPUT_DATA;
    }

    s1 = (unsigned char *) strstr((const char *) data, header);

    if (s1 == NULL) {
        return MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    }

    s2 = (unsigned char *) strstr((const char *) data, footer);

    if (s2 == NULL || s2 <= s1) {
        return MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    }

    s1 += strlen(header);
    if (*s1 == ' ') {
        s1++;
    }
    if (*s1 == '\r') {
        s1++;
    }
    if (*s1 == '\n') {
        s1++;
    } else {
        return MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT;
    }

    end = s2;
    end += strlen(footer);
    if (*end == ' ') {
        end++;
    }
    if (*end == '\r') {
        end++;
    }
    if (*end == '\n') {
        end++;
    }
    *use_len = (size_t) (end - data);

    enc = 0;

    if (s2 - s1 >= 22 && memcmp(s1, "Proc-Type: 4,ENCRYPTED", 22) == 0) {
#if defined(PEM_RFC1421)
        enc++;

        s1 += 22;
        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return MBEDTLS_ERR_PEM_INVALID_DATA;
        }


#if defined(MBEDTLS_DES_C)
        if (s2 - s1 >= 23 && memcmp(s1, "DEK-Info: DES-EDE3-CBC,", 23) == 0) {
            enc_alg = MBEDTLS_CIPHER_DES_EDE3_CBC;

            s1 += 23;
            if (s2 - s1 < 16 || pem_get_iv(s1, pem_iv, 8) != 0) {
                return MBEDTLS_ERR_PEM_INVALID_ENC_IV;
            }

            s1 += 16;
        } else if (s2 - s1 >= 18 && memcmp(s1, "DEK-Info: DES-CBC,", 18) == 0) {
            enc_alg = MBEDTLS_CIPHER_DES_CBC;

            s1 += 18;
            if (s2 - s1 < 16 || pem_get_iv(s1, pem_iv, 8) != 0) {
                return MBEDTLS_ERR_PEM_INVALID_ENC_IV;
            }

            s1 += 16;
        }
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_AES_C)
        if (s2 - s1 >= 14 && memcmp(s1, "DEK-Info: AES-", 14) == 0) {
            if (s2 - s1 < 22) {
                return MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG;
            } else if (memcmp(s1, "DEK-Info: AES-128-CBC,", 22) == 0) {
                enc_alg = MBEDTLS_CIPHER_AES_128_CBC;
            } else if (memcmp(s1, "DEK-Info: AES-192-CBC,", 22) == 0) {
                enc_alg = MBEDTLS_CIPHER_AES_192_CBC;
            } else if (memcmp(s1, "DEK-Info: AES-256-CBC,", 22) == 0) {
                enc_alg = MBEDTLS_CIPHER_AES_256_CBC;
            } else {
                return MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG;
            }

            s1 += 22;
            if (s2 - s1 < 32 || pem_get_iv(s1, pem_iv, 16) != 0) {
                return MBEDTLS_ERR_PEM_INVALID_ENC_IV;
            }

            s1 += 32;
        }
#endif /* MBEDTLS_AES_C */

        if (enc_alg == MBEDTLS_CIPHER_NONE) {
            return MBEDTLS_ERR_PEM_UNKNOWN_ENC_ALG;
        }

        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return MBEDTLS_ERR_PEM_INVALID_DATA;
        }
#else
        return MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE;
#endif /* PEM_RFC1421 */
    }

    if (s1 >= s2) {
        return MBEDTLS_ERR_PEM_INVALID_DATA;
    }

    ret = mbedtls_base64_decode(NULL, 0, &len, s1, (size_t) (s2 - s1));

    if (ret == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PEM_INVALID_DATA, ret);
    }

    if (len == 0) {
        return MBEDTLS_ERR_PEM_BAD_INPUT_DATA;
    }

    if ((buf = mbedtls_calloc(1, len)) == NULL) {
        return MBEDTLS_ERR_PEM_ALLOC_FAILED;
    }

    if ((ret = mbedtls_base64_decode(buf, len, &len, s1, (size_t) (s2 - s1))) != 0) {
        mbedtls_zeroize_and_free(buf, len);
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PEM_INVALID_DATA, ret);
    }

    if (enc != 0) {
#if defined(PEM_RFC1421)
        if (pwd == NULL) {
            mbedtls_zeroize_and_free(buf, len);
            return MBEDTLS_ERR_PEM_PASSWORD_REQUIRED;
        }

        ret = 0;

#if defined(MBEDTLS_DES_C)
        if (enc_alg == MBEDTLS_CIPHER_DES_EDE3_CBC) {
            ret = pem_des3_decrypt(pem_iv, buf, len, pwd, pwdlen);
        } else if (enc_alg == MBEDTLS_CIPHER_DES_CBC) {
            ret = pem_des_decrypt(pem_iv, buf, len, pwd, pwdlen);
        }
#endif /* MBEDTLS_DES_C */

#if defined(MBEDTLS_AES_C)
        if (enc_alg == MBEDTLS_CIPHER_AES_128_CBC) {
            ret = pem_aes_decrypt(pem_iv, 16, buf, len, pwd, pwdlen);
        } else if (enc_alg == MBEDTLS_CIPHER_AES_192_CBC) {
            ret = pem_aes_decrypt(pem_iv, 24, buf, len, pwd, pwdlen);
        } else if (enc_alg == MBEDTLS_CIPHER_AES_256_CBC) {
            ret = pem_aes_decrypt(pem_iv, 32, buf, len, pwd, pwdlen);
        }
#endif /* MBEDTLS_AES_C */

        if (ret != 0) {
            mbedtls_zeroize_and_free(buf, len);
            return ret;
        }

        /* Check PKCS padding and update data length based on padding info.
         * This can be used to detect invalid padding data and password
         * mismatches. */
        size_t unpadded_len;
        ret = pem_check_pkcs_padding(buf, len, &unpadded_len);
        if (ret != 0) {
            mbedtls_zeroize_and_free(buf, len);
            return ret;
        }
        len = unpadded_len;
#else
        mbedtls_zeroize_and_free(buf, len);
        return MBEDTLS_ERR_PEM_FEATURE_UNAVAILABLE;
#endif /* PEM_RFC1421 */
    }

    ctx->buf = buf;
    ctx->buflen = len;

    return 0;
}

void mbedtls_pem_free(mbedtls_pem_context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->buf != NULL) {
        mbedtls_zeroize_and_free(ctx->buf, ctx->buflen);
    }
    mbedtls_free(ctx->info);

    mbedtls_platform_zeroize(ctx, sizeof(mbedtls_pem_context));
}
#endif /* MBEDTLS_PEM_PARSE_C */

#if defined(MBEDTLS_PEM_WRITE_C)
int mbedtls_pem_write_buffer(const char *header, const char *footer,
                             const unsigned char *der_data, size_t der_len,
                             unsigned char *buf, size_t buf_len, size_t *olen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    unsigned char *encode_buf = NULL, *c, *p = buf;
    size_t len = 0, use_len, add_len = 0;

    mbedtls_base64_encode(NULL, 0, &use_len, der_data, der_len);
    add_len = strlen(header) + strlen(footer) + (((use_len > 2) ? (use_len - 2) : 0) / 64) + 1;

    if (use_len + add_len > buf_len) {
        *olen = use_len + add_len;
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    if (use_len != 0 &&
        ((encode_buf = mbedtls_calloc(1, use_len)) == NULL)) {
        return MBEDTLS_ERR_PEM_ALLOC_FAILED;
    }

    if ((ret = mbedtls_base64_encode(encode_buf, use_len, &use_len, der_data,
                                     der_len)) != 0) {
        mbedtls_free(encode_buf);
        return ret;
    }

    memcpy(p, header, strlen(header));
    p += strlen(header);
    c = encode_buf;

    while (use_len) {
        len = (use_len > 64) ? 64 : use_len;
        memcpy(p, c, len);
        use_len -= len;
        p += len;
        c += len;
        *p++ = '\n';
    }

    memcpy(p, footer, strlen(footer));
    p += strlen(footer);

    *p++ = '\0';
    *olen = (size_t) (p - buf);

    /* Clean any remaining data previously written to the buffer */
    memset(buf + *olen, 0, buf_len - *olen);

    mbedtls_free(encode_buf);
    return 0;
}
#endif /* MBEDTLS_PEM_WRITE_C */
#endif /* MBEDTLS_PEM_PARSE_C || MBEDTLS_PEM_WRITE_C */
