/*
 *  PSA PAKE layer on top of Mbed TLS software crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "tf_psa_crypto_common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include <psa/crypto.h>
#include "psa_crypto_core.h"
#include "psa_crypto_pake.h"
#include "psa_crypto_slot_management.h"

#include <mbedtls/private/ecjpake.h>
#include "psa_util_internal.h"

#include <mbedtls/platform.h>
#include <mbedtls/private/error_common.h>
#include <string.h>

/*
 * State sequence:
 *
 *   psa_pake_setup()
 *   |
 *   |-- In any order:
 *   |   | psa_pake_set_user()
 *   |   | psa_pake_set_peer()
 *   |   | psa_pake_set_role()
 *   |
 *   |--- In any order: (First round input before or after first round output)
 *   |   |
 *   |   |------ In Order
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_KEY_SHARE)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PROOF)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_KEY_SHARE)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PROOF)
 *   |   |
 *   |   |------ In Order:
 *   |           | psa_pake_input(PSA_PAKE_STEP_KEY_SHARE)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PROOF)
 *   |           | psa_pake_input(PSA_PAKE_STEP_KEY_SHARE)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PROOF)
 *   |
 *   |--- In any order: (Second round input before or after second round output)
 *   |   |
 *   |   |------ In Order
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_KEY_SHARE)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |   |       | psa_pake_output(PSA_PAKE_STEP_ZK_PROOF)
 *   |   |
 *   |   |------ In Order:
 *   |           | psa_pake_input(PSA_PAKE_STEP_KEY_SHARE)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PUBLIC)
 *   |           | psa_pake_input(PSA_PAKE_STEP_ZK_PROOF)
 *   |
 *   psa_pake_get_shared_key()
 *   psa_pake_abort()
 */

/*
 * Possible sequence of calls to implementation:
 *
 * |--- In any order:
 * |   |
 * |   |------ In Order
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X1_STEP_KEY_SHARE)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X1_STEP_ZK_PUBLIC)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X1_STEP_ZK_PROOF)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2_STEP_KEY_SHARE)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2_STEP_ZK_PUBLIC)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2_STEP_ZK_PROOF)
 * |   |
 * |   |------ In Order:
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X1_STEP_KEY_SHARE)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X1_STEP_ZK_PUBLIC)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X1_STEP_ZK_PROOF)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X2_STEP_KEY_SHARE)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X2_STEP_ZK_PUBLIC)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X2_STEP_ZK_PROOF)
 * |
 * |--- In any order:
 * |   |
 * |   |------ In Order
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2S_STEP_KEY_SHARE)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2S_STEP_ZK_PUBLIC)
 * |   |       | mbedtls_psa_pake_output(PSA_JPAKE_X2S_STEP_ZK_PROOF)
 * |   |
 * |   |------ In Order:
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X4S_STEP_KEY_SHARE)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X4S_STEP_ZK_PUBLIC)
 * |           | mbedtls_psa_pake_input(PSA_JPAKE_X4S_STEP_ZK_PROOF)
 */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
static psa_status_t mbedtls_ecjpake_to_psa_error(int ret)
{
    /* Only legacy error codes need to be translated.
     * Those are either a low-level error code (-127..-2)
     * or a high-level error code (<= -0x1000). */
    if (ret > -0x1000 && ret <= -0x80) {
        return (psa_status_t) ret;
    }
    switch (ret) {
        case MBEDTLS_ERR_ECP_INVALID_KEY:
            return PSA_ERROR_DATA_INVALID;
        case MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED:
            return PSA_ERROR_CORRUPTION_DETECTED;
        default:
            return PSA_ERROR_GENERIC_ERROR;
    }
}
#endif

#if defined(MBEDTLS_PSA_BUILTIN_PAKE)
#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
static psa_status_t psa_pake_ecjpake_setup(mbedtls_psa_pake_operation_t *operation)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

    mbedtls_ecjpake_init(&operation->ctx.jpake);

    ret = mbedtls_ecjpake_setup(&operation->ctx.jpake,
                                operation->role,
                                MBEDTLS_MD_SHA256,
                                MBEDTLS_ECP_DP_SECP256R1,
                                operation->password,
                                operation->password_len);

    mbedtls_platform_zeroize(operation->password, operation->password_len);

    if (ret != 0) {
        return mbedtls_ecjpake_to_psa_error(ret);
    }

    return PSA_SUCCESS;
}
#endif

/* The only two JPAKE user/peer identifiers supported in built-in implementation. */
static const uint8_t jpake_server_id[] = { 's', 'e', 'r', 'v', 'e', 'r' };
static const uint8_t jpake_client_id[] = { 'c', 'l', 'i', 'e', 'n', 't' };

psa_status_t mbedtls_psa_pake_setup(mbedtls_psa_pake_operation_t *operation,
                                    const psa_crypto_driver_pake_inputs_t *inputs)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t user_len = 0, peer_len = 0, password_len = 0;
    uint8_t *peer = NULL, *user = NULL;
    size_t actual_user_len = 0, actual_peer_len = 0, actual_password_len = 0;
    psa_pake_cipher_suite_t cipher_suite = psa_pake_cipher_suite_init();

    status = psa_crypto_driver_pake_get_password_len(inputs, &password_len);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_crypto_driver_pake_get_user_len(inputs, &user_len);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_crypto_driver_pake_get_peer_len(inputs, &peer_len);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_crypto_driver_pake_get_cipher_suite(inputs, &cipher_suite);
    if (status != PSA_SUCCESS) {
        return status;
    }

    operation->password = mbedtls_calloc(1, password_len);
    if (operation->password == NULL) {
        status = PSA_ERROR_INSUFFICIENT_MEMORY;
        goto error;
    }

    user = mbedtls_calloc(1, user_len);
    if (user == NULL) {
        status = PSA_ERROR_INSUFFICIENT_MEMORY;
        goto error;
    }

    peer = mbedtls_calloc(1, peer_len);
    if (peer == NULL) {
        status = PSA_ERROR_INSUFFICIENT_MEMORY;
        goto error;
    }

    status = psa_crypto_driver_pake_get_password(inputs, operation->password,
                                                 password_len, &actual_password_len);
    if (status != PSA_SUCCESS) {
        goto error;
    }

    status = psa_crypto_driver_pake_get_user(inputs, user,
                                             user_len, &actual_user_len);
    if (status != PSA_SUCCESS) {
        goto error;
    }

    status = psa_crypto_driver_pake_get_peer(inputs, peer,
                                             peer_len, &actual_peer_len);
    if (status != PSA_SUCCESS) {
        goto error;
    }

    operation->password_len = actual_password_len;
    operation->alg = cipher_suite.algorithm;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    if (PSA_ALG_IS_JPAKE(cipher_suite.algorithm)) {
        if (cipher_suite.type != PSA_PAKE_PRIMITIVE_TYPE_ECC ||
            cipher_suite.family != PSA_ECC_FAMILY_SECP_R1 ||
            cipher_suite.bits != 256 ||
            PSA_ALG_GET_HASH(cipher_suite.algorithm) != PSA_ALG_SHA_256) {
            status = PSA_ERROR_NOT_SUPPORTED;
            goto error;
        }

        const size_t user_peer_len = sizeof(jpake_client_id); // client and server have the same length
        if (actual_user_len != user_peer_len ||
            actual_peer_len != user_peer_len) {
            status = PSA_ERROR_NOT_SUPPORTED;
            goto error;
        }

        if (memcmp(user, jpake_client_id, actual_user_len) == 0 &&
            memcmp(peer, jpake_server_id, actual_peer_len) == 0) {
            operation->role = MBEDTLS_ECJPAKE_CLIENT;
        } else
        if (memcmp(user, jpake_server_id, actual_user_len) == 0 &&
            memcmp(peer, jpake_client_id, actual_peer_len) == 0) {
            operation->role = MBEDTLS_ECJPAKE_SERVER;
        } else {
            status = PSA_ERROR_NOT_SUPPORTED;
            goto error;
        }

        operation->buffer_length = 0;
        operation->buffer_offset = 0;

        status = psa_pake_ecjpake_setup(operation);
        if (status != PSA_SUCCESS) {
            goto error;
        }

        /* Role has been set, release user/peer buffers. */
        mbedtls_free(user); mbedtls_free(peer);

        return PSA_SUCCESS;
    } else
#else
    (void) operation;
    (void) inputs;
#endif
    { status = PSA_ERROR_NOT_SUPPORTED; }

error:
    mbedtls_free(user); mbedtls_free(peer);
    /* In case of failure of the setup of a multipart operation, the PSA driver interface
     * specifies that the core does not call any other driver entry point thus does not
     * call mbedtls_psa_pake_abort(). Therefore call it here to do the needed clean
     * up like freeing the memory that may have been allocated to store the password.
     */
    mbedtls_psa_pake_abort(operation);
    return status;
}

static psa_status_t mbedtls_psa_pake_output_internal(
    mbedtls_psa_pake_operation_t *operation,
    psa_crypto_driver_pake_step_t step,
    uint8_t *output,
    size_t output_size,
    size_t *output_length)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t length;
    (void) step; // Unused parameter

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    /*
     * The PSA CRYPTO PAKE and Mbed TLS JPAKE API have a different
     * handling of output sequencing.
     *
     * The Mbed TLS JPAKE API outputs the whole X1+X2 and X2S steps data
     * at once, on the other side the PSA CRYPTO PAKE api requires
     * the KEY_SHARE/ZP_PUBLIC/ZK_PROOF parts of X1, X2 & X2S to be
     * retrieved in sequence.
     *
     * In order to achieve API compatibility, the whole X1+X2 or X2S steps
     * data is stored in an intermediate buffer at first step output call,
     * and data is sliced down by parsing the ECPoint records in order
     * to return the right parts on each step.
     */
    if (PSA_ALG_IS_JPAKE(operation->alg)) {
        /* Initialize & write round on KEY_SHARE sequences */
        if (step == PSA_JPAKE_X1_STEP_KEY_SHARE) {
            ret = mbedtls_ecjpake_write_round_one(&operation->ctx.jpake,
                                                  operation->buffer,
                                                  sizeof(operation->buffer),
                                                  &operation->buffer_length,
                                                  mbedtls_psa_get_random,
                                                  MBEDTLS_PSA_RANDOM_STATE);
            if (ret != 0) {
                return mbedtls_ecjpake_to_psa_error(ret);
            }

            operation->buffer_offset = 0;
        } else if (step == PSA_JPAKE_X2S_STEP_KEY_SHARE) {
            ret = mbedtls_ecjpake_write_round_two(&operation->ctx.jpake,
                                                  operation->buffer,
                                                  sizeof(operation->buffer),
                                                  &operation->buffer_length,
                                                  mbedtls_psa_get_random,
                                                  MBEDTLS_PSA_RANDOM_STATE);
            if (ret != 0) {
                return mbedtls_ecjpake_to_psa_error(ret);
            }

            operation->buffer_offset = 0;
        }

        /*
         * mbedtls_ecjpake_write_round_xxx() outputs thing in the format
         * defined by draft-cragie-tls-ecjpake-01 section 7. The summary is
         * that the data for each step is prepended with a length byte, and
         * then they're concatenated. Additionally, the server's second round
         * output is prepended with a 3-bytes ECParameters structure.
         *
         * In PSA, we output each step separately, and don't prepend the
         * output with a length byte, even less a curve identifier, as that
         * information is already available.
         */
        if (step == PSA_JPAKE_X2S_STEP_KEY_SHARE &&
            operation->role == MBEDTLS_ECJPAKE_SERVER) {
            /* Skip ECParameters, with is 3 bytes (RFC 8422) */
            operation->buffer_offset += 3;
        }

        /* Read the length byte then move past it to the data */
        length = operation->buffer[operation->buffer_offset];
        operation->buffer_offset += 1;

        if (operation->buffer_offset + length > operation->buffer_length) {
            return PSA_ERROR_DATA_CORRUPT;
        }

        if (output_size < length) {
            return PSA_ERROR_BUFFER_TOO_SMALL;
        }

        memcpy(output,
               operation->buffer + operation->buffer_offset,
               length);
        *output_length = length;

        operation->buffer_offset += length;

        /* Reset buffer after ZK_PROOF sequence */
        if ((step == PSA_JPAKE_X2_STEP_ZK_PROOF) ||
            (step == PSA_JPAKE_X2S_STEP_ZK_PROOF)) {
            mbedtls_platform_zeroize(operation->buffer, sizeof(operation->buffer));
            operation->buffer_length = 0;
            operation->buffer_offset = 0;
        }

        return PSA_SUCCESS;
    } else
#else
    (void) step;
    (void) output;
    (void) output_size;
    (void) output_length;
#endif
    { return PSA_ERROR_NOT_SUPPORTED; }
}

psa_status_t mbedtls_psa_pake_output(mbedtls_psa_pake_operation_t *operation,
                                     psa_crypto_driver_pake_step_t step,
                                     uint8_t *output,
                                     size_t output_size,
                                     size_t *output_length)
{
    psa_status_t status = mbedtls_psa_pake_output_internal(
        operation, step, output, output_size, output_length);

    return status;
}

static psa_status_t mbedtls_psa_pake_input_internal(
    mbedtls_psa_pake_operation_t *operation,
    psa_crypto_driver_pake_step_t step,
    const uint8_t *input,
    size_t input_length)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    (void) step; // Unused parameter

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    /*
     * The PSA CRYPTO PAKE and Mbed TLS JPAKE API have a different
     * handling of input sequencing.
     *
     * The Mbed TLS JPAKE API takes the whole X1+X2 or X4S steps data
     * at once as input, on the other side the PSA CRYPTO PAKE api requires
     * the KEY_SHARE/ZP_PUBLIC/ZK_PROOF parts of X1, X2 & X4S to be
     * given in sequence.
     *
     * In order to achieve API compatibility, each X1+X2 or X4S step data
     * is stored sequentially in an intermediate buffer and given to the
     * Mbed TLS JPAKE API on the last step.
     *
     * This causes any input error to be only detected on the last step.
     */
    if (PSA_ALG_IS_JPAKE(operation->alg)) {
        /*
         * Copy input to local buffer and format it as the Mbed TLS API
         * expects, i.e. as defined by draft-cragie-tls-ecjpake-01 section 7.
         * The summary is that the data for each step is prepended with a
         * length byte, and then they're concatenated. Additionally, the
         * server's second round output is prepended with a 3-bytes
         * ECParameters structure - which means we have to prepend that when
         * we're a client.
         */
        if (step == PSA_JPAKE_X4S_STEP_KEY_SHARE &&
            operation->role == MBEDTLS_ECJPAKE_CLIENT) {
            /* We only support secp256r1. */
            /* This is the ECParameters structure defined by RFC 8422. */
            unsigned char ecparameters[3] = {
                3, /* named_curve */
                0, 23 /* secp256r1 */
            };

            if (operation->buffer_length + sizeof(ecparameters) >
                sizeof(operation->buffer)) {
                return PSA_ERROR_BUFFER_TOO_SMALL;
            }

            memcpy(operation->buffer + operation->buffer_length,
                   ecparameters, sizeof(ecparameters));
            operation->buffer_length += sizeof(ecparameters);
        }

        /*
         * The core checks that input_length is smaller than
         * PSA_PAKE_INPUT_MAX_SIZE.
         * Thus no risk of integer overflow here.
         */
        if (operation->buffer_length + input_length + 1 > sizeof(operation->buffer)) {
            return PSA_ERROR_BUFFER_TOO_SMALL;
        }

        /* Write the length byte */
        operation->buffer[operation->buffer_length] = (uint8_t) input_length;
        operation->buffer_length += 1;

        /* Finally copy the data */
        memcpy(operation->buffer + operation->buffer_length,
               input, input_length);
        operation->buffer_length += input_length;

        /* Load buffer at each last round ZK_PROOF */
        if (step == PSA_JPAKE_X2_STEP_ZK_PROOF) {
            ret = mbedtls_ecjpake_read_round_one(&operation->ctx.jpake,
                                                 operation->buffer,
                                                 operation->buffer_length);

            mbedtls_platform_zeroize(operation->buffer, sizeof(operation->buffer));
            operation->buffer_length = 0;

            if (ret != 0) {
                return mbedtls_ecjpake_to_psa_error(ret);
            }
        } else if (step == PSA_JPAKE_X4S_STEP_ZK_PROOF) {
            ret = mbedtls_ecjpake_read_round_two(&operation->ctx.jpake,
                                                 operation->buffer,
                                                 operation->buffer_length);

            mbedtls_platform_zeroize(operation->buffer, sizeof(operation->buffer));
            operation->buffer_length = 0;

            if (ret != 0) {
                return mbedtls_ecjpake_to_psa_error(ret);
            }
        }

        return PSA_SUCCESS;
    } else
#else
    (void) step;
    (void) input;
    (void) input_length;
#endif
    { return PSA_ERROR_NOT_SUPPORTED; }
}

psa_status_t mbedtls_psa_pake_input(mbedtls_psa_pake_operation_t *operation,
                                    psa_crypto_driver_pake_step_t step,
                                    const uint8_t *input,
                                    size_t input_length)
{
    psa_status_t status = mbedtls_psa_pake_input_internal(
        operation, step, input, input_length);

    return status;
}

psa_status_t mbedtls_psa_pake_get_implicit_key(
    mbedtls_psa_pake_operation_t *operation,
    uint8_t *output, size_t output_size,
    size_t *output_length)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    if (PSA_ALG_IS_JPAKE(operation->alg)) {
        ret = mbedtls_ecjpake_write_shared_key(&operation->ctx.jpake,
                                               output,
                                               output_size,
                                               output_length,
                                               mbedtls_psa_get_random,
                                               MBEDTLS_PSA_RANDOM_STATE);
        if (ret != 0) {
            return mbedtls_ecjpake_to_psa_error(ret);
        }

        return PSA_SUCCESS;
    } else
#else
    (void) output;
#endif
    { return PSA_ERROR_NOT_SUPPORTED; }
}

psa_status_t mbedtls_psa_pake_abort(mbedtls_psa_pake_operation_t *operation)
{
    mbedtls_zeroize_and_free(operation->password, operation->password_len);
    operation->password = NULL;
    operation->password_len = 0;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_JPAKE)
    if (PSA_ALG_IS_JPAKE(operation->alg)) {
        operation->role = MBEDTLS_ECJPAKE_NONE;
        mbedtls_platform_zeroize(operation->buffer, sizeof(operation->buffer));
        operation->buffer_length = 0;
        operation->buffer_offset = 0;
        mbedtls_ecjpake_free(&operation->ctx.jpake);
    }
#endif

    operation->alg = PSA_ALG_NONE;

    return PSA_SUCCESS;
}

#endif /* MBEDTLS_PSA_BUILTIN_PAKE */

#endif /* MBEDTLS_PSA_CRYPTO_C */
