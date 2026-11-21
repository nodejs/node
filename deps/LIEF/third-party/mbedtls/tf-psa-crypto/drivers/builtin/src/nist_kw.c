/*
 *  Implementation of NIST SP 800-38F key wrapping, supporting KW and KWP modes
 *  only
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
/*
 * Definition of Key Wrapping:
 * https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-38F.pdf
 * RFC 3394 "Advanced Encryption Standard (AES) Key Wrap Algorithm"
 * RFC 5649 "Advanced Encryption Standard (AES) Key Wrap with Padding Algorithm"
 *
 * Note: RFC 3394 defines different methodology for intermediate operations for
 * the wrapping and unwrapping operation than the definition in NIST SP 800-38F.
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_NIST_KW_C)

#include "mbedtls/nist_kw.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/private/error_common.h"
#include "mbedtls/constant_time.h"
#include "constant_time_internal.h"

#include <stdint.h>
#include <string.h>

#include "mbedtls/platform.h"
#include "psa/crypto.h"

#define KW_SEMIBLOCK_LENGTH    8
#define MIN_SEMIBLOCKS_COUNT   3

/*! The 64-bit default integrity check value (ICV) for KW mode. */
static const unsigned char NIST_KW_ICV1[] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
/*! The 32-bit default integrity check value (ICV) for KWP mode. */
static const  unsigned char NIST_KW_ICV2[] = { 0xA6, 0x59, 0x59, 0xA6 };

/*
 * Helper function for Xoring the uint64_t "t" with the encrypted A.
 * Defined in NIST SP 800-38F section 6.1
 */
static void calc_a_xor_t(unsigned char A[KW_SEMIBLOCK_LENGTH], uint64_t t)
{
    size_t i = 0;
    for (i = 0; i < sizeof(t); i++) {
        A[i] ^= (t >> ((sizeof(t) - 1 - i) * 8)) & 0xff;
    }
}

static int verify_input(mbedtls_svc_key_id_t *key)
{
    int ret = PSA_SUCCESS;

    psa_key_attributes_t attributes;
    ret = psa_get_key_attributes(*key, &attributes);

    if (ret == PSA_SUCCESS) {

        /*
         * Currently NIST KW only supports PSA_KEY_TYPE_AES, so verify this is
         * set in the key attributes.
         */

        if (psa_get_key_type(&attributes) != PSA_KEY_TYPE_AES) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    psa_reset_key_attributes(&attributes);

    return ret;
}

/*
 * KW-AE as defined in SP 800-38F section 6.2
 * KWP-AE as defined in SP 800-38F section 6.3
 */
psa_status_t mbedtls_nist_kw_wrap(mbedtls_svc_key_id_t key,
                                  mbedtls_nist_kw_mode_t mode,
                                  const unsigned char *input, size_t input_length,
                                  unsigned char *output, size_t output_size, size_t *output_length)
{
    psa_status_t ret = 0;
    size_t semiblocks = 0, s, olen, padlen = 0, update_output_length, finish_output_length;
    uint64_t t = 0;
    unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char inbuff[KW_SEMIBLOCK_LENGTH * 2];
    psa_cipher_operation_t wrap_operation = PSA_CIPHER_OPERATION_INIT;
    *output_length = 0;

    ret = verify_input(&key);
    if (ret != PSA_SUCCESS) {
        goto cleanup;
    }

    ret = psa_cipher_encrypt_setup(&wrap_operation, key, PSA_ALG_ECB_NO_PADDING);
    if (ret != PSA_SUCCESS) {
        goto cleanup;
    }

    /*
     * Generate the String to work on
     */
    if (mode == MBEDTLS_KW_MODE_KW) {
        if (output_size < input_length + KW_SEMIBLOCK_LENGTH) {
            ret = PSA_ERROR_BUFFER_TOO_SMALL;
            goto cleanup;
        }

        /*
         * According to SP 800-38F Table 1, the plaintext length for KW
         * must be between 2 to 2^54-1 semiblocks inclusive.
         */
        if (input_length < 16 ||
#if SIZE_MAX > 0x1FFFFFFFFFFFFF8
            input_length > 0x1FFFFFFFFFFFFF8 ||
#endif
            input_length % KW_SEMIBLOCK_LENGTH != 0) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }

        memcpy(output, NIST_KW_ICV1, KW_SEMIBLOCK_LENGTH);
        memmove(output + KW_SEMIBLOCK_LENGTH, input, input_length);
    } else { //MBEDTLS_KW_MODE_KWP
        if (input_length % 8 != 0) {
            padlen = (8 - (input_length % 8));
        }

        if (output_size < input_length + KW_SEMIBLOCK_LENGTH + padlen) {
            ret = PSA_ERROR_BUFFER_TOO_SMALL;
            goto cleanup;
        }

        /*
         * According to SP 800-38F Table 1, the plaintext length for KWP
         * must be between 1 and 2^32-1 octets inclusive.
         */
        if (input_length < 1
#if SIZE_MAX > 0xFFFFFFFF
            || input_length > 0xFFFFFFFF
#endif
            ) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }

        memcpy(output, NIST_KW_ICV2, KW_SEMIBLOCK_LENGTH / 2);
        MBEDTLS_PUT_UINT32_BE((input_length & 0xffffffff), output,
                              KW_SEMIBLOCK_LENGTH / 2);

        memcpy(output + KW_SEMIBLOCK_LENGTH, input, input_length);
        memset(output + KW_SEMIBLOCK_LENGTH + input_length, 0, padlen);
    }
    semiblocks = ((input_length + padlen) / KW_SEMIBLOCK_LENGTH) + 1;

    s = 6 * (semiblocks - 1);

    if (mode == MBEDTLS_KW_MODE_KWP
        && input_length <= KW_SEMIBLOCK_LENGTH) {
        memcpy(inbuff, output, 16);
        ret = psa_cipher_update(&wrap_operation,
                                inbuff, 16, output, output_size, &update_output_length);
        if (ret != PSA_SUCCESS) {
            goto cleanup;
        }
        ret = psa_cipher_finish(&wrap_operation,
                                mbedtls_buffer_offset(output, update_output_length),
                                output_size - update_output_length,
                                &finish_output_length);
        if (ret != PSA_SUCCESS) {
            goto cleanup;
        }
        *output_length = update_output_length + finish_output_length;
    } else {
        unsigned char *R2 = output + KW_SEMIBLOCK_LENGTH;
        unsigned char *A = output;

        /*
         * Do the wrapping function W, as defined in RFC 3394 section 2.2.1
         */
        if (semiblocks < MIN_SEMIBLOCKS_COUNT) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }

        /* Calculate intermediate values */
        for (t = 1; t <= s; t++) {
            memcpy(inbuff, A, KW_SEMIBLOCK_LENGTH);
            memcpy(inbuff + KW_SEMIBLOCK_LENGTH, R2, KW_SEMIBLOCK_LENGTH);

            ret = psa_cipher_update(&wrap_operation,
                                    inbuff, 16, outbuff, sizeof(outbuff), &olen);
            if (ret != PSA_SUCCESS) {
                goto cleanup;
            }

            memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);
            calc_a_xor_t(A, t);

            memcpy(R2, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);
            R2 += KW_SEMIBLOCK_LENGTH;
            if (R2 >= output + (semiblocks * KW_SEMIBLOCK_LENGTH)) {
                R2 = output + KW_SEMIBLOCK_LENGTH;
            }
        }
        if (olen != 16) {
            ret = PSA_ERROR_CORRUPTION_DETECTED;
            goto cleanup;
        }
    }

    *output_length = semiblocks * KW_SEMIBLOCK_LENGTH;

cleanup:

    if (ret != PSA_SUCCESS && output != NULL) {
        memset(output, 0, output_size);
    }
    psa_cipher_abort(&wrap_operation);
    mbedtls_platform_zeroize(inbuff, KW_SEMIBLOCK_LENGTH * 2);
    mbedtls_platform_zeroize(outbuff, KW_SEMIBLOCK_LENGTH * 2);

    return ret;
}

/*
 * W-1 function as defined in RFC 3394 section 2.2.2
 * This function assumes the following:
 * 1. Output buffer is at least of size ( semiblocks - 1 ) * KW_SEMIBLOCK_LENGTH.
 * 2. The input buffer is of size semiblocks * KW_SEMIBLOCK_LENGTH.
 * 3. Minimal number of semiblocks is 3.
 * 4. A is a buffer to hold the first semiblock of the input buffer.
 */
static int unwrap(const unsigned char *input, size_t semiblocks,
                  unsigned char A[KW_SEMIBLOCK_LENGTH],
                  unsigned char *output, size_t *output_length, psa_cipher_operation_t *operation)
{
    psa_status_t ret = 0;
    const size_t s = 6 * (semiblocks - 1);
    size_t part_length;
    uint64_t t = 0;
    unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char inbuff[KW_SEMIBLOCK_LENGTH * 2];
    unsigned char *R = NULL;
    *output_length = 0;

    if (semiblocks < MIN_SEMIBLOCKS_COUNT) {
        ret = PSA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    memcpy(A, input, KW_SEMIBLOCK_LENGTH);
    memmove(output, input + KW_SEMIBLOCK_LENGTH, (semiblocks - 1) * KW_SEMIBLOCK_LENGTH);
    R = output + (semiblocks - 2) * KW_SEMIBLOCK_LENGTH;

    /* Calculate intermediate values */
    for (t = s; t >= 1; t--) {
        calc_a_xor_t(A, t);

        memcpy(inbuff, A, KW_SEMIBLOCK_LENGTH);
        memcpy(inbuff + KW_SEMIBLOCK_LENGTH, R, KW_SEMIBLOCK_LENGTH);

        ret = psa_cipher_update(operation,
                                inbuff, 16, outbuff, sizeof(outbuff), output_length);
        if (ret != PSA_SUCCESS) {
            goto cleanup;
        }

        memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);

        /* Set R as LSB64 of outbuff */
        memcpy(R, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);

        if (R == output) {
            R = output + (semiblocks - 2) * KW_SEMIBLOCK_LENGTH;
        } else {
            R -= KW_SEMIBLOCK_LENGTH;
        }
    }

    ret = psa_cipher_finish(operation,
                            outbuff + *output_length,
                            sizeof(outbuff) - *output_length,
                            &part_length);
    if (ret != PSA_SUCCESS) {
        goto cleanup;
    }
    *output_length = (semiblocks - 1) * KW_SEMIBLOCK_LENGTH;

cleanup:
    if (ret != PSA_SUCCESS) {
        memset(output, 0, (semiblocks - 1) * KW_SEMIBLOCK_LENGTH);
    }
    mbedtls_platform_zeroize(inbuff, sizeof(inbuff));
    mbedtls_platform_zeroize(outbuff, sizeof(outbuff));

    return ret;
}

/*
 * KW-AD as defined in SP 800-38F section 6.2
 * KWP-AD as defined in SP 800-38F section 6.3
 */
psa_status_t mbedtls_nist_kw_unwrap(mbedtls_svc_key_id_t key,
                                    mbedtls_nist_kw_mode_t mode,
                                    const unsigned char *input, size_t input_length,
                                    unsigned char *output, size_t output_size,
                                    size_t *output_length)
{
    psa_status_t ret = 0;
    unsigned char A[KW_SEMIBLOCK_LENGTH];
    int diff;
    size_t part_length, padlen = 0, Plen;
    psa_cipher_operation_t unwrap_operation = PSA_CIPHER_OPERATION_INIT;
    *output_length = 0;

    ret = verify_input(&key);
    if (ret != PSA_SUCCESS) {
        goto cleanup;
    }

    ret = psa_cipher_decrypt_setup(&unwrap_operation, key, PSA_ALG_ECB_NO_PADDING);
    if (ret != PSA_SUCCESS) {
        goto cleanup;
    }
    if (output_size < input_length - KW_SEMIBLOCK_LENGTH) {
        ret = PSA_ERROR_BUFFER_TOO_SMALL;
        goto cleanup;
    }

    if (mode == MBEDTLS_KW_MODE_KW) {
        /*
         * According to SP 800-38F Table 1, the ciphertext length for KW
         * must be between 3 to 2^54 semiblocks inclusive.
         */
        if (input_length < 24 ||
#if SIZE_MAX > 0x200000000000000
            input_length > 0x200000000000000 ||
#endif
            input_length % KW_SEMIBLOCK_LENGTH != 0) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }

        ret = unwrap(input, input_length / KW_SEMIBLOCK_LENGTH,
                     A, output, output_length, &unwrap_operation);
        if (ret != PSA_SUCCESS) {
            goto cleanup;
        }

        /* Check ICV in "constant-time" */
        diff = mbedtls_ct_memcmp(NIST_KW_ICV1, A, KW_SEMIBLOCK_LENGTH);

        if (diff != 0) {
            ret = PSA_ERROR_INVALID_SIGNATURE;
            goto cleanup;
        }

    } else if (mode == MBEDTLS_KW_MODE_KWP) {
        /*
         * According to SP 800-38F Table 1, the ciphertext length for KWP
         * must be between 2 to 2^29 semiblocks inclusive.
         */
        if (input_length < KW_SEMIBLOCK_LENGTH * 2 ||
#if SIZE_MAX > 0x100000000
            input_length > 0x100000000 ||
#endif
            input_length % KW_SEMIBLOCK_LENGTH != 0) {
            ret = PSA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }

        if (input_length == KW_SEMIBLOCK_LENGTH * 2) {
            unsigned char outbuff[KW_SEMIBLOCK_LENGTH * 2];
            ret = psa_cipher_update(&unwrap_operation,
                                    input, 16, outbuff, sizeof(outbuff), output_length);
            if (ret != PSA_SUCCESS) {
                goto cleanup;
            }
            ret = psa_cipher_finish(&unwrap_operation,
                                    outbuff + *output_length,
                                    sizeof(outbuff) - *output_length,
                                    &part_length);
            if (ret != PSA_SUCCESS) {
                goto cleanup;
            }

            memcpy(A, outbuff, KW_SEMIBLOCK_LENGTH);
            memcpy(output, outbuff + KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH);
            mbedtls_platform_zeroize(outbuff, sizeof(outbuff));
            *output_length = KW_SEMIBLOCK_LENGTH;
        } else {
            /* input_length >=  KW_SEMIBLOCK_LENGTH * 3 */
            ret = unwrap(input, input_length / KW_SEMIBLOCK_LENGTH,
                         A, output, output_length, &unwrap_operation);
            if (ret != PSA_SUCCESS) {
                goto cleanup;
            }
        }

        /* Check ICV in "constant-time" */
        diff = mbedtls_ct_memcmp(NIST_KW_ICV2, A, KW_SEMIBLOCK_LENGTH / 2);

        if (diff != 0) {
            ret = PSA_ERROR_INVALID_SIGNATURE;
        }

        Plen = MBEDTLS_GET_UINT32_BE(A, KW_SEMIBLOCK_LENGTH / 2);

        /*
         * Plen is the length of the plaintext, when the input is valid.
         * If Plen is larger than the plaintext and padding, padlen will be
         * larger than 8, because of the type wrap around.
         */
        padlen = input_length - KW_SEMIBLOCK_LENGTH - Plen;
        ret = mbedtls_ct_error_if(mbedtls_ct_uint_gt(padlen, 7),
                                  PSA_ERROR_INVALID_SIGNATURE, ret);
        padlen &= 7;

        /* Check padding in "constant-time" */
        const uint8_t zero[KW_SEMIBLOCK_LENGTH] = { 0 };
        diff = mbedtls_ct_memcmp_partial(
            &output[*output_length - KW_SEMIBLOCK_LENGTH], zero,
            KW_SEMIBLOCK_LENGTH, KW_SEMIBLOCK_LENGTH - padlen, 0);

        if (diff != 0) {
            ret = PSA_ERROR_INVALID_SIGNATURE;
        }

        if (ret != PSA_SUCCESS) {
            goto cleanup;
        }
        memset(output + Plen, 0, padlen);
        *output_length = Plen;
    } else {
        ret = PSA_ERROR_NOT_SUPPORTED;
        goto cleanup;
    }

cleanup:
    if (ret != PSA_SUCCESS  && output != NULL) {
        memset(output, 0, *output_length);
        *output_length = 0;
    }

    psa_cipher_abort(&unwrap_operation);
    mbedtls_platform_zeroize(&diff, sizeof(diff));
    mbedtls_platform_zeroize(A, sizeof(A));

    return ret;
}

#endif /* MBEDTLS_NIST_KW_C */
