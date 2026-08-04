/**
 * \file rsa.h
 *
 * \brief This file provides an API for the RSA public-key cryptosystem.
 *
 * The RSA public-key cryptosystem is defined in <em>Public-Key
 * Cryptography Standards (PKCS) #1 v1.5: RSA Encryption</em>
 * and <em>Public-Key Cryptography Standards (PKCS) #1 v2.1:
 * RSA Cryptography Specifications</em>.
 *
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#ifndef MBEDTLS_RSA_H
#define MBEDTLS_RSA_H
#include "mbedtls/private_access.h"

#include "mbedtls/build_info.h"

#include "mbedtls/bignum.h"
#include "mbedtls/md.h"

#if defined(MBEDTLS_THREADING_C)
#include "mbedtls/threading.h"
#endif

/*
 * RSA Error codes
 */
/** Bad input parameters to function. */
#define MBEDTLS_ERR_RSA_BAD_INPUT_DATA                    -0x4080
/** Input data contains invalid padding and is rejected. */
#define MBEDTLS_ERR_RSA_INVALID_PADDING                   -0x4100
/** Something failed during generation of a key. */
#define MBEDTLS_ERR_RSA_KEY_GEN_FAILED                    -0x4180
/** Key failed to pass the validity check of the library. */
#define MBEDTLS_ERR_RSA_KEY_CHECK_FAILED                  -0x4200
/** The public key operation failed. */
#define MBEDTLS_ERR_RSA_PUBLIC_FAILED                     -0x4280
/** The private key operation failed. */
#define MBEDTLS_ERR_RSA_PRIVATE_FAILED                    -0x4300
/** The PKCS#1 verification failed. */
#define MBEDTLS_ERR_RSA_VERIFY_FAILED                     -0x4380
/** The output buffer for decryption is not large enough. */
#define MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE                  -0x4400
/** The random generator failed to generate non-zeros. */
#define MBEDTLS_ERR_RSA_RNG_FAILED                        -0x4480

/*
 * RSA constants
 */

#define MBEDTLS_RSA_PKCS_V15    0 /**< Use PKCS#1 v1.5 encoding. */
#define MBEDTLS_RSA_PKCS_V21    1 /**< Use PKCS#1 v2.1 encoding. */

#define MBEDTLS_RSA_SIGN        1 /**< Identifier for RSA signature operations. */
#define MBEDTLS_RSA_CRYPT       2 /**< Identifier for RSA encryption and decryption operations. */

#define MBEDTLS_RSA_SALT_LEN_ANY    -1

/*
 * The above constants may be used even if the RSA module is compile out,
 * eg for alternative (PKCS#11) RSA implementations in the PK layers.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(MBEDTLS_RSA_ALT)
// Regular implementation
//

#if !defined(MBEDTLS_RSA_GEN_KEY_MIN_BITS)
#define MBEDTLS_RSA_GEN_KEY_MIN_BITS 1024
#elif MBEDTLS_RSA_GEN_KEY_MIN_BITS < 128
#error "MBEDTLS_RSA_GEN_KEY_MIN_BITS must be at least 128 bits"
#endif

/**
 * \brief   The RSA context structure.
 */
typedef struct mbedtls_rsa_context {
    int MBEDTLS_PRIVATE(ver);                    /*!<  Reserved for internal purposes.
                                                  *    Do not set this field in application
                                                  *    code. Its meaning might change without
                                                  *    notice. */
    size_t MBEDTLS_PRIVATE(len);                 /*!<  The size of \p N in Bytes. */

    mbedtls_mpi MBEDTLS_PRIVATE(N);              /*!<  The public modulus. */
    mbedtls_mpi MBEDTLS_PRIVATE(E);              /*!<  The public exponent. */

    mbedtls_mpi MBEDTLS_PRIVATE(D);              /*!<  The private exponent. */
    mbedtls_mpi MBEDTLS_PRIVATE(P);              /*!<  The first prime factor. */
    mbedtls_mpi MBEDTLS_PRIVATE(Q);              /*!<  The second prime factor. */

    mbedtls_mpi MBEDTLS_PRIVATE(DP);             /*!<  <code>D % (P - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(DQ);             /*!<  <code>D % (Q - 1)</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(QP);             /*!<  <code>1 / (Q % P)</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RN);             /*!<  cached <code>R^2 mod N</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(RP);             /*!<  cached <code>R^2 mod P</code>. */
    mbedtls_mpi MBEDTLS_PRIVATE(RQ);             /*!<  cached <code>R^2 mod Q</code>. */

    mbedtls_mpi MBEDTLS_PRIVATE(Vi);             /*!<  The cached blinding value. */
    mbedtls_mpi MBEDTLS_PRIVATE(Vf);             /*!<  The cached un-blinding value. */

    int MBEDTLS_PRIVATE(padding);                /*!< Selects padding mode:
                                                  #MBEDTLS_RSA_PKCS_V15 for 1.5 padding and
                                                  #MBEDTLS_RSA_PKCS_V21 for OAEP or PSS. */
    int MBEDTLS_PRIVATE(hash_id);                /*!< Hash identifier of mbedtls_md_type_t type,
                                                    as specified in md.h for use in the MGF
                                                    mask generating function used in the
                                                    EME-OAEP and EMSA-PSS encodings. */
#if defined(MBEDTLS_THREADING_C)
    /* Invariant: the mutex is initialized iff ver != 0. */
    mbedtls_threading_mutex_t MBEDTLS_PRIVATE(mutex);    /*!<  Thread-safety mutex. */
#endif
}
mbedtls_rsa_context;

#else  /* MBEDTLS_RSA_ALT */
#include "rsa_alt.h"
#endif /* MBEDTLS_RSA_ALT */

/**
 * \brief          This function initializes an RSA context.
 *
 * \note           This function initializes the padding and the hash
 *                 identifier to respectively #MBEDTLS_RSA_PKCS_V15 and
 *                 #MBEDTLS_MD_NONE. See mbedtls_rsa_set_padding() for more
 *                 information about those parameters.
 *
 * \param ctx      The RSA context to initialize. This must not be \c NULL.
 */
void mbedtls_rsa_init(mbedtls_rsa_context *ctx);

/**
 * \brief          This function sets padding for an already initialized RSA
 *                 context.
 *
 * \note           Set padding to #MBEDTLS_RSA_PKCS_V21 for the RSAES-OAEP
 *                 encryption scheme and the RSASSA-PSS signature scheme.
 *
 * \note           The \p hash_id parameter is ignored when using
 *                 #MBEDTLS_RSA_PKCS_V15 padding.
 *
 * \note           The choice of padding mode is strictly enforced for private
 *                 key operations, since there might be security concerns in
 *                 mixing padding modes. For public key operations it is
 *                 a default value, which can be overridden by calling specific
 *                 \c mbedtls_rsa_rsaes_xxx or \c mbedtls_rsa_rsassa_xxx
 *                 functions.
 *
 * \note           The hash selected in \p hash_id is always used for OEAP
 *                 encryption. For PSS signatures, it is always used for
 *                 making signatures, but can be overridden for verifying them.
 *                 If set to #MBEDTLS_MD_NONE, it is always overridden.
 *
 * \param ctx      The initialized RSA context to be configured.
 * \param padding  The padding mode to use. This must be either
 *                 #MBEDTLS_RSA_PKCS_V15 or #MBEDTLS_RSA_PKCS_V21.
 * \param hash_id  The hash identifier for PSS or OAEP, if \p padding is
 *                 #MBEDTLS_RSA_PKCS_V21. #MBEDTLS_MD_NONE is accepted by this
 *                 function but may be not suitable for some operations.
 *                 Ignored if \p padding is #MBEDTLS_RSA_PKCS_V15.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_RSA_INVALID_PADDING failure:
 *                 \p padding or \p hash_id is invalid.
 */
int mbedtls_rsa_set_padding(mbedtls_rsa_context *ctx, int padding,
                            mbedtls_md_type_t hash_id);

/**
 * \brief          This function retrieves padding mode of initialized
 *                 RSA context.
 *
 * \param ctx      The initialized RSA context.
 *
 * \return         RSA padding mode.
 *
 */
int mbedtls_rsa_get_padding_mode(const mbedtls_rsa_context *ctx);

/**
 * \brief          This function retrieves hash identifier of mbedtls_md_type_t
 *                 type.
 *
 * \param ctx      The initialized RSA context.
 *
 * \return         Hash identifier of mbedtls_md_type_t type.
 *
 */
int mbedtls_rsa_get_md_alg(const mbedtls_rsa_context *ctx);

/**
 * \brief          This function imports a set of core parameters into an
 *                 RSA context.
 *
 * \note           This function can be called multiple times for successive
 *                 imports, if the parameters are not simultaneously present.
 *
 *                 Any sequence of calls to this function should be followed
 *                 by a call to mbedtls_rsa_complete(), which checks and
 *                 completes the provided information to a ready-for-use
 *                 public or private RSA key.
 *
 * \note           See mbedtls_rsa_complete() for more information on which
 *                 parameters are necessary to set up a private or public
 *                 RSA key.
 *
 * \note           The imported parameters are copied and need not be preserved
 *                 for the lifetime of the RSA context being set up.
 *
 * \param ctx      The initialized RSA context to store the parameters in.
 * \param N        The RSA modulus. This may be \c NULL.
 * \param P        The first prime factor of \p N. This may be \c NULL.
 * \param Q        The second prime factor of \p N. This may be \c NULL.
 * \param D        The private exponent. This may be \c NULL.
 * \param E        The public exponent. This may be \c NULL.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_rsa_import(mbedtls_rsa_context *ctx,
                       const mbedtls_mpi *N,
                       const mbedtls_mpi *P, const mbedtls_mpi *Q,
                       const mbedtls_mpi *D, const mbedtls_mpi *E);

/**
 * \brief          This function imports core RSA parameters, in raw big-endian
 *                 binary format, into an RSA context.
 *
 * \note           This function can be called multiple times for successive
 *                 imports, if the parameters are not simultaneously present.
 *
 *                 Any sequence of calls to this function should be followed
 *                 by a call to mbedtls_rsa_complete(), which checks and
 *                 completes the provided information to a ready-for-use
 *                 public or private RSA key.
 *
 * \note           See mbedtls_rsa_complete() for more information on which
 *                 parameters are necessary to set up a private or public
 *                 RSA key.
 *
 * \note           The imported parameters are copied and need not be preserved
 *                 for the lifetime of the RSA context being set up.
 *
 * \param ctx      The initialized RSA context to store the parameters in.
 * \param N        The RSA modulus. This may be \c NULL.
 * \param N_len    The Byte length of \p N; it is ignored if \p N == NULL.
 * \param P        The first prime factor of \p N. This may be \c NULL.
 * \param P_len    The Byte length of \p P; it is ignored if \p P == NULL.
 * \param Q        The second prime factor of \p N. This may be \c NULL.
 * \param Q_len    The Byte length of \p Q; it is ignored if \p Q == NULL.
 * \param D        The private exponent. This may be \c NULL.
 * \param D_len    The Byte length of \p D; it is ignored if \p D == NULL.
 * \param E        The public exponent. This may be \c NULL.
 * \param E_len    The Byte length of \p E; it is ignored if \p E == NULL.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 */
int mbedtls_rsa_import_raw(mbedtls_rsa_context *ctx,
                           unsigned char const *N, size_t N_len,
                           unsigned char const *P, size_t P_len,
                           unsigned char const *Q, size_t Q_len,
                           unsigned char const *D, size_t D_len,
                           unsigned char const *E, size_t E_len);

/**
 * \brief          This function completes an RSA context from
 *                 a set of imported core parameters.
 *
 *                 To setup an RSA public key, precisely \c N and \c E
 *                 must have been imported.
 *
 *                 To setup an RSA private key, sufficient information must
 *                 be present for the other parameters to be derivable.
 *
 *                 The default implementation supports the following:
 *                 <ul><li>Derive \c P, \c Q from \c N, \c D, \c E.</li>
 *                 <li>Derive \c N, \c D from \c P, \c Q, \c E.</li></ul>
 *                 Alternative implementations need not support these.
 *
 *                 If this function runs successfully, it guarantees that
 *                 the RSA context can be used for RSA operations without
 *                 the risk of failure or crash.
 *
 * \warning        This function need not perform consistency checks
 *                 for the imported parameters. In particular, parameters that
 *                 are not needed by the implementation might be silently
 *                 discarded and left unchecked. To check the consistency
 *                 of the key material, see mbedtls_rsa_check_privkey().
 *
 * \param ctx      The initialized RSA context holding imported parameters.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_RSA_BAD_INPUT_DATA if the attempted derivations
 *                 failed.
 *
 */
int mbedtls_rsa_complete(mbedtls_rsa_context *ctx);

/**
 * \brief          This function exports the core parameters of an RSA key.
 *
 *                 If this function runs successfully, the non-NULL buffers
 *                 pointed to by \p N, \p P, \p Q, \p D, and \p E are fully
 *                 written, with additional unused space filled leading by
 *                 zero Bytes.
 *
 *                 Possible reasons for returning
 *                 #MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED:<ul>
 *                 <li>An alternative RSA implementation is in use, which
 *                 stores the key externally, and either cannot or should
 *                 not export it into RAM.</li>
 *                 <li>A SW or HW implementation might not support a certain
 *                 deduction. For example, \p P, \p Q from \p N, \p D,
 *                 and \p E if the former are not part of the
 *                 implementation.</li></ul>
 *
 *                 If the function fails due to an unsupported operation,
 *                 the RSA context stays intact and remains usable.
 *
 * \param ctx      The initialized RSA context.
 * \param N        The MPI to hold the RSA modulus.
 *                 This may be \c NULL if this field need not be exported.
 * \param P        The MPI to hold the first prime factor of \p N.
 *                 This may be \c NULL if this field need not be exported.
 * \param Q        The MPI to hold the second prime factor of \p N.
 *                 This may be \c NULL if this field need not be exported.
 * \param D        The MPI to hold the private exponent.
 *                 This may be \c NULL if this field need not be exported.
 * \param E        The MPI to hold the public exponent.
 *                 This may be \c NULL if this field need not be exported.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED if exporting the
 *                 requested parameters cannot be done due to missing
 *                 functionality or because of security policies.
 * \return         A non-zero return code on any other failure.
 *
 */
int mbedtls_rsa_export(const mbedtls_rsa_context *ctx,
                       mbedtls_mpi *N, mbedtls_mpi *P, mbedtls_mpi *Q,
                       mbedtls_mpi *D, mbedtls_mpi *E);

/**
 * \brief          This function exports core parameters of an RSA key
 *                 in raw big-endian binary format.
 *
 *                 If this function runs successfully, the non-NULL buffers
 *                 pointed to by \p N, \p P, \p Q, \p D, and \p E are fully
 *                 written, with additional unused space filled leading by
 *                 zero Bytes.
 *
 *                 Possible reasons for returning
 *                 #MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED:<ul>
 *                 <li>An alternative RSA implementation is in use, which
 *                 stores the key externally, and either cannot or should
 *                 not export it into RAM.</li>
 *                 <li>A SW or HW implementation might not support a certain
 *                 deduction. For example, \p P, \p Q from \p N, \p D,
 *                 and \p E if the former are not part of the
 *                 implementation.</li></ul>
 *                 If the function fails due to an unsupported operation,
 *                 the RSA context stays intact and remains usable.
 *
 * \note           The length parameters are ignored if the corresponding
 *                 buffer pointers are NULL.
 *
 * \param ctx      The initialized RSA context.
 * \param N        The Byte array to store the RSA modulus,
 *                 or \c NULL if this field need not be exported.
 * \param N_len    The size of the buffer for the modulus.
 * \param P        The Byte array to hold the first prime factor of \p N,
 *                 or \c NULL if this field need not be exported.
 * \param P_len    The size of the buffer for the first prime factor.
 * \param Q        The Byte array to hold the second prime factor of \p N,
 *                 or \c NULL if this field need not be exported.
 * \param Q_len    The size of the buffer for the second prime factor.
 * \param D        The Byte array to hold the private exponent,
 *                 or \c NULL if this field need not be exported.
 * \param D_len    The size of the buffer for the private exponent.
 * \param E        The Byte array to hold the public exponent,
 *                 or \c NULL if this field need not be exported.
 * \param E_len    The size of the buffer for the public exponent.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED if exporting the
 *                 requested parameters cannot be done due to missing
 *                 functionality or because of security policies.
 * \return         A non-zero return code on any other failure.
 */
int mbedtls_rsa_export_raw(const mbedtls_rsa_context *ctx,
                           unsigned char *N, size_t N_len,
                           unsigned char *P, size_t P_len,
                           unsigned char *Q, size_t Q_len,
                           unsigned char *D, size_t D_len,
                           unsigned char *E, size_t E_len);

/**
 * \brief          This function exports CRT parameters of a private RSA key.
 *
 * \note           Alternative RSA implementations not using CRT-parameters
 *                 internally can implement this function based on
 *                 mbedtls_rsa_deduce_opt().
 *
 * \param ctx      The initialized RSA context.
 * \param DP       The MPI to hold \c D modulo `P-1`,
 *                 or \c NULL if it need not be exported.
 * \param DQ       The MPI to hold \c D modulo `Q-1`,
 *                 or \c NULL if it need not be exported.
 * \param QP       The MPI to hold modular inverse of \c Q modulo \c P,
 *                 or \c NULL if it need not be exported.
 *
 * \return         \c 0 on success.
 * \return         A non-zero error code on failure.
 *
 */
int mbedtls_rsa_export_crt(const mbedtls_rsa_context *ctx,
                           mbedtls_mpi *DP, mbedtls_mpi *DQ, mbedtls_mpi *QP);

/**
 * \brief          This function retrieves the length of the RSA modulus in bits.
 *
 * \param ctx      The initialized RSA context.
 *
 * \return         The length of the RSA modulus in bits.
 *
 */
size_t mbedtls_rsa_get_bitlen(const mbedtls_rsa_context *ctx);

/**
 * \brief          This function retrieves the length of RSA modulus in Bytes.
 *
 * \param ctx      The initialized RSA context.
 *
 * \return         The length of the RSA modulus in Bytes.
 *
 */
size_t mbedtls_rsa_get_len(const mbedtls_rsa_context *ctx);

/**
 * \brief          This function generates an RSA keypair.
 *
 * \note           mbedtls_rsa_init() must be called before this function,
 *                 to set up the RSA context.
 *
 * \param ctx      The initialized RSA context used to hold the key.
 * \param f_rng    The RNG function to be used for key generation.
 *                 This is mandatory and must not be \c NULL.
 * \param p_rng    The RNG context to be passed to \p f_rng.
 *                 This may be \c NULL if \p f_rng doesn't need a context.
 * \param nbits    The size of the public key in bits.
 * \param exponent The public exponent to use. For example, \c 65537.
 *                 This must be odd and greater than \c 1.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_gen_key(mbedtls_rsa_context *ctx,
                        mbedtls_f_rng_t *f_rng,
                        void *p_rng,
                        unsigned int nbits, int exponent);

/**
 * \brief          This function checks if a context contains at least an RSA
 *                 public key.
 *
 *                 If the function runs successfully, it is guaranteed that
 *                 enough information is present to perform an RSA public key
 *                 operation using mbedtls_rsa_public().
 *
 * \param ctx      The initialized RSA context to check.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 *
 */
int mbedtls_rsa_check_pubkey(const mbedtls_rsa_context *ctx);

/**
 * \brief      This function checks if a context contains an RSA private key
 *             and perform basic consistency checks.
 *
 * \note       The consistency checks performed by this function not only
 *             ensure that mbedtls_rsa_private() can be called successfully
 *             on the given context, but that the various parameters are
 *             mutually consistent with high probability, in the sense that
 *             mbedtls_rsa_public() and mbedtls_rsa_private() are inverses.
 *
 * \warning    This function should catch accidental misconfigurations
 *             like swapping of parameters, but it cannot establish full
 *             trust in neither the quality nor the consistency of the key
 *             material that was used to setup the given RSA context:
 *             <ul><li>Consistency: Imported parameters that are irrelevant
 *             for the implementation might be silently dropped. If dropped,
 *             the current function does not have access to them,
 *             and therefore cannot check them. See mbedtls_rsa_complete().
 *             If you want to check the consistency of the entire
 *             content of a PKCS1-encoded RSA private key, for example, you
 *             should use mbedtls_rsa_validate_params() before setting
 *             up the RSA context.
 *             Additionally, if the implementation performs empirical checks,
 *             these checks substantiate but do not guarantee consistency.</li>
 *             <li>Quality: This function is not expected to perform
 *             extended quality assessments like checking that the prime
 *             factors are safe. Additionally, it is the responsibility of the
 *             user to ensure the trustworthiness of the source of his RSA
 *             parameters, which goes beyond what is effectively checkable
 *             by the library.</li></ul>
 *
 * \param ctx  The initialized RSA context to check.
 *
 * \return     \c 0 on success.
 * \return     An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_check_privkey(const mbedtls_rsa_context *ctx);

/**
 * \brief          This function checks a public-private RSA key pair.
 *
 *                 It checks each of the contexts, and makes sure they match.
 *
 * \param pub      The initialized RSA context holding the public key.
 * \param prv      The initialized RSA context holding the private key.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_check_pub_priv(const mbedtls_rsa_context *pub,
                               const mbedtls_rsa_context *prv);

/**
 * \brief          This function performs an RSA public key operation.
 *
 * \param ctx      The initialized RSA context to use.
 * \param input    The input buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \note           This function does not handle message padding.
 *
 * \note           Make sure to set \p input[0] = 0 or ensure that
 *                 input is smaller than \c N.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_public(mbedtls_rsa_context *ctx,
                       const unsigned char *input,
                       unsigned char *output);

/**
 * \brief          This function performs an RSA private key operation.
 *
 * \note           Blinding is used if and only if a PRNG is provided.
 *
 * \note           If blinding is used, both the base of exponentiation
 *                 and the exponent are blinded, providing protection
 *                 against some side-channel attacks.
 *
 * \warning        It is deprecated and a security risk to not provide
 *                 a PRNG here and thereby prevent the use of blinding.
 *                 Future versions of the library may enforce the presence
 *                 of a PRNG.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function, used for blinding. It is mandatory.
 * \param p_rng    The RNG context to pass to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context.
 * \param input    The input buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 *
 */
int mbedtls_rsa_private(mbedtls_rsa_context *ctx,
                        mbedtls_f_rng_t *f_rng,
                        void *p_rng,
                        const unsigned char *input,
                        unsigned char *output);

/**
 * \brief          This function adds the message padding, then performs an RSA
 *                 operation.
 *
 *                 It is the generic wrapper for performing a PKCS#1 encryption
 *                 operation.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG to use. It is used for padding generation
 *                 and it is mandatory.
 * \param p_rng    The RNG context to be passed to \p f_rng. May be
 *                 \c NULL if \p f_rng doesn't need a context argument.
 * \param ilen     The length of the plaintext in Bytes.
 * \param input    The input data to encrypt. This must be a readable
 *                 buffer of size \p ilen Bytes. It may be \c NULL if
 *                 `ilen == 0`.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_pkcs1_encrypt(mbedtls_rsa_context *ctx,
                              mbedtls_f_rng_t *f_rng,
                              void *p_rng,
                              size_t ilen,
                              const unsigned char *input,
                              unsigned char *output);

/**
 * \brief          This function performs a PKCS#1 v1.5 encryption operation
 *                 (RSAES-PKCS1-v1_5-ENCRYPT).
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function to use. It is mandatory and used for
 *                 padding generation.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may
 *                 be \c NULL if \p f_rng doesn't need a context argument.
 * \param ilen     The length of the plaintext in Bytes.
 * \param input    The input data to encrypt. This must be a readable
 *                 buffer of size \p ilen Bytes. It may be \c NULL if
 *                 `ilen == 0`.
 * \param output   The output buffer. This must be a writable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsaes_pkcs1_v15_encrypt(mbedtls_rsa_context *ctx,
                                        mbedtls_f_rng_t *f_rng,
                                        void *p_rng,
                                        size_t ilen,
                                        const unsigned char *input,
                                        unsigned char *output);

/**
 * \brief            This function performs a PKCS#1 v2.1 OAEP encryption
 *                   operation (RSAES-OAEP-ENCRYPT).
 *
 * \note             The output buffer must be as large as the size
 *                   of ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \param ctx        The initialized RSA context to use.
 * \param f_rng      The RNG function to use. This is needed for padding
 *                   generation and is mandatory.
 * \param p_rng      The RNG context to be passed to \p f_rng. This may
 *                   be \c NULL if \p f_rng doesn't need a context argument.
 * \param label      The buffer holding the custom label to use.
 *                   This must be a readable buffer of length \p label_len
 *                   Bytes. It may be \c NULL if \p label_len is \c 0.
 * \param label_len  The length of the label in Bytes.
 * \param ilen       The length of the plaintext buffer \p input in Bytes.
 * \param input      The input data to encrypt. This must be a readable
 *                   buffer of size \p ilen Bytes. It may be \c NULL if
 *                   `ilen == 0`.
 * \param output     The output buffer. This must be a writable buffer
 *                   of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                   for an 2048-bit RSA modulus.
 *
 * \return           \c 0 on success.
 * \return           An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsaes_oaep_encrypt(mbedtls_rsa_context *ctx,
                                   mbedtls_f_rng_t *f_rng,
                                   void *p_rng,
                                   const unsigned char *label, size_t label_len,
                                   size_t ilen,
                                   const unsigned char *input,
                                   unsigned char *output);

/**
 * \brief          This function performs an RSA operation, then removes the
 *                 message padding.
 *
 *                 It is the generic wrapper for performing a PKCS#1 decryption
 *                 operation.
 *
 * \warning        When \p ctx->padding is set to #MBEDTLS_RSA_PKCS_V15,
 *                 mbedtls_rsa_rsaes_pkcs1_v15_decrypt() is called, which is an
 *                 inherently dangerous function (CWE-242).
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size \p ctx->len of \p ctx->N (for example,
 *                 128 Bytes if RSA-1024 is used) to be able to hold an
 *                 arbitrary decrypted message. If it is not large enough to
 *                 hold the decryption of the particular ciphertext provided,
 *                 the function returns \c MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function. This is used for blinding and is
 *                 mandatory; see mbedtls_rsa_private() for more.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be
 *                 \c NULL if \p f_rng doesn't need a context.
 * \param olen     The address at which to store the length of
 *                 the plaintext. This must not be \c NULL.
 * \param input    The ciphertext buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The buffer used to hold the plaintext. This must
 *                 be a writable buffer of length \p output_max_len Bytes.
 * \param output_max_len The length in Bytes of the output buffer \p output.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_pkcs1_decrypt(mbedtls_rsa_context *ctx,
                              mbedtls_f_rng_t *f_rng,
                              void *p_rng,
                              size_t *olen,
                              const unsigned char *input,
                              unsigned char *output,
                              size_t output_max_len);

/**
 * \brief          This function performs a PKCS#1 v1.5 decryption
 *                 operation (RSAES-PKCS1-v1_5-DECRYPT).
 *
 * \warning        This is an inherently dangerous function (CWE-242). Unless
 *                 it is used in a side channel free and safe way (eg.
 *                 implementing the TLS protocol as per 7.4.7.1 of RFC 5246),
 *                 the calling code is vulnerable.
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size \p ctx->len of \p ctx->N, for example,
 *                 128 Bytes if RSA-1024 is used, to be able to hold an
 *                 arbitrary decrypted message. If it is not large enough to
 *                 hold the decryption of the particular ciphertext provided,
 *                 the function returns #MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function. This is used for blinding and is
 *                 mandatory; see mbedtls_rsa_private() for more.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be
 *                 \c NULL if \p f_rng doesn't need a context.
 * \param olen     The address at which to store the length of
 *                 the plaintext. This must not be \c NULL.
 * \param input    The ciphertext buffer. This must be a readable buffer
 *                 of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 * \param output   The buffer used to hold the plaintext. This must
 *                 be a writable buffer of length \p output_max_len Bytes.
 * \param output_max_len The length in Bytes of the output buffer \p output.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 *
 */
int mbedtls_rsa_rsaes_pkcs1_v15_decrypt(mbedtls_rsa_context *ctx,
                                        mbedtls_f_rng_t *f_rng,
                                        void *p_rng,
                                        size_t *olen,
                                        const unsigned char *input,
                                        unsigned char *output,
                                        size_t output_max_len);

/**
 * \brief            This function performs a PKCS#1 v2.1 OAEP decryption
 *                   operation (RSAES-OAEP-DECRYPT).
 *
 * \note             The output buffer length \c output_max_len should be
 *                   as large as the size \p ctx->len of \p ctx->N, for
 *                   example, 128 Bytes if RSA-1024 is used, to be able to
 *                   hold an arbitrary decrypted message. If it is not
 *                   large enough to hold the decryption of the particular
 *                   ciphertext provided, the function returns
 *                   #MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \param ctx        The initialized RSA context to use.
 * \param f_rng      The RNG function. This is used for blinding and is
 *                   mandatory.
 * \param p_rng      The RNG context to be passed to \p f_rng. This may be
 *                   \c NULL if \p f_rng doesn't need a context.
 * \param label      The buffer holding the custom label to use.
 *                   This must be a readable buffer of length \p label_len
 *                   Bytes. It may be \c NULL if \p label_len is \c 0.
 * \param label_len  The length of the label in Bytes.
 * \param olen       The address at which to store the length of
 *                   the plaintext. This must not be \c NULL.
 * \param input      The ciphertext buffer. This must be a readable buffer
 *                   of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                   for an 2048-bit RSA modulus.
 * \param output     The buffer used to hold the plaintext. This must
 *                   be a writable buffer of length \p output_max_len Bytes.
 * \param output_max_len The length in Bytes of the output buffer \p output.
 *
 * \return         \c 0 on success.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsaes_oaep_decrypt(mbedtls_rsa_context *ctx,
                                   mbedtls_f_rng_t *f_rng,
                                   void *p_rng,
                                   const unsigned char *label, size_t label_len,
                                   size_t *olen,
                                   const unsigned char *input,
                                   unsigned char *output,
                                   size_t output_max_len);

/**
 * \brief          This function performs a private RSA operation to sign
 *                 a message digest using PKCS#1.
 *
 *                 It is the generic wrapper for performing a PKCS#1
 *                 signature.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           For PKCS#1 v2.1 encoding, see comments on
 *                 mbedtls_rsa_rsassa_pss_sign() for details on
 *                 \p md_alg and \p hash_id.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function to use. This is mandatory and
 *                 must not be \c NULL.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context argument.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer to hold the signature. This must be a writable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus. A buffer length of
 *                 #MBEDTLS_MPI_MAX_SIZE is always safe.
 *
 * \return         \c 0 if the signing operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_pkcs1_sign(mbedtls_rsa_context *ctx,
                           mbedtls_f_rng_t *f_rng,
                           void *p_rng,
                           mbedtls_md_type_t md_alg,
                           unsigned int hashlen,
                           const unsigned char *hash,
                           unsigned char *sig);

/**
 * \brief          This function performs a PKCS#1 v1.5 signature
 *                 operation (RSASSA-PKCS1-v1_5-SIGN).
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function. This is used for blinding and is
 *                 mandatory; see mbedtls_rsa_private() for more.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context argument.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer to hold the signature. This must be a writable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus. A buffer length of
 *                 #MBEDTLS_MPI_MAX_SIZE is always safe.
 *
 * \return         \c 0 if the signing operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pkcs1_v15_sign(mbedtls_rsa_context *ctx,
                                      mbedtls_f_rng_t *f_rng,
                                      void *p_rng,
                                      mbedtls_md_type_t md_alg,
                                      unsigned int hashlen,
                                      const unsigned char *hash,
                                      unsigned char *sig);

#if defined(MBEDTLS_PKCS1_V21)
/**
 * \brief          This function performs a PKCS#1 v2.1 PSS signature
 *                 operation (RSASSA-PSS-SIGN).
 *
 * \note           The \c hash_id set in \p ctx by calling
 *                 mbedtls_rsa_set_padding() selects the hash used for the
 *                 encoding operation and for the mask generation function
 *                 (MGF1). For more details on the encoding operation and the
 *                 mask generation function, consult <em>RFC-3447: Public-Key
 *                 Cryptography Standards (PKCS) #1 v2.1: RSA Cryptography
 *                 Specifications</em>.
 *
 * \note           This function enforces that the provided salt length complies
 *                 with FIPS 186-4 §5.5 (e) and RFC 8017 (PKCS#1 v2.2) §9.1.1
 *                 step 3. The constraint is that the hash length plus the salt
 *                 length plus 2 bytes must be at most the key length. If this
 *                 constraint is not met, this function returns
 *                 #MBEDTLS_ERR_RSA_BAD_INPUT_DATA.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function. It is mandatory and must not be \c NULL.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context argument.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param saltlen  The length of the salt that should be used.
 *                 If passed #MBEDTLS_RSA_SALT_LEN_ANY, the function will use
 *                 the largest possible salt length up to the hash length,
 *                 which is the largest permitted by some standards including
 *                 FIPS 186-4 §5.5.
 * \param sig      The buffer to hold the signature. This must be a writable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus. A buffer length of
 *                 #MBEDTLS_MPI_MAX_SIZE is always safe.
 *
 * \return         \c 0 if the signing operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pss_sign_ext(mbedtls_rsa_context *ctx,
                                    mbedtls_f_rng_t *f_rng,
                                    void *p_rng,
                                    mbedtls_md_type_t md_alg,
                                    unsigned int hashlen,
                                    const unsigned char *hash,
                                    int saltlen,
                                    unsigned char *sig);

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS signature
 *                 operation (RSASSA-PSS-SIGN).
 *
 * \note           The \c hash_id set in \p ctx by calling
 *                 mbedtls_rsa_set_padding() selects the hash used for the
 *                 encoding operation and for the mask generation function
 *                 (MGF1). For more details on the encoding operation and the
 *                 mask generation function, consult <em>RFC-3447: Public-Key
 *                 Cryptography Standards (PKCS) #1 v2.1: RSA Cryptography
 *                 Specifications</em>.
 *
 * \note           This function always uses the maximum possible salt size,
 *                 up to the length of the payload hash. This choice of salt
 *                 size complies with FIPS 186-4 §5.5 (e) and RFC 8017 (PKCS#1
 *                 v2.2) §9.1.1 step 3. Furthermore this function enforces a
 *                 minimum salt size which is the hash size minus 2 bytes. If
 *                 this minimum size is too large given the key size (the salt
 *                 size, plus the hash size, plus 2 bytes must be no more than
 *                 the key size in bytes), this function returns
 *                 #MBEDTLS_ERR_RSA_BAD_INPUT_DATA.
 *
 * \param ctx      The initialized RSA context to use.
 * \param f_rng    The RNG function. It is mandatory and must not be \c NULL.
 * \param p_rng    The RNG context to be passed to \p f_rng. This may be \c NULL
 *                 if \p f_rng doesn't need a context argument.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer to hold the signature. This must be a writable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus. A buffer length of
 *                 #MBEDTLS_MPI_MAX_SIZE is always safe.
 *
 * \return         \c 0 if the signing operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pss_sign(mbedtls_rsa_context *ctx,
                                mbedtls_f_rng_t *f_rng,
                                void *p_rng,
                                mbedtls_md_type_t md_alg,
                                unsigned int hashlen,
                                const unsigned char *hash,
                                unsigned char *sig);
#endif /* MBEDTLS_PKCS1_V21 */

/**
 * \brief          This function performs a public RSA operation and checks
 *                 the message digest.
 *
 *                 This is the generic wrapper for performing a PKCS#1
 *                 verification.
 *
 * \note           For PKCS#1 v2.1 encoding, see comments on
 *                 mbedtls_rsa_rsassa_pss_verify() about \c md_alg and
 *                 \c hash_id.
 *
 * \param ctx      The initialized RSA public key context to use.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer holding the signature. This must be a readable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 if the verify operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_pkcs1_verify(mbedtls_rsa_context *ctx,
                             mbedtls_md_type_t md_alg,
                             unsigned int hashlen,
                             const unsigned char *hash,
                             const unsigned char *sig);

/**
 * \brief          This function performs a PKCS#1 v1.5 verification
 *                 operation (RSASSA-PKCS1-v1_5-VERIFY).
 *
 * \param ctx      The initialized RSA public key context to use.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer holding the signature. This must be a readable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 if the verify operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pkcs1_v15_verify(mbedtls_rsa_context *ctx,
                                        mbedtls_md_type_t md_alg,
                                        unsigned int hashlen,
                                        const unsigned char *hash,
                                        const unsigned char *sig);

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS verification
 *                 operation (RSASSA-PSS-VERIFY).
 *
 * \note           The \c hash_id set in \p ctx by calling
 *                 mbedtls_rsa_set_padding() selects the hash used for the
 *                 encoding operation and for the mask generation function
 *                 (MGF1). For more details on the encoding operation and the
 *                 mask generation function, consult <em>RFC-3447: Public-Key
 *                 Cryptography Standards (PKCS) #1 v2.1: RSA Cryptography
 *                 Specifications</em>. If the \c hash_id set in \p ctx by
 *                 mbedtls_rsa_set_padding() is #MBEDTLS_MD_NONE, the \p md_alg
 *                 parameter is used.
 *
 * \param ctx      The initialized RSA public key context to use.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param sig      The buffer holding the signature. This must be a readable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 if the verify operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pss_verify(mbedtls_rsa_context *ctx,
                                  mbedtls_md_type_t md_alg,
                                  unsigned int hashlen,
                                  const unsigned char *hash,
                                  const unsigned char *sig);

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS verification
 *                 operation (RSASSA-PSS-VERIFY).
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           The \c hash_id set in \p ctx by mbedtls_rsa_set_padding() is
 *                 ignored.
 *
 * \param ctx      The initialized RSA public key context to use.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest or raw data in Bytes.
 *                 If \p md_alg is not #MBEDTLS_MD_NONE, this must match the
 *                 output length of the corresponding hash algorithm.
 * \param hash     The buffer holding the message digest or raw data.
 *                 This must be a readable buffer of at least \p hashlen Bytes.
 * \param mgf1_hash_id      The message digest algorithm used for the
 *                          verification operation and the mask generation
 *                          function (MGF1). For more details on the encoding
 *                          operation and the mask generation function, consult
 *                          <em>RFC-3447: Public-Key Cryptography Standards
 *                          (PKCS) #1 v2.1: RSA Cryptography
 *                          Specifications</em>.
 * \param expected_salt_len The length of the salt used in padding. Use
 *                          #MBEDTLS_RSA_SALT_LEN_ANY to accept any salt length.
 * \param sig      The buffer holding the signature. This must be a readable
 *                 buffer of length \c ctx->len Bytes. For example, \c 256 Bytes
 *                 for an 2048-bit RSA modulus.
 *
 * \return         \c 0 if the verify operation was successful.
 * \return         An \c MBEDTLS_ERR_RSA_XXX error code on failure.
 */
int mbedtls_rsa_rsassa_pss_verify_ext(mbedtls_rsa_context *ctx,
                                      mbedtls_md_type_t md_alg,
                                      unsigned int hashlen,
                                      const unsigned char *hash,
                                      mbedtls_md_type_t mgf1_hash_id,
                                      int expected_salt_len,
                                      const unsigned char *sig);

/**
 * \brief          This function copies the components of an RSA context.
 *
 * \param dst      The destination context. This must be initialized.
 * \param src      The source context. This must be initialized.
 *
 * \return         \c 0 on success.
 * \return         #MBEDTLS_ERR_MPI_ALLOC_FAILED on memory allocation failure.
 */
int mbedtls_rsa_copy(mbedtls_rsa_context *dst, const mbedtls_rsa_context *src);

/**
 * \brief          This function frees the components of an RSA key.
 *
 * \param ctx      The RSA context to free. May be \c NULL, in which case
 *                 this function is a no-op. If it is not \c NULL, it must
 *                 point to an initialized RSA context.
 */
void mbedtls_rsa_free(mbedtls_rsa_context *ctx);

#if defined(MBEDTLS_SELF_TEST)

/**
 * \brief          The RSA checkup routine.
 *
 * \return         \c 0 on success.
 * \return         \c 1 on failure.
 */
int mbedtls_rsa_self_test(int verbose);

#endif /* MBEDTLS_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* rsa.h */
