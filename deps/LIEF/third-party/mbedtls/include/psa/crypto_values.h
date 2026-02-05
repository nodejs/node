/**
 * \file psa/crypto_values.h
 *
 * \brief PSA cryptography module: macros to build and analyze integer values.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h. Drivers must include the appropriate driver
 * header file.
 *
 * This file contains portable definitions of macros to build and analyze
 * values of integral types that encode properties of cryptographic keys,
 * designations of cryptographic algorithms, and error codes returned by
 * the library.
 *
 * Note that many of the constants defined in this file are embedded in
 * the persistent key store, as part of key metadata (including usage
 * policies). As a consequence, they must not be changed (unless the storage
 * format version changes).
 *
 * This header file only defines preprocessor macros.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_VALUES_H
#define PSA_CRYPTO_VALUES_H
#include "mbedtls/private_access.h"

/** \defgroup error Error codes
 * @{
 */

/* PSA error codes */

/* Error codes are standardized across PSA domains (framework, crypto, storage,
 * etc.). Do not change the values in this section or even the expansions
 * of each macro: it must be possible to `#include` both this header
 * and some other PSA component's headers in the same C source,
 * which will lead to duplicate definitions of the `PSA_SUCCESS` and
 * `PSA_ERROR_xxx` macros, which is ok if and only if the macros expand
 * to the same sequence of tokens.
 *
 * If you must add a new
 * value, check with the Arm PSA framework group to pick one that other
 * domains aren't already using. */

/* Tell uncrustify not to touch the constant definitions, otherwise
 * it might change the spacing to something that is not PSA-compliant
 * (e.g. adding a space after casts).
 *
 * *INDENT-OFF*
 */

/** The action was completed successfully. */
#define PSA_SUCCESS ((psa_status_t)0)

/** An error occurred that does not correspond to any defined
 * failure cause.
 *
 * Implementations may use this error code if none of the other standard
 * error codes are applicable. */
#define PSA_ERROR_GENERIC_ERROR         ((psa_status_t)-132)

/** The requested operation or a parameter is not supported
 * by this implementation.
 *
 * Implementations should return this error code when an enumeration
 * parameter such as a key type, algorithm, etc. is not recognized.
 * If a combination of parameters is recognized and identified as
 * not valid, return #PSA_ERROR_INVALID_ARGUMENT instead. */
#define PSA_ERROR_NOT_SUPPORTED         ((psa_status_t)-134)

/** The requested action is denied by a policy.
 *
 * Implementations should return this error code when the parameters
 * are recognized as valid and supported, and a policy explicitly
 * denies the requested operation.
 *
 * If a subset of the parameters of a function call identify a
 * forbidden operation, and another subset of the parameters are
 * not valid or not supported, it is unspecified whether the function
 * returns #PSA_ERROR_NOT_PERMITTED, #PSA_ERROR_NOT_SUPPORTED or
 * #PSA_ERROR_INVALID_ARGUMENT. */
#define PSA_ERROR_NOT_PERMITTED         ((psa_status_t)-133)

/** An output buffer is too small.
 *
 * Applications can call the \c PSA_xxx_SIZE macro listed in the function
 * description to determine a sufficient buffer size.
 *
 * Implementations should preferably return this error code only
 * in cases when performing the operation with a larger output
 * buffer would succeed. However implementations may return this
 * error if a function has invalid or unsupported parameters in addition
 * to the parameters that determine the necessary output buffer size. */
#define PSA_ERROR_BUFFER_TOO_SMALL      ((psa_status_t)-138)

/** Asking for an item that already exists
 *
 * Implementations should return this error, when attempting
 * to write an item (like a key) that already exists. */
#define PSA_ERROR_ALREADY_EXISTS        ((psa_status_t)-139)

/** Asking for an item that doesn't exist
 *
 * Implementations should return this error, if a requested item (like
 * a key) does not exist. */
#define PSA_ERROR_DOES_NOT_EXIST        ((psa_status_t)-140)

/** The requested action cannot be performed in the current state.
 *
 * Multipart operations return this error when one of the
 * functions is called out of sequence. Refer to the function
 * descriptions for permitted sequencing of functions.
 *
 * Implementations shall not return this error code to indicate
 * that a key either exists or not,
 * but shall instead return #PSA_ERROR_ALREADY_EXISTS or #PSA_ERROR_DOES_NOT_EXIST
 * as applicable.
 *
 * Implementations shall not return this error code to indicate that a
 * key identifier is invalid, but shall return #PSA_ERROR_INVALID_HANDLE
 * instead. */
#define PSA_ERROR_BAD_STATE             ((psa_status_t)-137)

/** The parameters passed to the function are invalid.
 *
 * Implementations may return this error any time a parameter or
 * combination of parameters are recognized as invalid.
 *
 * Implementations shall not return this error code to indicate that a
 * key identifier is invalid, but shall return #PSA_ERROR_INVALID_HANDLE
 * instead.
 */
#define PSA_ERROR_INVALID_ARGUMENT      ((psa_status_t)-135)

/** There is not enough runtime memory.
 *
 * If the action is carried out across multiple security realms, this
 * error can refer to available memory in any of the security realms. */
#define PSA_ERROR_INSUFFICIENT_MEMORY   ((psa_status_t)-141)

/** There is not enough persistent storage.
 *
 * Functions that modify the key storage return this error code if
 * there is insufficient storage space on the host media. In addition,
 * many functions that do not otherwise access storage may return this
 * error code if the implementation requires a mandatory log entry for
 * the requested action and the log storage space is full. */
#define PSA_ERROR_INSUFFICIENT_STORAGE  ((psa_status_t)-142)

/** There was a communication failure inside the implementation.
 *
 * This can indicate a communication failure between the application
 * and an external cryptoprocessor or between the cryptoprocessor and
 * an external volatile or persistent memory. A communication failure
 * may be transient or permanent depending on the cause.
 *
 * \warning If a function returns this error, it is undetermined
 * whether the requested action has completed or not. Implementations
 * should return #PSA_SUCCESS on successful completion whenever
 * possible, however functions may return #PSA_ERROR_COMMUNICATION_FAILURE
 * if the requested action was completed successfully in an external
 * cryptoprocessor but there was a breakdown of communication before
 * the cryptoprocessor could report the status to the application.
 */
#define PSA_ERROR_COMMUNICATION_FAILURE ((psa_status_t)-145)

/** There was a storage failure that may have led to data loss.
 *
 * This error indicates that some persistent storage is corrupted.
 * It should not be used for a corruption of volatile memory
 * (use #PSA_ERROR_CORRUPTION_DETECTED), for a communication error
 * between the cryptoprocessor and its external storage (use
 * #PSA_ERROR_COMMUNICATION_FAILURE), or when the storage is
 * in a valid state but is full (use #PSA_ERROR_INSUFFICIENT_STORAGE).
 *
 * Note that a storage failure does not indicate that any data that was
 * previously read is invalid. However this previously read data may no
 * longer be readable from storage.
 *
 * When a storage failure occurs, it is no longer possible to ensure
 * the global integrity of the keystore. Depending on the global
 * integrity guarantees offered by the implementation, access to other
 * data may or may not fail even if the data is still readable but
 * its integrity cannot be guaranteed.
 *
 * Implementations should only use this error code to report a
 * permanent storage corruption. However application writers should
 * keep in mind that transient errors while reading the storage may be
 * reported using this error code. */
#define PSA_ERROR_STORAGE_FAILURE       ((psa_status_t)-146)

/** A hardware failure was detected.
 *
 * A hardware failure may be transient or permanent depending on the
 * cause. */
#define PSA_ERROR_HARDWARE_FAILURE      ((psa_status_t)-147)

/** A tampering attempt was detected.
 *
 * If an application receives this error code, there is no guarantee
 * that previously accessed or computed data was correct and remains
 * confidential. Applications should not perform any security function
 * and should enter a safe failure state.
 *
 * Implementations may return this error code if they detect an invalid
 * state that cannot happen during normal operation and that indicates
 * that the implementation's security guarantees no longer hold. Depending
 * on the implementation architecture and on its security and safety goals,
 * the implementation may forcibly terminate the application.
 *
 * This error code is intended as a last resort when a security breach
 * is detected and it is unsure whether the keystore data is still
 * protected. Implementations shall only return this error code
 * to report an alarm from a tampering detector, to indicate that
 * the confidentiality of stored data can no longer be guaranteed,
 * or to indicate that the integrity of previously returned data is now
 * considered compromised. Implementations shall not use this error code
 * to indicate a hardware failure that merely makes it impossible to
 * perform the requested operation (use #PSA_ERROR_COMMUNICATION_FAILURE,
 * #PSA_ERROR_STORAGE_FAILURE, #PSA_ERROR_HARDWARE_FAILURE,
 * #PSA_ERROR_INSUFFICIENT_ENTROPY or other applicable error code
 * instead).
 *
 * This error indicates an attack against the application. Implementations
 * shall not return this error code as a consequence of the behavior of
 * the application itself. */
#define PSA_ERROR_CORRUPTION_DETECTED    ((psa_status_t)-151)

/** There is not enough entropy to generate random data needed
 * for the requested action.
 *
 * This error indicates a failure of a hardware random generator.
 * Application writers should note that this error can be returned not
 * only by functions whose purpose is to generate random data, such
 * as key, IV or nonce generation, but also by functions that execute
 * an algorithm with a randomized result, as well as functions that
 * use randomization of intermediate computations as a countermeasure
 * to certain attacks.
 *
 * Implementations should avoid returning this error after psa_crypto_init()
 * has succeeded. Implementations should generate sufficient
 * entropy during initialization and subsequently use a cryptographically
 * secure pseudorandom generator (PRNG). However implementations may return
 * this error at any time if a policy requires the PRNG to be reseeded
 * during normal operation. */
#define PSA_ERROR_INSUFFICIENT_ENTROPY  ((psa_status_t)-148)

/** The signature, MAC or hash is incorrect.
 *
 * Verification functions return this error if the verification
 * calculations completed successfully, and the value to be verified
 * was determined to be incorrect.
 *
 * If the value to verify has an invalid size, implementations may return
 * either #PSA_ERROR_INVALID_ARGUMENT or #PSA_ERROR_INVALID_SIGNATURE. */
#define PSA_ERROR_INVALID_SIGNATURE     ((psa_status_t)-149)

/** The decrypted padding is incorrect.
 *
 * \warning In some protocols, when decrypting data, it is essential that
 * the behavior of the application does not depend on whether the padding
 * is correct, down to precise timing. Applications should prefer
 * protocols that use authenticated encryption rather than plain
 * encryption. If the application must perform a decryption of
 * unauthenticated data, the application writer should take care not
 * to reveal whether the padding is invalid.
 *
 * Implementations should strive to make valid and invalid padding
 * as close as possible to indistinguishable to an external observer.
 * In particular, the timing of a decryption operation should not
 * depend on the validity of the padding. */
#define PSA_ERROR_INVALID_PADDING       ((psa_status_t)-150)

/** Return this error when there's insufficient data when attempting
 * to read from a resource. */
#define PSA_ERROR_INSUFFICIENT_DATA     ((psa_status_t)-143)

/** This can be returned if a function can no longer operate correctly.
 * For example, if an essential initialization operation failed or
 * a mutex operation failed. */
#define PSA_ERROR_SERVICE_FAILURE       ((psa_status_t)-144)

/** The key identifier is not valid. See also :ref:\`key-handles\`.
 */
#define PSA_ERROR_INVALID_HANDLE        ((psa_status_t)-136)

/** Stored data has been corrupted.
 *
 * This error indicates that some persistent storage has suffered corruption.
 * It does not indicate the following situations, which have specific error
 * codes:
 *
 * - A corruption of volatile memory - use #PSA_ERROR_CORRUPTION_DETECTED.
 * - A communication error between the cryptoprocessor and its external
 *   storage - use #PSA_ERROR_COMMUNICATION_FAILURE.
 * - When the storage is in a valid state but is full - use
 *   #PSA_ERROR_INSUFFICIENT_STORAGE.
 * - When the storage fails for other reasons - use
 *   #PSA_ERROR_STORAGE_FAILURE.
 * - When the stored data is not valid - use #PSA_ERROR_DATA_INVALID.
 *
 * \note A storage corruption does not indicate that any data that was
 * previously read is invalid. However this previously read data might no
 * longer be readable from storage.
 *
 * When a storage failure occurs, it is no longer possible to ensure the
 * global integrity of the keystore.
 */
#define PSA_ERROR_DATA_CORRUPT          ((psa_status_t)-152)

/** Data read from storage is not valid for the implementation.
 *
 * This error indicates that some data read from storage does not have a valid
 * format. It does not indicate the following situations, which have specific
 * error codes:
 *
 * - When the storage or stored data is corrupted - use #PSA_ERROR_DATA_CORRUPT
 * - When the storage fails for other reasons - use #PSA_ERROR_STORAGE_FAILURE
 * - An invalid argument to the API - use #PSA_ERROR_INVALID_ARGUMENT
 *
 * This error is typically a result of either storage corruption on a
 * cleartext storage backend, or an attempt to read data that was
 * written by an incompatible version of the library.
 */
#define PSA_ERROR_DATA_INVALID          ((psa_status_t)-153)

/** The function that returns this status is defined as interruptible and
 *  still has work to do, thus the user should call the function again with the
 *  same operation context until it either returns #PSA_SUCCESS or any other
 *  error. This is not an error per se, more a notification of status.
 */
#define PSA_OPERATION_INCOMPLETE           ((psa_status_t)-248)

/* *INDENT-ON* */

/**@}*/

/** \defgroup crypto_types Key and algorithm types
 * @{
 */

/* Note that key type values, including ECC family and DH group values, are
 * embedded in the persistent key store, as part of key metadata. As a
 * consequence, they must not be changed (unless the storage format version
 * changes).
 */

/** An invalid key type value.
 *
 * Zero is not the encoding of any key type.
 */
#define PSA_KEY_TYPE_NONE                           ((psa_key_type_t) 0x0000)

/** Vendor-defined key type flag.
 *
 * Key types defined by this standard will never have the
 * #PSA_KEY_TYPE_VENDOR_FLAG bit set. Vendors who define additional key types
 * must use an encoding with the #PSA_KEY_TYPE_VENDOR_FLAG bit set and should
 * respect the bitwise structure used by standard encodings whenever practical.
 */
#define PSA_KEY_TYPE_VENDOR_FLAG                    ((psa_key_type_t) 0x8000)

#define PSA_KEY_TYPE_CATEGORY_MASK                  ((psa_key_type_t) 0x7000)
#define PSA_KEY_TYPE_CATEGORY_RAW                   ((psa_key_type_t) 0x1000)
#define PSA_KEY_TYPE_CATEGORY_SYMMETRIC             ((psa_key_type_t) 0x2000)
#define PSA_KEY_TYPE_CATEGORY_PUBLIC_KEY            ((psa_key_type_t) 0x4000)
#define PSA_KEY_TYPE_CATEGORY_KEY_PAIR              ((psa_key_type_t) 0x7000)

#define PSA_KEY_TYPE_CATEGORY_FLAG_PAIR             ((psa_key_type_t) 0x3000)

/** Whether a key type is vendor-defined.
 *
 * See also #PSA_KEY_TYPE_VENDOR_FLAG.
 */
#define PSA_KEY_TYPE_IS_VENDOR_DEFINED(type) \
    (((type) & PSA_KEY_TYPE_VENDOR_FLAG) != 0)

/** Whether a key type is an unstructured array of bytes.
 *
 * This encompasses both symmetric keys and non-key data.
 */
#define PSA_KEY_TYPE_IS_UNSTRUCTURED(type) \
    (((type) & PSA_KEY_TYPE_CATEGORY_MASK) == PSA_KEY_TYPE_CATEGORY_RAW || \
     ((type) & PSA_KEY_TYPE_CATEGORY_MASK) == PSA_KEY_TYPE_CATEGORY_SYMMETRIC)

/** Whether a key type is asymmetric: either a key pair or a public key. */
#define PSA_KEY_TYPE_IS_ASYMMETRIC(type)                                \
    (((type) & PSA_KEY_TYPE_CATEGORY_MASK                               \
      & ~PSA_KEY_TYPE_CATEGORY_FLAG_PAIR) ==                            \
     PSA_KEY_TYPE_CATEGORY_PUBLIC_KEY)
/** Whether a key type is the public part of a key pair. */
#define PSA_KEY_TYPE_IS_PUBLIC_KEY(type)                                \
    (((type) & PSA_KEY_TYPE_CATEGORY_MASK) == PSA_KEY_TYPE_CATEGORY_PUBLIC_KEY)
/** Whether a key type is a key pair containing a private part and a public
 * part. */
#define PSA_KEY_TYPE_IS_KEY_PAIR(type)                                   \
    (((type) & PSA_KEY_TYPE_CATEGORY_MASK) == PSA_KEY_TYPE_CATEGORY_KEY_PAIR)
/** The key pair type corresponding to a public key type.
 *
 * You may also pass a key pair type as \p type, it will be left unchanged.
 *
 * \param type      A public key type or key pair type.
 *
 * \return          The corresponding key pair type.
 *                  If \p type is not a public key or a key pair,
 *                  the return value is undefined.
 */
#define PSA_KEY_TYPE_KEY_PAIR_OF_PUBLIC_KEY(type)        \
    ((type) | PSA_KEY_TYPE_CATEGORY_FLAG_PAIR)
/** The public key type corresponding to a key pair type.
 *
 * You may also pass a public key type as \p type, it will be left unchanged.
 *
 * \param type      A public key type or key pair type.
 *
 * \return          The corresponding public key type.
 *                  If \p type is not a public key or a key pair,
 *                  the return value is undefined.
 */
#define PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(type)        \
    ((type) & ~PSA_KEY_TYPE_CATEGORY_FLAG_PAIR)

/** Raw data.
 *
 * A "key" of this type cannot be used for any cryptographic operation.
 * Applications may use this type to store arbitrary data in the keystore. */
#define PSA_KEY_TYPE_RAW_DATA                       ((psa_key_type_t) 0x1001)

/** HMAC key.
 *
 * The key policy determines which underlying hash algorithm the key can be
 * used for.
 *
 * HMAC keys should generally have the same size as the underlying hash.
 * This size can be calculated with #PSA_HASH_LENGTH(\c alg) where
 * \c alg is the HMAC algorithm or the underlying hash algorithm. */
#define PSA_KEY_TYPE_HMAC                           ((psa_key_type_t) 0x1100)

/** A secret for key derivation.
 *
 * This key type is for high-entropy secrets only. For low-entropy secrets,
 * #PSA_KEY_TYPE_PASSWORD should be used instead.
 *
 * These keys can be used as the #PSA_KEY_DERIVATION_INPUT_SECRET or
 * #PSA_KEY_DERIVATION_INPUT_PASSWORD input of key derivation algorithms.
 *
 * The key policy determines which key derivation algorithm the key
 * can be used for.
 */
#define PSA_KEY_TYPE_DERIVE                         ((psa_key_type_t) 0x1200)

/** A low-entropy secret for password hashing or key derivation.
 *
 * This key type is suitable for passwords and passphrases which are typically
 * intended to be memorizable by humans, and have a low entropy relative to
 * their size. It can be used for randomly generated or derived keys with
 * maximum or near-maximum entropy, but #PSA_KEY_TYPE_DERIVE is more suitable
 * for such keys. It is not suitable for passwords with extremely low entropy,
 * such as numerical PINs.
 *
 * These keys can be used as the #PSA_KEY_DERIVATION_INPUT_PASSWORD input of
 * key derivation algorithms. Algorithms that accept such an input were
 * designed to accept low-entropy secret and are known as password hashing or
 * key stretching algorithms.
 *
 * These keys cannot be used as the #PSA_KEY_DERIVATION_INPUT_SECRET input of
 * key derivation algorithms, as the algorithms that take such an input expect
 * it to be high-entropy.
 *
 * The key policy determines which key derivation algorithm the key can be
 * used for, among the permissible subset defined above.
 */
#define PSA_KEY_TYPE_PASSWORD                       ((psa_key_type_t) 0x1203)

/** A secret value that can be used to verify a password hash.
 *
 * The key policy determines which key derivation algorithm the key
 * can be used for, among the same permissible subset as for
 * #PSA_KEY_TYPE_PASSWORD.
 */
#define PSA_KEY_TYPE_PASSWORD_HASH                  ((psa_key_type_t) 0x1205)

/** A secret value that can be used in when computing a password hash.
 *
 * The key policy determines which key derivation algorithm the key
 * can be used for, among the subset of algorithms that can use pepper.
 */
#define PSA_KEY_TYPE_PEPPER                         ((psa_key_type_t) 0x1206)

/** Key for a cipher, AEAD or MAC algorithm based on the AES block cipher.
 *
 * The size of the key can be 16 bytes (AES-128), 24 bytes (AES-192) or
 * 32 bytes (AES-256).
 */
#define PSA_KEY_TYPE_AES                            ((psa_key_type_t) 0x2400)

/** Key for a cipher, AEAD or MAC algorithm based on the
 * ARIA block cipher. */
#define PSA_KEY_TYPE_ARIA                           ((psa_key_type_t) 0x2406)

/** Key for a cipher or MAC algorithm based on DES or 3DES (Triple-DES).
 *
 * The size of the key can be 64 bits (single DES), 128 bits (2-key 3DES) or
 * 192 bits (3-key 3DES).
 *
 * Note that single DES and 2-key 3DES are weak and strongly
 * deprecated and should only be used to decrypt legacy data. 3-key 3DES
 * is weak and deprecated and should only be used in legacy protocols.
 */
#define PSA_KEY_TYPE_DES                            ((psa_key_type_t) 0x2301)

/** Key for a cipher, AEAD or MAC algorithm based on the
 * Camellia block cipher. */
#define PSA_KEY_TYPE_CAMELLIA                       ((psa_key_type_t) 0x2403)

/** Key for the ChaCha20 stream cipher or the Chacha20-Poly1305 AEAD algorithm.
 *
 * ChaCha20 and the ChaCha20_Poly1305 construction are defined in RFC 7539.
 *
 * \note For ChaCha20 and ChaCha20_Poly1305, Mbed TLS only supports
 *       12-byte nonces.
 *
 * \note For ChaCha20, the initial counter value is 0. To encrypt or decrypt
 *       with the initial counter value 1, you can process and discard a
 *       64-byte block before the real data.
 */
#define PSA_KEY_TYPE_CHACHA20                       ((psa_key_type_t) 0x2004)

/** RSA public key.
 *
 * The size of an RSA key is the bit size of the modulus.
 */
#define PSA_KEY_TYPE_RSA_PUBLIC_KEY                 ((psa_key_type_t) 0x4001)
/** RSA key pair (private and public key).
 *
 * The size of an RSA key is the bit size of the modulus.
 */
#define PSA_KEY_TYPE_RSA_KEY_PAIR                   ((psa_key_type_t) 0x7001)
/** Whether a key type is an RSA key (pair or public-only). */
#define PSA_KEY_TYPE_IS_RSA(type)                                       \
    (PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(type) == PSA_KEY_TYPE_RSA_PUBLIC_KEY)

#define PSA_KEY_TYPE_ECC_PUBLIC_KEY_BASE            ((psa_key_type_t) 0x4100)
#define PSA_KEY_TYPE_ECC_KEY_PAIR_BASE              ((psa_key_type_t) 0x7100)
#define PSA_KEY_TYPE_ECC_CURVE_MASK                 ((psa_key_type_t) 0x00ff)
/** Elliptic curve key pair.
 *
 * The size of an elliptic curve key is the bit size associated with the curve,
 * i.e. the bit size of *q* for a curve over a field *F<sub>q</sub>*.
 * See the documentation of `PSA_ECC_FAMILY_xxx` curve families for details.
 *
 * \param curve     A value of type ::psa_ecc_family_t that
 *                  identifies the ECC curve to be used.
 */
#define PSA_KEY_TYPE_ECC_KEY_PAIR(curve)         \
    (PSA_KEY_TYPE_ECC_KEY_PAIR_BASE | (curve))
/** Elliptic curve public key.
 *
 * The size of an elliptic curve public key is the same as the corresponding
 * private key (see #PSA_KEY_TYPE_ECC_KEY_PAIR and the documentation of
 * `PSA_ECC_FAMILY_xxx` curve families).
 *
 * \param curve     A value of type ::psa_ecc_family_t that
 *                  identifies the ECC curve to be used.
 */
#define PSA_KEY_TYPE_ECC_PUBLIC_KEY(curve)              \
    (PSA_KEY_TYPE_ECC_PUBLIC_KEY_BASE | (curve))

/** Whether a key type is an elliptic curve key (pair or public-only). */
#define PSA_KEY_TYPE_IS_ECC(type)                                       \
    ((PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(type) &                        \
      ~PSA_KEY_TYPE_ECC_CURVE_MASK) == PSA_KEY_TYPE_ECC_PUBLIC_KEY_BASE)
/** Whether a key type is an elliptic curve key pair. */
#define PSA_KEY_TYPE_IS_ECC_KEY_PAIR(type)                               \
    (((type) & ~PSA_KEY_TYPE_ECC_CURVE_MASK) ==                         \
     PSA_KEY_TYPE_ECC_KEY_PAIR_BASE)
/** Whether a key type is an elliptic curve public key. */
#define PSA_KEY_TYPE_IS_ECC_PUBLIC_KEY(type)                            \
    (((type) & ~PSA_KEY_TYPE_ECC_CURVE_MASK) ==                         \
     PSA_KEY_TYPE_ECC_PUBLIC_KEY_BASE)

/** Extract the curve from an elliptic curve key type. */
#define PSA_KEY_TYPE_ECC_GET_FAMILY(type)                        \
    ((psa_ecc_family_t) (PSA_KEY_TYPE_IS_ECC(type) ?             \
                         ((type) & PSA_KEY_TYPE_ECC_CURVE_MASK) : \
                         0))

/** Check if the curve of given family is Weierstrass elliptic curve. */
#define PSA_ECC_FAMILY_IS_WEIERSTRASS(family) ((family & 0xc0) == 0)

/** SEC Koblitz curves over prime fields.
 *
 * This family comprises the following curves:
 * secp192k1, secp224k1, secp256k1.
 * They are defined in _Standards for Efficient Cryptography_,
 * _SEC 2: Recommended Elliptic Curve Domain Parameters_.
 * https://www.secg.org/sec2-v2.pdf
 *
 * \note For secp224k1, the bit-size is 225 (size of a private value).
 *
 * \note Mbed TLS only supports secp192k1 and secp256k1.
 */
#define PSA_ECC_FAMILY_SECP_K1           ((psa_ecc_family_t) 0x17)

/** SEC random curves over prime fields.
 *
 * This family comprises the following curves:
 * secp192r1, secp224r1, secp256r1, secp384r1, secp521r1.
 * They are defined in _Standards for Efficient Cryptography_,
 * _SEC 2: Recommended Elliptic Curve Domain Parameters_.
 * https://www.secg.org/sec2-v2.pdf
 */
#define PSA_ECC_FAMILY_SECP_R1           ((psa_ecc_family_t) 0x12)
/* SECP160R2 (SEC2 v1, obsolete, not supported in Mbed TLS) */
#define PSA_ECC_FAMILY_SECP_R2           ((psa_ecc_family_t) 0x1b)

/** SEC Koblitz curves over binary fields.
 *
 * This family comprises the following curves:
 * sect163k1, sect233k1, sect239k1, sect283k1, sect409k1, sect571k1.
 * They are defined in _Standards for Efficient Cryptography_,
 * _SEC 2: Recommended Elliptic Curve Domain Parameters_.
 * https://www.secg.org/sec2-v2.pdf
 *
 * \note Mbed TLS does not support any curve in this family.
 */
#define PSA_ECC_FAMILY_SECT_K1           ((psa_ecc_family_t) 0x27)

/** SEC random curves over binary fields.
 *
 * This family comprises the following curves:
 * sect163r1, sect233r1, sect283r1, sect409r1, sect571r1.
 * They are defined in _Standards for Efficient Cryptography_,
 * _SEC 2: Recommended Elliptic Curve Domain Parameters_.
 * https://www.secg.org/sec2-v2.pdf
 *
 * \note Mbed TLS does not support any curve in this family.
 */
#define PSA_ECC_FAMILY_SECT_R1           ((psa_ecc_family_t) 0x22)

/** SEC additional random curves over binary fields.
 *
 * This family comprises the following curve:
 * sect163r2.
 * It is defined in _Standards for Efficient Cryptography_,
 * _SEC 2: Recommended Elliptic Curve Domain Parameters_.
 * https://www.secg.org/sec2-v2.pdf
 *
 * \note Mbed TLS does not support any curve in this family.
 */
#define PSA_ECC_FAMILY_SECT_R2           ((psa_ecc_family_t) 0x2b)

/** Brainpool P random curves.
 *
 * This family comprises the following curves:
 * brainpoolP160r1, brainpoolP192r1, brainpoolP224r1, brainpoolP256r1,
 * brainpoolP320r1, brainpoolP384r1, brainpoolP512r1.
 * It is defined in RFC 5639.
 *
 * \note Mbed TLS only supports the 256-bit, 384-bit and 512-bit curves
 *       in this family.
 */
#define PSA_ECC_FAMILY_BRAINPOOL_P_R1    ((psa_ecc_family_t) 0x30)

/** Curve25519 and Curve448.
 *
 * This family comprises the following Montgomery curves:
 * - 255-bit: Bernstein et al.,
 *   _Curve25519: new Diffie-Hellman speed records_, LNCS 3958, 2006.
 *   The algorithm #PSA_ALG_ECDH performs X25519 when used with this curve.
 * - 448-bit: Hamburg,
 *   _Ed448-Goldilocks, a new elliptic curve_, NIST ECC Workshop, 2015.
 *   The algorithm #PSA_ALG_ECDH performs X448 when used with this curve.
 */
#define PSA_ECC_FAMILY_MONTGOMERY        ((psa_ecc_family_t) 0x41)

/** The twisted Edwards curves Ed25519 and Ed448.
 *
 * These curves are suitable for EdDSA (#PSA_ALG_PURE_EDDSA for both curves,
 * #PSA_ALG_ED25519PH for the 255-bit curve,
 * #PSA_ALG_ED448PH for the 448-bit curve).
 *
 * This family comprises the following twisted Edwards curves:
 * - 255-bit: Edwards25519, the twisted Edwards curve birationally equivalent
 *   to Curve25519.
 *   Bernstein et al., _Twisted Edwards curves_, Africacrypt 2008.
 * - 448-bit: Edwards448, the twisted Edwards curve birationally equivalent
 *   to Curve448.
 *   Hamburg, _Ed448-Goldilocks, a new elliptic curve_, NIST ECC Workshop, 2015.
 *
 * \note Mbed TLS does not support Edwards curves yet.
 */
#define PSA_ECC_FAMILY_TWISTED_EDWARDS   ((psa_ecc_family_t) 0x42)

#define PSA_KEY_TYPE_DH_PUBLIC_KEY_BASE             ((psa_key_type_t) 0x4200)
#define PSA_KEY_TYPE_DH_KEY_PAIR_BASE               ((psa_key_type_t) 0x7200)
#define PSA_KEY_TYPE_DH_GROUP_MASK                  ((psa_key_type_t) 0x00ff)
/** Diffie-Hellman key pair.
 *
 * \param group     A value of type ::psa_dh_family_t that identifies the
 *                  Diffie-Hellman group to be used.
 */
#define PSA_KEY_TYPE_DH_KEY_PAIR(group)          \
    (PSA_KEY_TYPE_DH_KEY_PAIR_BASE | (group))
/** Diffie-Hellman public key.
 *
 * \param group     A value of type ::psa_dh_family_t that identifies the
 *                  Diffie-Hellman group to be used.
 */
#define PSA_KEY_TYPE_DH_PUBLIC_KEY(group)               \
    (PSA_KEY_TYPE_DH_PUBLIC_KEY_BASE | (group))

/** Whether a key type is a Diffie-Hellman key (pair or public-only). */
#define PSA_KEY_TYPE_IS_DH(type)                                        \
    ((PSA_KEY_TYPE_PUBLIC_KEY_OF_KEY_PAIR(type) &                        \
      ~PSA_KEY_TYPE_DH_GROUP_MASK) == PSA_KEY_TYPE_DH_PUBLIC_KEY_BASE)
/** Whether a key type is a Diffie-Hellman key pair. */
#define PSA_KEY_TYPE_IS_DH_KEY_PAIR(type)                               \
    (((type) & ~PSA_KEY_TYPE_DH_GROUP_MASK) ==                         \
     PSA_KEY_TYPE_DH_KEY_PAIR_BASE)
/** Whether a key type is a Diffie-Hellman public key. */
#define PSA_KEY_TYPE_IS_DH_PUBLIC_KEY(type)                            \
    (((type) & ~PSA_KEY_TYPE_DH_GROUP_MASK) ==                         \
     PSA_KEY_TYPE_DH_PUBLIC_KEY_BASE)

/** Extract the group from a Diffie-Hellman key type. */
#define PSA_KEY_TYPE_DH_GET_FAMILY(type)                        \
    ((psa_dh_family_t) (PSA_KEY_TYPE_IS_DH(type) ?              \
                        ((type) & PSA_KEY_TYPE_DH_GROUP_MASK) :  \
                        0))

/** Diffie-Hellman groups defined in RFC 7919 Appendix A.
 *
 * This family includes groups with the following key sizes (in bits):
 * 2048, 3072, 4096, 6144, 8192. A given implementation may support
 * all of these sizes or only a subset.
 */
#define PSA_DH_FAMILY_RFC7919            ((psa_dh_family_t) 0x03)

#define PSA_GET_KEY_TYPE_BLOCK_SIZE_EXPONENT(type)      \
    (((type) >> 8) & 7)
/** The block size of a block cipher.
 *
 * \param type  A cipher key type (value of type #psa_key_type_t).
 *
 * \return      The block size for a block cipher, or 1 for a stream cipher.
 *              The return value is undefined if \p type is not a supported
 *              cipher key type.
 *
 * \note It is possible to build stream cipher algorithms on top of a block
 *       cipher, for example CTR mode (#PSA_ALG_CTR).
 *       This macro only takes the key type into account, so it cannot be
 *       used to determine the size of the data that #psa_cipher_update()
 *       might buffer for future processing in general.
 *
 * \note This macro returns a compile-time constant if its argument is one.
 *
 * \warning This macro may evaluate its argument multiple times.
 */
#define PSA_BLOCK_CIPHER_BLOCK_LENGTH(type)                                     \
    (((type) & PSA_KEY_TYPE_CATEGORY_MASK) == PSA_KEY_TYPE_CATEGORY_SYMMETRIC ? \
     1u << PSA_GET_KEY_TYPE_BLOCK_SIZE_EXPONENT(type) :                         \
        0u)

/* Note that algorithm values are embedded in the persistent key store,
 * as part of key metadata. As a consequence, they must not be changed
 * (unless the storage format version changes).
 */

/** Vendor-defined algorithm flag.
 *
 * Algorithms defined by this standard will never have the #PSA_ALG_VENDOR_FLAG
 * bit set. Vendors who define additional algorithms must use an encoding with
 * the #PSA_ALG_VENDOR_FLAG bit set and should respect the bitwise structure
 * used by standard encodings whenever practical.
 */
#define PSA_ALG_VENDOR_FLAG                     ((psa_algorithm_t) 0x80000000)

#define PSA_ALG_CATEGORY_MASK                   ((psa_algorithm_t) 0x7f000000)
#define PSA_ALG_CATEGORY_HASH                   ((psa_algorithm_t) 0x02000000)
#define PSA_ALG_CATEGORY_MAC                    ((psa_algorithm_t) 0x03000000)
#define PSA_ALG_CATEGORY_CIPHER                 ((psa_algorithm_t) 0x04000000)
#define PSA_ALG_CATEGORY_AEAD                   ((psa_algorithm_t) 0x05000000)
#define PSA_ALG_CATEGORY_SIGN                   ((psa_algorithm_t) 0x06000000)
#define PSA_ALG_CATEGORY_ASYMMETRIC_ENCRYPTION  ((psa_algorithm_t) 0x07000000)
#define PSA_ALG_CATEGORY_KEY_DERIVATION         ((psa_algorithm_t) 0x08000000)
#define PSA_ALG_CATEGORY_KEY_AGREEMENT          ((psa_algorithm_t) 0x09000000)

/** Whether an algorithm is vendor-defined.
 *
 * See also #PSA_ALG_VENDOR_FLAG.
 */
#define PSA_ALG_IS_VENDOR_DEFINED(alg)                                  \
    (((alg) & PSA_ALG_VENDOR_FLAG) != 0)

/** Whether the specified algorithm is a hash algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a hash algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_HASH(alg)                                            \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_HASH)

/** Whether the specified algorithm is a MAC algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a MAC algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_MAC(alg)                                             \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_MAC)

/** Whether the specified algorithm is a symmetric cipher algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a symmetric cipher algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_CIPHER(alg)                                          \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_CIPHER)

/** Whether the specified algorithm is an authenticated encryption
 * with associated data (AEAD) algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is an AEAD algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_AEAD(alg)                                            \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_AEAD)

/** Whether the specified algorithm is an asymmetric signature algorithm,
 * also known as public-key signature algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is an asymmetric signature algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_SIGN(alg)                                            \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_SIGN)

/** Whether the specified algorithm is an asymmetric encryption algorithm,
 * also known as public-key encryption algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is an asymmetric encryption algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_ASYMMETRIC_ENCRYPTION(alg)                           \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_ASYMMETRIC_ENCRYPTION)

/** Whether the specified algorithm is a key agreement algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a key agreement algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_KEY_AGREEMENT(alg)                                   \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_KEY_AGREEMENT)

/** Whether the specified algorithm is a key derivation algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a key derivation algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_KEY_DERIVATION(alg)                                  \
    (((alg) & PSA_ALG_CATEGORY_MASK) == PSA_ALG_CATEGORY_KEY_DERIVATION)

/** Whether the specified algorithm is a key stretching / password hashing
 * algorithm.
 *
 * A key stretching / password hashing algorithm is a key derivation algorithm
 * that is suitable for use with a low-entropy secret such as a password.
 * Equivalently, it's a key derivation algorithm that uses a
 * #PSA_KEY_DERIVATION_INPUT_PASSWORD input step.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a key stretching / password hashing algorithm, 0
 *         otherwise. This macro may return either 0 or 1 if \p alg is not a
 *         supported algorithm identifier.
 */
#define PSA_ALG_IS_KEY_DERIVATION_STRETCHING(alg)                                  \
    (PSA_ALG_IS_KEY_DERIVATION(alg) &&              \
     (alg) & PSA_ALG_KEY_DERIVATION_STRETCHING_FLAG)

/** An invalid algorithm identifier value. */
/* *INDENT-OFF* (https://github.com/ARM-software/psa-arch-tests/issues/337) */
#define PSA_ALG_NONE                            ((psa_algorithm_t)0)
/* *INDENT-ON* */

#define PSA_ALG_HASH_MASK                       ((psa_algorithm_t) 0x000000ff)
/** MD5 */
#define PSA_ALG_MD5                             ((psa_algorithm_t) 0x02000003)
/** PSA_ALG_RIPEMD160 */
#define PSA_ALG_RIPEMD160                       ((psa_algorithm_t) 0x02000004)
/** SHA1 */
#define PSA_ALG_SHA_1                           ((psa_algorithm_t) 0x02000005)
/** SHA2-224 */
#define PSA_ALG_SHA_224                         ((psa_algorithm_t) 0x02000008)
/** SHA2-256 */
#define PSA_ALG_SHA_256                         ((psa_algorithm_t) 0x02000009)
/** SHA2-384 */
#define PSA_ALG_SHA_384                         ((psa_algorithm_t) 0x0200000a)
/** SHA2-512 */
#define PSA_ALG_SHA_512                         ((psa_algorithm_t) 0x0200000b)
/** SHA2-512/224 */
#define PSA_ALG_SHA_512_224                     ((psa_algorithm_t) 0x0200000c)
/** SHA2-512/256 */
#define PSA_ALG_SHA_512_256                     ((psa_algorithm_t) 0x0200000d)
/** SHA3-224 */
#define PSA_ALG_SHA3_224                        ((psa_algorithm_t) 0x02000010)
/** SHA3-256 */
#define PSA_ALG_SHA3_256                        ((psa_algorithm_t) 0x02000011)
/** SHA3-384 */
#define PSA_ALG_SHA3_384                        ((psa_algorithm_t) 0x02000012)
/** SHA3-512 */
#define PSA_ALG_SHA3_512                        ((psa_algorithm_t) 0x02000013)
/** The first 512 bits (64 bytes) of the SHAKE256 output.
 *
 * This is the prehashing for Ed448ph (see #PSA_ALG_ED448PH). For other
 * scenarios where a hash function based on SHA3/SHAKE is desired, SHA3-512
 * has the same output size and a (theoretically) higher security strength.
 */
#define PSA_ALG_SHAKE256_512                    ((psa_algorithm_t) 0x02000015)

/** In a hash-and-sign algorithm policy, allow any hash algorithm.
 *
 * This value may be used to form the algorithm usage field of a policy
 * for a signature algorithm that is parametrized by a hash. The key
 * may then be used to perform operations using the same signature
 * algorithm parametrized with any supported hash.
 *
 * That is, suppose that `PSA_xxx_SIGNATURE` is one of the following macros:
 * - #PSA_ALG_RSA_PKCS1V15_SIGN, #PSA_ALG_RSA_PSS, #PSA_ALG_RSA_PSS_ANY_SALT,
 * - #PSA_ALG_ECDSA, #PSA_ALG_DETERMINISTIC_ECDSA.
 * Then you may create and use a key as follows:
 * - Set the key usage field using #PSA_ALG_ANY_HASH, for example:
 *   ```
 *   psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH); // or VERIFY
 *   psa_set_key_algorithm(&attributes, PSA_xxx_SIGNATURE(PSA_ALG_ANY_HASH));
 *   ```
 * - Import or generate key material.
 * - Call psa_sign_hash() or psa_verify_hash(), passing
 *   an algorithm built from `PSA_xxx_SIGNATURE` and a specific hash. Each
 *   call to sign or verify a message may use a different hash.
 *   ```
 *   psa_sign_hash(key, PSA_xxx_SIGNATURE(PSA_ALG_SHA_256), ...);
 *   psa_sign_hash(key, PSA_xxx_SIGNATURE(PSA_ALG_SHA_512), ...);
 *   psa_sign_hash(key, PSA_xxx_SIGNATURE(PSA_ALG_SHA3_256), ...);
 *   ```
 *
 * This value may not be used to build other algorithms that are
 * parametrized over a hash. For any valid use of this macro to build
 * an algorithm \c alg, #PSA_ALG_IS_HASH_AND_SIGN(\c alg) is true.
 *
 * This value may not be used to build an algorithm specification to
 * perform an operation. It is only valid to build policies.
 */
#define PSA_ALG_ANY_HASH                        ((psa_algorithm_t) 0x020000ff)

#define PSA_ALG_MAC_SUBCATEGORY_MASK            ((psa_algorithm_t) 0x00c00000)
#define PSA_ALG_HMAC_BASE                       ((psa_algorithm_t) 0x03800000)
/** Macro to build an HMAC algorithm.
 *
 * For example, #PSA_ALG_HMAC(#PSA_ALG_SHA_256) is HMAC-SHA-256.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding HMAC algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_HMAC(hash_alg)                                  \
    (PSA_ALG_HMAC_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

#define PSA_ALG_HMAC_GET_HASH(hmac_alg)                             \
    (PSA_ALG_CATEGORY_HASH | ((hmac_alg) & PSA_ALG_HASH_MASK))

/** Whether the specified algorithm is an HMAC algorithm.
 *
 * HMAC is a family of MAC algorithms that are based on a hash function.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is an HMAC algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_HMAC(alg)                                            \
    (((alg) & (PSA_ALG_CATEGORY_MASK | PSA_ALG_MAC_SUBCATEGORY_MASK)) == \
     PSA_ALG_HMAC_BASE)

/* In the encoding of a MAC algorithm, the bits corresponding to
 * PSA_ALG_MAC_TRUNCATION_MASK encode the length to which the MAC is
 * truncated. As an exception, the value 0 means the untruncated algorithm,
 * whatever its length is. The length is encoded in 6 bits, so it can
 * reach up to 63; the largest MAC is 64 bytes so its trivial truncation
 * to full length is correctly encoded as 0 and any non-trivial truncation
 * is correctly encoded as a value between 1 and 63. */
#define PSA_ALG_MAC_TRUNCATION_MASK             ((psa_algorithm_t) 0x003f0000)
#define PSA_MAC_TRUNCATION_OFFSET 16

/* In the encoding of a MAC algorithm, the bit corresponding to
 * #PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG encodes the fact that the algorithm
 * is a wildcard algorithm. A key with such wildcard algorithm as permitted
 * algorithm policy can be used with any algorithm corresponding to the
 * same base class and having a (potentially truncated) MAC length greater or
 * equal than the one encoded in #PSA_ALG_MAC_TRUNCATION_MASK. */
#define PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG   ((psa_algorithm_t) 0x00008000)

/** Macro to build a truncated MAC algorithm.
 *
 * A truncated MAC algorithm is identical to the corresponding MAC
 * algorithm except that the MAC value for the truncated algorithm
 * consists of only the first \p mac_length bytes of the MAC value
 * for the untruncated algorithm.
 *
 * \note    This macro may allow constructing algorithm identifiers that
 *          are not valid, either because the specified length is larger
 *          than the untruncated MAC or because the specified length is
 *          smaller than permitted by the implementation.
 *
 * \note    It is implementation-defined whether a truncated MAC that
 *          is truncated to the same length as the MAC of the untruncated
 *          algorithm is considered identical to the untruncated algorithm
 *          for policy comparison purposes.
 *
 * \param mac_alg       A MAC algorithm identifier (value of type
 *                      #psa_algorithm_t such that #PSA_ALG_IS_MAC(\p mac_alg)
 *                      is true). This may be a truncated or untruncated
 *                      MAC algorithm.
 * \param mac_length    Desired length of the truncated MAC in bytes.
 *                      This must be at most the full length of the MAC
 *                      and must be at least an implementation-specified
 *                      minimum. The implementation-specified minimum
 *                      shall not be zero.
 *
 * \return              The corresponding MAC algorithm with the specified
 *                      length.
 * \return              Unspecified if \p mac_alg is not a supported
 *                      MAC algorithm or if \p mac_length is too small or
 *                      too large for the specified MAC algorithm.
 */
#define PSA_ALG_TRUNCATED_MAC(mac_alg, mac_length)              \
    (((mac_alg) & ~(PSA_ALG_MAC_TRUNCATION_MASK |               \
                    PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG)) |   \
     ((mac_length) << PSA_MAC_TRUNCATION_OFFSET & PSA_ALG_MAC_TRUNCATION_MASK))

/** Macro to build the base MAC algorithm corresponding to a truncated
 * MAC algorithm.
 *
 * \param mac_alg       A MAC algorithm identifier (value of type
 *                      #psa_algorithm_t such that #PSA_ALG_IS_MAC(\p mac_alg)
 *                      is true). This may be a truncated or untruncated
 *                      MAC algorithm.
 *
 * \return              The corresponding base MAC algorithm.
 * \return              Unspecified if \p mac_alg is not a supported
 *                      MAC algorithm.
 */
#define PSA_ALG_FULL_LENGTH_MAC(mac_alg)                        \
    ((mac_alg) & ~(PSA_ALG_MAC_TRUNCATION_MASK |                \
                   PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG))

/** Length to which a MAC algorithm is truncated.
 *
 * \param mac_alg       A MAC algorithm identifier (value of type
 *                      #psa_algorithm_t such that #PSA_ALG_IS_MAC(\p mac_alg)
 *                      is true).
 *
 * \return              Length of the truncated MAC in bytes.
 * \return              0 if \p mac_alg is a non-truncated MAC algorithm.
 * \return              Unspecified if \p mac_alg is not a supported
 *                      MAC algorithm.
 */
#define PSA_MAC_TRUNCATED_LENGTH(mac_alg)                               \
    (((mac_alg) & PSA_ALG_MAC_TRUNCATION_MASK) >> PSA_MAC_TRUNCATION_OFFSET)

/** Macro to build a MAC minimum-MAC-length wildcard algorithm.
 *
 * A minimum-MAC-length MAC wildcard algorithm permits all MAC algorithms
 * sharing the same base algorithm, and where the (potentially truncated) MAC
 * length of the specific algorithm is equal to or larger then the wildcard
 * algorithm's minimum MAC length.
 *
 * \note    When setting the minimum required MAC length to less than the
 *          smallest MAC length allowed by the base algorithm, this effectively
 *          becomes an 'any-MAC-length-allowed' policy for that base algorithm.
 *
 * \param mac_alg         A MAC algorithm identifier (value of type
 *                        #psa_algorithm_t such that #PSA_ALG_IS_MAC(\p mac_alg)
 *                        is true).
 * \param min_mac_length  Desired minimum length of the message authentication
 *                        code in bytes. This must be at most the untruncated
 *                        length of the MAC and must be at least 1.
 *
 * \return                The corresponding MAC wildcard algorithm with the
 *                        specified minimum length.
 * \return                Unspecified if \p mac_alg is not a supported MAC
 *                        algorithm or if \p min_mac_length is less than 1 or
 *                        too large for the specified MAC algorithm.
 */
#define PSA_ALG_AT_LEAST_THIS_LENGTH_MAC(mac_alg, min_mac_length)   \
    (PSA_ALG_TRUNCATED_MAC(mac_alg, min_mac_length) |              \
     PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG)

#define PSA_ALG_CIPHER_MAC_BASE                 ((psa_algorithm_t) 0x03c00000)
/** The CBC-MAC construction over a block cipher
 *
 * \warning CBC-MAC is insecure in many cases.
 * A more secure mode, such as #PSA_ALG_CMAC, is recommended.
 */
#define PSA_ALG_CBC_MAC                         ((psa_algorithm_t) 0x03c00100)
/** The CMAC construction over a block cipher */
#define PSA_ALG_CMAC                            ((psa_algorithm_t) 0x03c00200)

/** Whether the specified algorithm is a MAC algorithm based on a block cipher.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a MAC algorithm based on a block cipher, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_BLOCK_CIPHER_MAC(alg)                                \
    (((alg) & (PSA_ALG_CATEGORY_MASK | PSA_ALG_MAC_SUBCATEGORY_MASK)) == \
     PSA_ALG_CIPHER_MAC_BASE)

#define PSA_ALG_CIPHER_STREAM_FLAG              ((psa_algorithm_t) 0x00800000)
#define PSA_ALG_CIPHER_FROM_BLOCK_FLAG          ((psa_algorithm_t) 0x00400000)

/** Whether the specified algorithm is a stream cipher.
 *
 * A stream cipher is a symmetric cipher that encrypts or decrypts messages
 * by applying a bitwise-xor with a stream of bytes that is generated
 * from a key.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a stream cipher algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier or if it is not a symmetric cipher algorithm.
 */
#define PSA_ALG_IS_STREAM_CIPHER(alg)            \
    (((alg) & (PSA_ALG_CATEGORY_MASK | PSA_ALG_CIPHER_STREAM_FLAG)) == \
     (PSA_ALG_CATEGORY_CIPHER | PSA_ALG_CIPHER_STREAM_FLAG))

/** The stream cipher mode of a stream cipher algorithm.
 *
 * The underlying stream cipher is determined by the key type.
 * - To use ChaCha20, use a key type of #PSA_KEY_TYPE_CHACHA20.
 */
#define PSA_ALG_STREAM_CIPHER                   ((psa_algorithm_t) 0x04800100)

/** The CTR stream cipher mode.
 *
 * CTR is a stream cipher which is built from a block cipher.
 * The underlying block cipher is determined by the key type.
 * For example, to use AES-128-CTR, use this algorithm with
 * a key of type #PSA_KEY_TYPE_AES and a length of 128 bits (16 bytes).
 */
#define PSA_ALG_CTR                             ((psa_algorithm_t) 0x04c01000)

/** The CFB stream cipher mode.
 *
 * The underlying block cipher is determined by the key type.
 */
#define PSA_ALG_CFB                             ((psa_algorithm_t) 0x04c01100)

/** The OFB stream cipher mode.
 *
 * The underlying block cipher is determined by the key type.
 */
#define PSA_ALG_OFB                             ((psa_algorithm_t) 0x04c01200)

/** The XTS cipher mode.
 *
 * XTS is a cipher mode which is built from a block cipher. It requires at
 * least one full block of input, but beyond this minimum the input
 * does not need to be a whole number of blocks.
 */
#define PSA_ALG_XTS                             ((psa_algorithm_t) 0x0440ff00)

/** The Electronic Code Book (ECB) mode of a block cipher, with no padding.
 *
 * \warning ECB mode does not protect the confidentiality of the encrypted data
 * except in extremely narrow circumstances. It is recommended that applications
 * only use ECB if they need to construct an operating mode that the
 * implementation does not provide. Implementations are encouraged to provide
 * the modes that applications need in preference to supporting direct access
 * to ECB.
 *
 * The underlying block cipher is determined by the key type.
 *
 * This symmetric cipher mode can only be used with messages whose lengths are a
 * multiple of the block size of the chosen block cipher.
 *
 * ECB mode does not accept an initialization vector (IV). When using a
 * multi-part cipher operation with this algorithm, psa_cipher_generate_iv()
 * and psa_cipher_set_iv() must not be called.
 */
#define PSA_ALG_ECB_NO_PADDING                  ((psa_algorithm_t) 0x04404400)

/** The CBC block cipher chaining mode, with no padding.
 *
 * The underlying block cipher is determined by the key type.
 *
 * This symmetric cipher mode can only be used with messages whose lengths
 * are whole number of blocks for the chosen block cipher.
 */
#define PSA_ALG_CBC_NO_PADDING                  ((psa_algorithm_t) 0x04404000)

/** The CBC block cipher chaining mode with PKCS#7 padding.
 *
 * The underlying block cipher is determined by the key type.
 *
 * This is the padding method defined by PKCS#7 (RFC 2315) &sect;10.3.
 */
#define PSA_ALG_CBC_PKCS7                       ((psa_algorithm_t) 0x04404100)

#define PSA_ALG_AEAD_FROM_BLOCK_FLAG            ((psa_algorithm_t) 0x00400000)

/** Whether the specified algorithm is an AEAD mode on a block cipher.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is an AEAD algorithm which is an AEAD mode based on
 *         a block cipher, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_AEAD_ON_BLOCK_CIPHER(alg)    \
    (((alg) & (PSA_ALG_CATEGORY_MASK | PSA_ALG_AEAD_FROM_BLOCK_FLAG)) == \
     (PSA_ALG_CATEGORY_AEAD | PSA_ALG_AEAD_FROM_BLOCK_FLAG))

/** The CCM authenticated encryption algorithm.
 *
 * The underlying block cipher is determined by the key type.
 */
#define PSA_ALG_CCM                             ((psa_algorithm_t) 0x05500100)

/** The CCM* cipher mode without authentication.
 *
 * This is CCM* as specified in IEEE 802.15.4 §7, with a tag length of 0.
 * For CCM* with a nonzero tag length, use the AEAD algorithm #PSA_ALG_CCM.
 *
 * The underlying block cipher is determined by the key type.
 *
 * Currently only 13-byte long IV's are supported.
 */
#define PSA_ALG_CCM_STAR_NO_TAG                 ((psa_algorithm_t) 0x04c01300)

/** The GCM authenticated encryption algorithm.
 *
 * The underlying block cipher is determined by the key type.
 */
#define PSA_ALG_GCM                             ((psa_algorithm_t) 0x05500200)

/** The Chacha20-Poly1305 AEAD algorithm.
 *
 * The ChaCha20_Poly1305 construction is defined in RFC 7539.
 *
 * Implementations must support 12-byte nonces, may support 8-byte nonces,
 * and should reject other sizes.
 *
 * Implementations must support 16-byte tags and should reject other sizes.
 */
#define PSA_ALG_CHACHA20_POLY1305               ((psa_algorithm_t) 0x05100500)

/* In the encoding of an AEAD algorithm, the bits corresponding to
 * PSA_ALG_AEAD_TAG_LENGTH_MASK encode the length of the AEAD tag.
 * The constants for default lengths follow this encoding.
 */
#define PSA_ALG_AEAD_TAG_LENGTH_MASK            ((psa_algorithm_t) 0x003f0000)
#define PSA_AEAD_TAG_LENGTH_OFFSET 16

/* In the encoding of an AEAD algorithm, the bit corresponding to
 * #PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG encodes the fact that the algorithm
 * is a wildcard algorithm. A key with such wildcard algorithm as permitted
 * algorithm policy can be used with any algorithm corresponding to the
 * same base class and having a tag length greater than or equal to the one
 * encoded in #PSA_ALG_AEAD_TAG_LENGTH_MASK. */
#define PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG  ((psa_algorithm_t) 0x00008000)

/** Macro to build a shortened AEAD algorithm.
 *
 * A shortened AEAD algorithm is similar to the corresponding AEAD
 * algorithm, but has an authentication tag that consists of fewer bytes.
 * Depending on the algorithm, the tag length may affect the calculation
 * of the ciphertext.
 *
 * \param aead_alg      An AEAD algorithm identifier (value of type
 *                      #psa_algorithm_t such that #PSA_ALG_IS_AEAD(\p aead_alg)
 *                      is true).
 * \param tag_length    Desired length of the authentication tag in bytes.
 *
 * \return              The corresponding AEAD algorithm with the specified
 *                      length.
 * \return              Unspecified if \p aead_alg is not a supported
 *                      AEAD algorithm or if \p tag_length is not valid
 *                      for the specified AEAD algorithm.
 */
#define PSA_ALG_AEAD_WITH_SHORTENED_TAG(aead_alg, tag_length)           \
    (((aead_alg) & ~(PSA_ALG_AEAD_TAG_LENGTH_MASK |                     \
                     PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG)) |         \
     ((tag_length) << PSA_AEAD_TAG_LENGTH_OFFSET &                      \
        PSA_ALG_AEAD_TAG_LENGTH_MASK))

/** Retrieve the tag length of a specified AEAD algorithm
 *
 * \param aead_alg      An AEAD algorithm identifier (value of type
 *                      #psa_algorithm_t such that #PSA_ALG_IS_AEAD(\p aead_alg)
 *                      is true).
 *
 * \return              The tag length specified by the input algorithm.
 * \return              Unspecified if \p aead_alg is not a supported
 *                      AEAD algorithm.
 */
#define PSA_ALG_AEAD_GET_TAG_LENGTH(aead_alg)                           \
    (((aead_alg) & PSA_ALG_AEAD_TAG_LENGTH_MASK) >>                     \
     PSA_AEAD_TAG_LENGTH_OFFSET)

/** Calculate the corresponding AEAD algorithm with the default tag length.
 *
 * \param aead_alg      An AEAD algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_AEAD(\p aead_alg) is true).
 *
 * \return              The corresponding AEAD algorithm with the default
 *                      tag length for that algorithm.
 */
#define PSA_ALG_AEAD_WITH_DEFAULT_LENGTH_TAG(aead_alg)                   \
    (                                                                    \
        PSA_ALG_AEAD_WITH_DEFAULT_LENGTH_TAG_CASE(aead_alg, PSA_ALG_CCM) \
        PSA_ALG_AEAD_WITH_DEFAULT_LENGTH_TAG_CASE(aead_alg, PSA_ALG_GCM) \
        PSA_ALG_AEAD_WITH_DEFAULT_LENGTH_TAG_CASE(aead_alg, PSA_ALG_CHACHA20_POLY1305) \
        0)
#define PSA_ALG_AEAD_WITH_DEFAULT_LENGTH_TAG_CASE(aead_alg, ref)         \
    PSA_ALG_AEAD_WITH_SHORTENED_TAG(aead_alg, 0) ==                      \
    PSA_ALG_AEAD_WITH_SHORTENED_TAG(ref, 0) ?                            \
    ref :

/** Macro to build an AEAD minimum-tag-length wildcard algorithm.
 *
 * A minimum-tag-length AEAD wildcard algorithm permits all AEAD algorithms
 * sharing the same base algorithm, and where the tag length of the specific
 * algorithm is equal to or larger then the minimum tag length specified by the
 * wildcard algorithm.
 *
 * \note    When setting the minimum required tag length to less than the
 *          smallest tag length allowed by the base algorithm, this effectively
 *          becomes an 'any-tag-length-allowed' policy for that base algorithm.
 *
 * \param aead_alg        An AEAD algorithm identifier (value of type
 *                        #psa_algorithm_t such that
 *                        #PSA_ALG_IS_AEAD(\p aead_alg) is true).
 * \param min_tag_length  Desired minimum length of the authentication tag in
 *                        bytes. This must be at least 1 and at most the largest
 *                        allowed tag length of the algorithm.
 *
 * \return                The corresponding AEAD wildcard algorithm with the
 *                        specified minimum length.
 * \return                Unspecified if \p aead_alg is not a supported
 *                        AEAD algorithm or if \p min_tag_length is less than 1
 *                        or too large for the specified AEAD algorithm.
 */
#define PSA_ALG_AEAD_WITH_AT_LEAST_THIS_LENGTH_TAG(aead_alg, min_tag_length) \
    (PSA_ALG_AEAD_WITH_SHORTENED_TAG(aead_alg, min_tag_length) |            \
     PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG)

#define PSA_ALG_RSA_PKCS1V15_SIGN_BASE          ((psa_algorithm_t) 0x06000200)
/** RSA PKCS#1 v1.5 signature with hashing.
 *
 * This is the signature scheme defined by RFC 8017
 * (PKCS#1: RSA Cryptography Specifications) under the name
 * RSASSA-PKCS1-v1_5.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *                      This includes #PSA_ALG_ANY_HASH
 *                      when specifying the algorithm in a usage policy.
 *
 * \return              The corresponding RSA PKCS#1 v1.5 signature algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_RSA_PKCS1V15_SIGN(hash_alg)                             \
    (PSA_ALG_RSA_PKCS1V15_SIGN_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
/** Raw PKCS#1 v1.5 signature.
 *
 * The input to this algorithm is the DigestInfo structure used by
 * RFC 8017 (PKCS#1: RSA Cryptography Specifications), &sect;9.2
 * steps 3&ndash;6.
 */
#define PSA_ALG_RSA_PKCS1V15_SIGN_RAW PSA_ALG_RSA_PKCS1V15_SIGN_BASE
#define PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg)                               \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_RSA_PKCS1V15_SIGN_BASE)

#define PSA_ALG_RSA_PSS_BASE               ((psa_algorithm_t) 0x06000300)
#define PSA_ALG_RSA_PSS_ANY_SALT_BASE      ((psa_algorithm_t) 0x06001300)
/** RSA PSS signature with hashing.
 *
 * This is the signature scheme defined by RFC 8017
 * (PKCS#1: RSA Cryptography Specifications) under the name
 * RSASSA-PSS, with the message generation function MGF1, and with
 * a salt length equal to the length of the hash, or the largest
 * possible salt length for the algorithm and key size if that is
 * smaller than the hash length. The specified hash algorithm is
 * used to hash the input message, to create the salted hash, and
 * for the mask generation.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *                      This includes #PSA_ALG_ANY_HASH
 *                      when specifying the algorithm in a usage policy.
 *
 * \return              The corresponding RSA PSS signature algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_RSA_PSS(hash_alg)                               \
    (PSA_ALG_RSA_PSS_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

/** RSA PSS signature with hashing with relaxed verification.
 *
 * This algorithm has the same behavior as #PSA_ALG_RSA_PSS when signing,
 * but allows an arbitrary salt length (including \c 0) when verifying a
 * signature.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *                      This includes #PSA_ALG_ANY_HASH
 *                      when specifying the algorithm in a usage policy.
 *
 * \return              The corresponding RSA PSS signature algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_RSA_PSS_ANY_SALT(hash_alg)                      \
    (PSA_ALG_RSA_PSS_ANY_SALT_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

/** Whether the specified algorithm is RSA PSS with standard salt.
 *
 * \param alg           An algorithm value or an algorithm policy wildcard.
 *
 * \return              1 if \p alg is of the form
 *                      #PSA_ALG_RSA_PSS(\c hash_alg),
 *                      where \c hash_alg is a hash algorithm or
 *                      #PSA_ALG_ANY_HASH. 0 otherwise.
 *                      This macro may return either 0 or 1 if \p alg is not
 *                      a supported algorithm identifier or policy.
 */
#define PSA_ALG_IS_RSA_PSS_STANDARD_SALT(alg)                   \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_RSA_PSS_BASE)

/** Whether the specified algorithm is RSA PSS with any salt.
 *
 * \param alg           An algorithm value or an algorithm policy wildcard.
 *
 * \return              1 if \p alg is of the form
 *                      #PSA_ALG_RSA_PSS_ANY_SALT_BASE(\c hash_alg),
 *                      where \c hash_alg is a hash algorithm or
 *                      #PSA_ALG_ANY_HASH. 0 otherwise.
 *                      This macro may return either 0 or 1 if \p alg is not
 *                      a supported algorithm identifier or policy.
 */
#define PSA_ALG_IS_RSA_PSS_ANY_SALT(alg)                                \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_RSA_PSS_ANY_SALT_BASE)

/** Whether the specified algorithm is RSA PSS.
 *
 * This includes any of the RSA PSS algorithm variants, regardless of the
 * constraints on salt length.
 *
 * \param alg           An algorithm value or an algorithm policy wildcard.
 *
 * \return              1 if \p alg is of the form
 *                      #PSA_ALG_RSA_PSS(\c hash_alg) or
 *                      #PSA_ALG_RSA_PSS_ANY_SALT_BASE(\c hash_alg),
 *                      where \c hash_alg is a hash algorithm or
 *                      #PSA_ALG_ANY_HASH. 0 otherwise.
 *                      This macro may return either 0 or 1 if \p alg is not
 *                      a supported algorithm identifier or policy.
 */
#define PSA_ALG_IS_RSA_PSS(alg)                                 \
    (PSA_ALG_IS_RSA_PSS_STANDARD_SALT(alg) ||                   \
     PSA_ALG_IS_RSA_PSS_ANY_SALT(alg))

#define PSA_ALG_ECDSA_BASE                      ((psa_algorithm_t) 0x06000600)
/** ECDSA signature with hashing.
 *
 * This is the ECDSA signature scheme defined by ANSI X9.62,
 * with a random per-message secret number (*k*).
 *
 * The representation of the signature as a byte string consists of
 * the concatenation of the signature values *r* and *s*. Each of
 * *r* and *s* is encoded as an *N*-octet string, where *N* is the length
 * of the base point of the curve in octets. Each value is represented
 * in big-endian order (most significant octet first).
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *                      This includes #PSA_ALG_ANY_HASH
 *                      when specifying the algorithm in a usage policy.
 *
 * \return              The corresponding ECDSA signature algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_ECDSA(hash_alg)                                 \
    (PSA_ALG_ECDSA_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
/** ECDSA signature without hashing.
 *
 * This is the same signature scheme as #PSA_ALG_ECDSA(), but
 * without specifying a hash algorithm. This algorithm may only be
 * used to sign or verify a sequence of bytes that should be an
 * already-calculated hash. Note that the input is padded with
 * zeros on the left or truncated on the left as required to fit
 * the curve size.
 */
#define PSA_ALG_ECDSA_ANY PSA_ALG_ECDSA_BASE
#define PSA_ALG_DETERMINISTIC_ECDSA_BASE        ((psa_algorithm_t) 0x06000700)
/** Deterministic ECDSA signature with hashing.
 *
 * This is the deterministic ECDSA signature scheme defined by RFC 6979.
 *
 * The representation of a signature is the same as with #PSA_ALG_ECDSA().
 *
 * Note that when this algorithm is used for verification, signatures
 * made with randomized ECDSA (#PSA_ALG_ECDSA(\p hash_alg)) with the
 * same private key are accepted. In other words,
 * #PSA_ALG_DETERMINISTIC_ECDSA(\p hash_alg) differs from
 * #PSA_ALG_ECDSA(\p hash_alg) only for signature, not for verification.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *                      This includes #PSA_ALG_ANY_HASH
 *                      when specifying the algorithm in a usage policy.
 *
 * \return              The corresponding deterministic ECDSA signature
 *                      algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_DETERMINISTIC_ECDSA(hash_alg)                           \
    (PSA_ALG_DETERMINISTIC_ECDSA_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
#define PSA_ALG_ECDSA_DETERMINISTIC_FLAG        ((psa_algorithm_t) 0x00000100)
#define PSA_ALG_IS_ECDSA(alg)                                           \
    (((alg) & ~PSA_ALG_HASH_MASK & ~PSA_ALG_ECDSA_DETERMINISTIC_FLAG) ==  \
     PSA_ALG_ECDSA_BASE)
#define PSA_ALG_ECDSA_IS_DETERMINISTIC(alg)             \
    (((alg) & PSA_ALG_ECDSA_DETERMINISTIC_FLAG) != 0)
#define PSA_ALG_IS_DETERMINISTIC_ECDSA(alg)                             \
    (PSA_ALG_IS_ECDSA(alg) && PSA_ALG_ECDSA_IS_DETERMINISTIC(alg))
#define PSA_ALG_IS_RANDOMIZED_ECDSA(alg)                                \
    (PSA_ALG_IS_ECDSA(alg) && !PSA_ALG_ECDSA_IS_DETERMINISTIC(alg))

/** Edwards-curve digital signature algorithm without prehashing (PureEdDSA),
 * using standard parameters.
 *
 * Contexts are not supported in the current version of this specification
 * because there is no suitable signature interface that can take the
 * context as a parameter. A future version of this specification may add
 * suitable functions and extend this algorithm to support contexts.
 *
 * PureEdDSA requires an elliptic curve key on a twisted Edwards curve.
 * In this specification, the following curves are supported:
 * - #PSA_ECC_FAMILY_TWISTED_EDWARDS, 255-bit: Ed25519 as specified
 *   in RFC 8032.
 *   The curve is Edwards25519.
 *   The hash function used internally is SHA-512.
 * - #PSA_ECC_FAMILY_TWISTED_EDWARDS, 448-bit: Ed448 as specified
 *   in RFC 8032.
 *   The curve is Edwards448.
 *   The hash function used internally is the first 114 bytes of the
 *   SHAKE256 output.
 *
 * This algorithm can be used with psa_sign_message() and
 * psa_verify_message(). Since there is no prehashing, it cannot be used
 * with psa_sign_hash() or psa_verify_hash().
 *
 * The signature format is the concatenation of R and S as defined by
 * RFC 8032 §5.1.6 and §5.2.6 (a 64-byte string for Ed25519, a 114-byte
 * string for Ed448).
 */
#define PSA_ALG_PURE_EDDSA                      ((psa_algorithm_t) 0x06000800)

#define PSA_ALG_HASH_EDDSA_BASE                 ((psa_algorithm_t) 0x06000900)
#define PSA_ALG_IS_HASH_EDDSA(alg)              \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HASH_EDDSA_BASE)

/** Edwards-curve digital signature algorithm with prehashing (HashEdDSA),
 * using SHA-512 and the Edwards25519 curve.
 *
 * See #PSA_ALG_PURE_EDDSA regarding context support and the signature format.
 *
 * This algorithm is Ed25519 as specified in RFC 8032.
 * The curve is Edwards25519.
 * The prehash is SHA-512.
 * The hash function used internally is SHA-512.
 *
 * This is a hash-and-sign algorithm: to calculate a signature,
 * you can either:
 * - call psa_sign_message() on the message;
 * - or calculate the SHA-512 hash of the message
 *   with psa_hash_compute()
 *   or with a multi-part hash operation started with psa_hash_setup(),
 *   using the hash algorithm #PSA_ALG_SHA_512,
 *   then sign the calculated hash with psa_sign_hash().
 * Verifying a signature is similar, using psa_verify_message() or
 * psa_verify_hash() instead of the signature function.
 */
#define PSA_ALG_ED25519PH                               \
    (PSA_ALG_HASH_EDDSA_BASE | (PSA_ALG_SHA_512 & PSA_ALG_HASH_MASK))

/** Edwards-curve digital signature algorithm with prehashing (HashEdDSA),
 * using SHAKE256 and the Edwards448 curve.
 *
 * See #PSA_ALG_PURE_EDDSA regarding context support and the signature format.
 *
 * This algorithm is Ed448 as specified in RFC 8032.
 * The curve is Edwards448.
 * The prehash is the first 64 bytes of the SHAKE256 output.
 * The hash function used internally is the first 114 bytes of the
 * SHAKE256 output.
 *
 * This is a hash-and-sign algorithm: to calculate a signature,
 * you can either:
 * - call psa_sign_message() on the message;
 * - or calculate the first 64 bytes of the SHAKE256 output of the message
 *   with psa_hash_compute()
 *   or with a multi-part hash operation started with psa_hash_setup(),
 *   using the hash algorithm #PSA_ALG_SHAKE256_512,
 *   then sign the calculated hash with psa_sign_hash().
 * Verifying a signature is similar, using psa_verify_message() or
 * psa_verify_hash() instead of the signature function.
 */
#define PSA_ALG_ED448PH                                 \
    (PSA_ALG_HASH_EDDSA_BASE | (PSA_ALG_SHAKE256_512 & PSA_ALG_HASH_MASK))

/* Default definition, to be overridden if the library is extended with
 * more hash-and-sign algorithms that we want to keep out of this header
 * file. */
#define PSA_ALG_IS_VENDOR_HASH_AND_SIGN(alg) 0

/** Whether the specified algorithm is a signature algorithm that can be used
 * with psa_sign_hash() and psa_verify_hash().
 *
 * This encompasses all strict hash-and-sign algorithms categorized by
 * PSA_ALG_IS_HASH_AND_SIGN(), as well as algorithms that follow the
 * paradigm more loosely:
 * - #PSA_ALG_RSA_PKCS1V15_SIGN_RAW (expects its input to be an encoded hash)
 * - #PSA_ALG_ECDSA_ANY (doesn't specify what kind of hash the input is)
 *
 * \param alg An algorithm identifier (value of type psa_algorithm_t).
 *
 * \return 1 if alg is a signature algorithm that can be used to sign a
 *         hash. 0 if alg is a signature algorithm that can only be used
 *         to sign a message. 0 if alg is not a signature algorithm.
 *         This macro can return either 0 or 1 if alg is not a
 *         supported algorithm identifier.
 */
#define PSA_ALG_IS_SIGN_HASH(alg)                                       \
    (PSA_ALG_IS_RSA_PSS(alg) || PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) ||    \
     PSA_ALG_IS_ECDSA(alg) || PSA_ALG_IS_HASH_EDDSA(alg) ||             \
     PSA_ALG_IS_VENDOR_HASH_AND_SIGN(alg))

/** Whether the specified algorithm is a signature algorithm that can be used
 * with psa_sign_message() and psa_verify_message().
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if alg is a signature algorithm that can be used to sign a
 *         message. 0 if \p alg is a signature algorithm that can only be used
 *         to sign an already-calculated hash. 0 if \p alg is not a signature
 *         algorithm. This macro can return either 0 or 1 if \p alg is not a
 *         supported algorithm identifier.
 */
#define PSA_ALG_IS_SIGN_MESSAGE(alg)                                    \
    (PSA_ALG_IS_SIGN_HASH(alg) || (alg) == PSA_ALG_PURE_EDDSA)

/** Whether the specified algorithm is a hash-and-sign algorithm.
 *
 * Hash-and-sign algorithms are asymmetric (public-key) signature algorithms
 * structured in two parts: first the calculation of a hash in a way that
 * does not depend on the key, then the calculation of a signature from the
 * hash value and the key. Hash-and-sign algorithms encode the hash
 * used for the hashing step, and you can call #PSA_ALG_SIGN_GET_HASH
 * to extract this algorithm.
 *
 * Thus, for a hash-and-sign algorithm,
 * `psa_sign_message(key, alg, input, ...)` is equivalent to
 * ```
 * psa_hash_compute(PSA_ALG_SIGN_GET_HASH(alg), input, ..., hash, ...);
 * psa_sign_hash(key, alg, hash, ..., signature, ...);
 * ```
 * Most usefully, separating the hash from the signature allows the hash
 * to be calculated in multiple steps with psa_hash_setup(), psa_hash_update()
 * and psa_hash_finish(). Likewise psa_verify_message() is equivalent to
 * calculating the hash and then calling psa_verify_hash().
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a hash-and-sign algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_HASH_AND_SIGN(alg)                                   \
    (PSA_ALG_IS_SIGN_HASH(alg) &&                                       \
     ((alg) & PSA_ALG_HASH_MASK) != 0)

/** Get the hash used by a hash-and-sign signature algorithm.
 *
 * A hash-and-sign algorithm is a signature algorithm which is
 * composed of two phases: first a hashing phase which does not use
 * the key and produces a hash of the input message, then a signing
 * phase which only uses the hash and the key and not the message
 * itself.
 *
 * \param alg   A signature algorithm (\c PSA_ALG_XXX value such that
 *              #PSA_ALG_IS_SIGN(\p alg) is true).
 *
 * \return      The underlying hash algorithm if \p alg is a hash-and-sign
 *              algorithm.
 * \return      0 if \p alg is a signature algorithm that does not
 *              follow the hash-and-sign structure.
 * \return      Unspecified if \p alg is not a signature algorithm or
 *              if it is not supported by the implementation.
 */
#define PSA_ALG_SIGN_GET_HASH(alg)                                     \
    (PSA_ALG_IS_HASH_AND_SIGN(alg) ?                                   \
     ((alg) & PSA_ALG_HASH_MASK) | PSA_ALG_CATEGORY_HASH :             \
     0)

/** RSA PKCS#1 v1.5 encryption.
 *
 * \warning     Calling psa_asymmetric_decrypt() with this algorithm as a
 *              parameter is considered an inherently dangerous function
 *              (CWE-242). Unless it is used in a side channel free and safe
 *              way (eg. implementing the TLS protocol as per 7.4.7.1 of
 *              RFC 5246), the calling code is vulnerable.
 *
 */
#define PSA_ALG_RSA_PKCS1V15_CRYPT              ((psa_algorithm_t) 0x07000200)

#define PSA_ALG_RSA_OAEP_BASE                   ((psa_algorithm_t) 0x07000300)
/** RSA OAEP encryption.
 *
 * This is the encryption scheme defined by RFC 8017
 * (PKCS#1: RSA Cryptography Specifications) under the name
 * RSAES-OAEP, with the message generation function MGF1.
 *
 * \param hash_alg      The hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true) to use
 *                      for MGF1.
 *
 * \return              The corresponding RSA OAEP encryption algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_RSA_OAEP(hash_alg)                              \
    (PSA_ALG_RSA_OAEP_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
#define PSA_ALG_IS_RSA_OAEP(alg)                                \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_RSA_OAEP_BASE)
#define PSA_ALG_RSA_OAEP_GET_HASH(alg)                          \
    (PSA_ALG_IS_RSA_OAEP(alg) ?                                 \
     ((alg) & PSA_ALG_HASH_MASK) | PSA_ALG_CATEGORY_HASH :      \
     0)

#define PSA_ALG_HKDF_BASE                       ((psa_algorithm_t) 0x08000100)
/** Macro to build an HKDF algorithm.
 *
 * For example, `PSA_ALG_HKDF(PSA_ALG_SHA_256)` is HKDF using HMAC-SHA-256.
 *
 * This key derivation algorithm uses the following inputs:
 * - #PSA_KEY_DERIVATION_INPUT_SALT is the salt used in the "extract" step.
 *   It is optional; if omitted, the derivation uses an empty salt.
 * - #PSA_KEY_DERIVATION_INPUT_SECRET is the secret key used in the "extract" step.
 * - #PSA_KEY_DERIVATION_INPUT_INFO is the info string used in the "expand" step.
 * You must pass #PSA_KEY_DERIVATION_INPUT_SALT before #PSA_KEY_DERIVATION_INPUT_SECRET.
 * You may pass #PSA_KEY_DERIVATION_INPUT_INFO at any time after steup and before
 * starting to generate output.
 *
 *  \warning  HKDF processes the salt as follows: first hash it with hash_alg
 *  if the salt is longer than the block size of the hash algorithm; then
 *  pad with null bytes up to the block size. As a result, it is possible
 *  for distinct salt inputs to result in the same outputs. To ensure
 *  unique outputs, it is recommended to use a fixed length for salt values.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding HKDF algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_HKDF(hash_alg)                                  \
    (PSA_ALG_HKDF_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
/** Whether the specified algorithm is an HKDF algorithm.
 *
 * HKDF is a family of key derivation algorithms that are based on a hash
 * function and the HMAC construction.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is an HKDF algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_HKDF(alg)                            \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_BASE)
#define PSA_ALG_HKDF_GET_HASH(hkdf_alg)                         \
    (PSA_ALG_CATEGORY_HASH | ((hkdf_alg) & PSA_ALG_HASH_MASK))

#define PSA_ALG_HKDF_EXTRACT_BASE                       ((psa_algorithm_t) 0x08000400)
/** Macro to build an HKDF-Extract algorithm.
 *
 * For example, `PSA_ALG_HKDF_EXTRACT(PSA_ALG_SHA_256)` is
 * HKDF-Extract using HMAC-SHA-256.
 *
 * This key derivation algorithm uses the following inputs:
 *  - PSA_KEY_DERIVATION_INPUT_SALT is the salt.
 *  - PSA_KEY_DERIVATION_INPUT_SECRET is the input keying material used in the
 *    "extract" step.
 * The inputs are mandatory and must be passed in the order above.
 * Each input may only be passed once.
 *
 *  \warning HKDF-Extract is not meant to be used on its own. PSA_ALG_HKDF
 *  should be used instead if possible. PSA_ALG_HKDF_EXTRACT is provided
 *  as a separate algorithm for the sake of protocols that use it as a
 *  building block. It may also be a slight performance optimization
 *  in applications that use HKDF with the same salt and key but many
 *  different info strings.
 *
 *  \warning  HKDF processes the salt as follows: first hash it with hash_alg
 *  if the salt is longer than the block size of the hash algorithm; then
 *  pad with null bytes up to the block size. As a result, it is possible
 *  for distinct salt inputs to result in the same outputs. To ensure
 *  unique outputs, it is recommended to use a fixed length for salt values.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding HKDF-Extract algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_HKDF_EXTRACT(hash_alg)                                  \
    (PSA_ALG_HKDF_EXTRACT_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
/** Whether the specified algorithm is an HKDF-Extract algorithm.
 *
 * HKDF-Extract is a family of key derivation algorithms that are based
 * on a hash function and the HMAC construction.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is an HKDF-Extract algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_HKDF_EXTRACT(alg)                            \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_EXTRACT_BASE)

#define PSA_ALG_HKDF_EXPAND_BASE                       ((psa_algorithm_t) 0x08000500)
/** Macro to build an HKDF-Expand algorithm.
 *
 * For example, `PSA_ALG_HKDF_EXPAND(PSA_ALG_SHA_256)` is
 * HKDF-Expand using HMAC-SHA-256.
 *
 * This key derivation algorithm uses the following inputs:
 *  - PSA_KEY_DERIVATION_INPUT_SECRET is the pseudorandom key (PRK).
 *  - PSA_KEY_DERIVATION_INPUT_INFO is the info string.
 *
 *  The inputs are mandatory and must be passed in the order above.
 *  Each input may only be passed once.
 *
 *  \warning HKDF-Expand is not meant to be used on its own. `PSA_ALG_HKDF`
 *  should be used instead if possible. `PSA_ALG_HKDF_EXPAND` is provided as
 *  a separate algorithm for the sake of protocols that use it as a building
 *  block. It may also be a slight performance optimization in applications
 *  that use HKDF with the same salt and key but many different info strings.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding HKDF-Expand algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_HKDF_EXPAND(hash_alg)                                  \
    (PSA_ALG_HKDF_EXPAND_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))
/** Whether the specified algorithm is an HKDF-Expand algorithm.
 *
 * HKDF-Expand is a family of key derivation algorithms that are based
 * on a hash function and the HMAC construction.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is an HKDF-Expand algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_HKDF_EXPAND(alg)                            \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_EXPAND_BASE)

/** Whether the specified algorithm is an HKDF or HKDF-Extract or
 *  HKDF-Expand algorithm.
 *
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is any HKDF type algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_ANY_HKDF(alg)                                   \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_BASE ||          \
     ((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_EXTRACT_BASE ||  \
     ((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_HKDF_EXPAND_BASE)

#define PSA_ALG_TLS12_PRF_BASE                  ((psa_algorithm_t) 0x08000200)
/** Macro to build a TLS-1.2 PRF algorithm.
 *
 * TLS 1.2 uses a custom pseudorandom function (PRF) for key schedule,
 * specified in Section 5 of RFC 5246. It is based on HMAC and can be
 * used with either SHA-256 or SHA-384.
 *
 * This key derivation algorithm uses the following inputs, which must be
 * passed in the order given here:
 * - #PSA_KEY_DERIVATION_INPUT_SEED is the seed.
 * - #PSA_KEY_DERIVATION_INPUT_SECRET is the secret key.
 * - #PSA_KEY_DERIVATION_INPUT_LABEL is the label.
 *
 * For the application to TLS-1.2 key expansion, the seed is the
 * concatenation of ServerHello.Random + ClientHello.Random,
 * and the label is "key expansion".
 *
 * For example, `PSA_ALG_TLS12_PRF(PSA_ALG_SHA_256)` represents the
 * TLS 1.2 PRF using HMAC-SHA-256.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding TLS-1.2 PRF algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_TLS12_PRF(hash_alg)                                  \
    (PSA_ALG_TLS12_PRF_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

/** Whether the specified algorithm is a TLS-1.2 PRF algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is a TLS-1.2 PRF algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_TLS12_PRF(alg)                                    \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_TLS12_PRF_BASE)
#define PSA_ALG_TLS12_PRF_GET_HASH(hkdf_alg)                         \
    (PSA_ALG_CATEGORY_HASH | ((hkdf_alg) & PSA_ALG_HASH_MASK))

#define PSA_ALG_TLS12_PSK_TO_MS_BASE            ((psa_algorithm_t) 0x08000300)
/** Macro to build a TLS-1.2 PSK-to-MasterSecret algorithm.
 *
 * In a pure-PSK handshake in TLS 1.2, the master secret is derived
 * from the PreSharedKey (PSK) through the application of padding
 * (RFC 4279, Section 2) and the TLS-1.2 PRF (RFC 5246, Section 5).
 * The latter is based on HMAC and can be used with either SHA-256
 * or SHA-384.
 *
 * This key derivation algorithm uses the following inputs, which must be
 * passed in the order given here:
 * - #PSA_KEY_DERIVATION_INPUT_SEED is the seed.
 * - #PSA_KEY_DERIVATION_INPUT_OTHER_SECRET is the other secret for the
 *   computation of the premaster secret. This input is optional;
 *   if omitted, it defaults to a string of null bytes with the same length
 *   as the secret (PSK) input.
 * - #PSA_KEY_DERIVATION_INPUT_SECRET is the secret key.
 * - #PSA_KEY_DERIVATION_INPUT_LABEL is the label.
 *
 * For the application to TLS-1.2, the seed (which is
 * forwarded to the TLS-1.2 PRF) is the concatenation of the
 * ClientHello.Random + ServerHello.Random,
 * the label is "master secret" or "extended master secret" and
 * the other secret depends on the key exchange specified in the cipher suite:
 * - for a plain PSK cipher suite (RFC 4279, Section 2), omit
 *   PSA_KEY_DERIVATION_INPUT_OTHER_SECRET
 * - for a DHE-PSK (RFC 4279, Section 3) or ECDHE-PSK cipher suite
 *   (RFC 5489, Section 2), the other secret should be the output of the
 *   PSA_ALG_FFDH or PSA_ALG_ECDH key agreement performed with the peer.
 *   The recommended way to pass this input is to use a key derivation
 *   algorithm constructed as
 *   PSA_ALG_KEY_AGREEMENT(ka_alg, PSA_ALG_TLS12_PSK_TO_MS(hash_alg))
 *   and to call psa_key_derivation_key_agreement(). Alternatively,
 *   this input may be an output of `psa_raw_key_agreement()` passed with
 *   psa_key_derivation_input_bytes(), or an equivalent input passed with
 *   psa_key_derivation_input_bytes() or psa_key_derivation_input_key().
 * - for a RSA-PSK cipher suite (RFC 4279, Section 4), the other secret
 *   should be the 48-byte client challenge (the PreMasterSecret of
 *   (RFC 5246, Section 7.4.7.1)) concatenation of the TLS version and
 *   a 46-byte random string chosen by the client. On the server, this is
 *   typically an output of psa_asymmetric_decrypt() using
 *   PSA_ALG_RSA_PKCS1V15_CRYPT, passed to the key derivation operation
 *   with `psa_key_derivation_input_bytes()`.
 *
 * For example, `PSA_ALG_TLS12_PSK_TO_MS(PSA_ALG_SHA_256)` represents the
 * TLS-1.2 PSK to MasterSecret derivation PRF using HMAC-SHA-256.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding TLS-1.2 PSK to MS algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_TLS12_PSK_TO_MS(hash_alg)                                  \
    (PSA_ALG_TLS12_PSK_TO_MS_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

/** Whether the specified algorithm is a TLS-1.2 PSK to MS algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is a TLS-1.2 PSK to MS algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_TLS12_PSK_TO_MS(alg)                                    \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_TLS12_PSK_TO_MS_BASE)
#define PSA_ALG_TLS12_PSK_TO_MS_GET_HASH(hkdf_alg)                         \
    (PSA_ALG_CATEGORY_HASH | ((hkdf_alg) & PSA_ALG_HASH_MASK))

/* The TLS 1.2 ECJPAKE-to-PMS KDF. It takes the shared secret K (an EC point
 * in case of EC J-PAKE) and calculates SHA256(K.X) that the rest of TLS 1.2
 * will use to derive the session secret, as defined by step 2 of
 * https://datatracker.ietf.org/doc/html/draft-cragie-tls-ecjpake-01#section-8.7.
 * Uses PSA_ALG_SHA_256.
 * This function takes a single input:
 * #PSA_KEY_DERIVATION_INPUT_SECRET is the shared secret K from EC J-PAKE.
 * The only supported curve is secp256r1 (the 256-bit curve in
 * #PSA_ECC_FAMILY_SECP_R1), so the input must be exactly 65 bytes.
 * The output has to be read as a single chunk of 32 bytes, defined as
 * PSA_TLS12_ECJPAKE_TO_PMS_DATA_SIZE.
 */
#define PSA_ALG_TLS12_ECJPAKE_TO_PMS            ((psa_algorithm_t) 0x08000609)

/* This flag indicates whether the key derivation algorithm is suitable for
 * use on low-entropy secrets such as password - these algorithms are also
 * known as key stretching or password hashing schemes. These are also the
 * algorithms that accepts inputs of type #PSA_KEY_DERIVATION_INPUT_PASSWORD.
 *
 * Those algorithms cannot be combined with a key agreement algorithm.
 */
#define PSA_ALG_KEY_DERIVATION_STRETCHING_FLAG  ((psa_algorithm_t) 0x00800000)

#define PSA_ALG_PBKDF2_HMAC_BASE                ((psa_algorithm_t) 0x08800100)
/** Macro to build a PBKDF2-HMAC password hashing / key stretching algorithm.
 *
 * PBKDF2 is defined by PKCS#5, republished as RFC 8018 (section 5.2).
 * This macro specifies the PBKDF2 algorithm constructed using a PRF based on
 * HMAC with the specified hash.
 * For example, `PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256)` specifies PBKDF2
 * using the PRF HMAC-SHA-256.
 *
 * This key derivation algorithm uses the following inputs, which must be
 * provided in the following order:
 * - #PSA_KEY_DERIVATION_INPUT_COST is the iteration count.
 *   This input step must be used exactly once.
 * - #PSA_KEY_DERIVATION_INPUT_SALT is the salt.
 *   This input step must be used one or more times; if used several times, the
 *   inputs will be concatenated. This can be used to build the final salt
 *   from multiple sources, both public and secret (also known as pepper).
 * - #PSA_KEY_DERIVATION_INPUT_PASSWORD is the password to be hashed.
 *   This input step must be used exactly once.
 *
 * \param hash_alg      A hash algorithm (\c PSA_ALG_XXX value such that
 *                      #PSA_ALG_IS_HASH(\p hash_alg) is true).
 *
 * \return              The corresponding PBKDF2-HMAC-XXX algorithm.
 * \return              Unspecified if \p hash_alg is not a supported
 *                      hash algorithm.
 */
#define PSA_ALG_PBKDF2_HMAC(hash_alg)                                  \
    (PSA_ALG_PBKDF2_HMAC_BASE | ((hash_alg) & PSA_ALG_HASH_MASK))

/** Whether the specified algorithm is a PBKDF2-HMAC algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is a PBKDF2-HMAC algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key derivation algorithm identifier.
 */
#define PSA_ALG_IS_PBKDF2_HMAC(alg)                                    \
    (((alg) & ~PSA_ALG_HASH_MASK) == PSA_ALG_PBKDF2_HMAC_BASE)
#define PSA_ALG_PBKDF2_HMAC_GET_HASH(pbkdf2_alg)                         \
    (PSA_ALG_CATEGORY_HASH | ((pbkdf2_alg) & PSA_ALG_HASH_MASK))
/** The PBKDF2-AES-CMAC-PRF-128 password hashing / key stretching algorithm.
 *
 * PBKDF2 is defined by PKCS#5, republished as RFC 8018 (section 5.2).
 * This macro specifies the PBKDF2 algorithm constructed using the
 * AES-CMAC-PRF-128 PRF specified by RFC 4615.
 *
 * This key derivation algorithm uses the same inputs as
 * #PSA_ALG_PBKDF2_HMAC() with the same constraints.
 */
#define PSA_ALG_PBKDF2_AES_CMAC_PRF_128         ((psa_algorithm_t) 0x08800200)

#define PSA_ALG_IS_PBKDF2(kdf_alg)                                      \
    (PSA_ALG_IS_PBKDF2_HMAC(kdf_alg) || \
     ((kdf_alg) == PSA_ALG_PBKDF2_AES_CMAC_PRF_128))

#define PSA_ALG_KEY_DERIVATION_MASK             ((psa_algorithm_t) 0xfe00ffff)
#define PSA_ALG_KEY_AGREEMENT_MASK              ((psa_algorithm_t) 0xffff0000)

/** Macro to build a combined algorithm that chains a key agreement with
 * a key derivation.
 *
 * \param ka_alg        A key agreement algorithm (\c PSA_ALG_XXX value such
 *                      that #PSA_ALG_IS_KEY_AGREEMENT(\p ka_alg) is true).
 * \param kdf_alg       A key derivation algorithm (\c PSA_ALG_XXX value such
 *                      that #PSA_ALG_IS_KEY_DERIVATION(\p kdf_alg) is true).
 *
 * \return              The corresponding key agreement and derivation
 *                      algorithm.
 * \return              Unspecified if \p ka_alg is not a supported
 *                      key agreement algorithm or \p kdf_alg is not a
 *                      supported key derivation algorithm.
 */
#define PSA_ALG_KEY_AGREEMENT(ka_alg, kdf_alg)  \
    ((ka_alg) | (kdf_alg))

#define PSA_ALG_KEY_AGREEMENT_GET_KDF(alg)                              \
    (((alg) & PSA_ALG_KEY_DERIVATION_MASK) | PSA_ALG_CATEGORY_KEY_DERIVATION)

#define PSA_ALG_KEY_AGREEMENT_GET_BASE(alg)                             \
    (((alg) & PSA_ALG_KEY_AGREEMENT_MASK) | PSA_ALG_CATEGORY_KEY_AGREEMENT)

/** Whether the specified algorithm is a raw key agreement algorithm.
 *
 * A raw key agreement algorithm is one that does not specify
 * a key derivation function.
 * Usually, raw key agreement algorithms are constructed directly with
 * a \c PSA_ALG_xxx macro while non-raw key agreement algorithms are
 * constructed with #PSA_ALG_KEY_AGREEMENT().
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \p alg is a raw key agreement algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \p alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_RAW_KEY_AGREEMENT(alg)                               \
    (PSA_ALG_IS_KEY_AGREEMENT(alg) &&                                   \
     PSA_ALG_KEY_AGREEMENT_GET_KDF(alg) == PSA_ALG_CATEGORY_KEY_DERIVATION)

#define PSA_ALG_IS_KEY_DERIVATION_OR_AGREEMENT(alg)     \
    ((PSA_ALG_IS_KEY_DERIVATION(alg) || PSA_ALG_IS_KEY_AGREEMENT(alg)))

/** The finite-field Diffie-Hellman (DH) key agreement algorithm.
 *
 * The shared secret produced by key agreement is
 * `g^{ab}` in big-endian format.
 * It is `ceiling(m / 8)` bytes long where `m` is the size of the prime `p`
 * in bits.
 */
#define PSA_ALG_FFDH                            ((psa_algorithm_t) 0x09010000)

/** Whether the specified algorithm is a finite field Diffie-Hellman algorithm.
 *
 * This includes the raw finite field Diffie-Hellman algorithm as well as
 * finite-field Diffie-Hellman followed by any supporter key derivation
 * algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is a finite field Diffie-Hellman algorithm, 0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key agreement algorithm identifier.
 */
#define PSA_ALG_IS_FFDH(alg) \
    (PSA_ALG_KEY_AGREEMENT_GET_BASE(alg) == PSA_ALG_FFDH)

/** The elliptic curve Diffie-Hellman (ECDH) key agreement algorithm.
 *
 * The shared secret produced by key agreement is the x-coordinate of
 * the shared secret point. It is always `ceiling(m / 8)` bytes long where
 * `m` is the bit size associated with the curve, i.e. the bit size of the
 * order of the curve's coordinate field. When `m` is not a multiple of 8,
 * the byte containing the most significant bit of the shared secret
 * is padded with zero bits. The byte order is either little-endian
 * or big-endian depending on the curve type.
 *
 * - For Montgomery curves (curve types `PSA_ECC_FAMILY_CURVEXXX`),
 *   the shared secret is the x-coordinate of `d_A Q_B = d_B Q_A`
 *   in little-endian byte order.
 *   The bit size is 448 for Curve448 and 255 for Curve25519.
 * - For Weierstrass curves over prime fields (curve types
 *   `PSA_ECC_FAMILY_SECPXXX` and `PSA_ECC_FAMILY_BRAINPOOL_PXXX`),
 *   the shared secret is the x-coordinate of `d_A Q_B = d_B Q_A`
 *   in big-endian byte order.
 *   The bit size is `m = ceiling(log_2(p))` for the field `F_p`.
 * - For Weierstrass curves over binary fields (curve types
 *   `PSA_ECC_FAMILY_SECTXXX`),
 *   the shared secret is the x-coordinate of `d_A Q_B = d_B Q_A`
 *   in big-endian byte order.
 *   The bit size is `m` for the field `F_{2^m}`.
 */
#define PSA_ALG_ECDH                            ((psa_algorithm_t) 0x09020000)

/** Whether the specified algorithm is an elliptic curve Diffie-Hellman
 * algorithm.
 *
 * This includes the raw elliptic curve Diffie-Hellman algorithm as well as
 * elliptic curve Diffie-Hellman followed by any supporter key derivation
 * algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is an elliptic curve Diffie-Hellman algorithm,
 *         0 otherwise.
 *         This macro may return either 0 or 1 if \c alg is not a supported
 *         key agreement algorithm identifier.
 */
#define PSA_ALG_IS_ECDH(alg) \
    (PSA_ALG_KEY_AGREEMENT_GET_BASE(alg) == PSA_ALG_ECDH)

/** Whether the specified algorithm encoding is a wildcard.
 *
 * Wildcard values may only be used to set the usage algorithm field in
 * a policy, not to perform an operation.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return 1 if \c alg is a wildcard algorithm encoding.
 * \return 0 if \c alg is a non-wildcard algorithm encoding (suitable for
 *         an operation).
 * \return This macro may return either 0 or 1 if \c alg is not a supported
 *         algorithm identifier.
 */
#define PSA_ALG_IS_WILDCARD(alg)                            \
    (PSA_ALG_IS_HASH_AND_SIGN(alg) ?                        \
     PSA_ALG_SIGN_GET_HASH(alg) == PSA_ALG_ANY_HASH :       \
     PSA_ALG_IS_MAC(alg) ?                                  \
     (alg & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0 :   \
     PSA_ALG_IS_AEAD(alg) ?                                 \
     (alg & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0 :  \
     (alg) == PSA_ALG_ANY_HASH)

/** Get the hash used by a composite algorithm.
 *
 * \param alg An algorithm identifier (value of type #psa_algorithm_t).
 *
 * \return The underlying hash algorithm if alg is a composite algorithm that
 * uses a hash algorithm.
 *
 * \return \c 0 if alg is not a composite algorithm that uses a hash.
 */
#define PSA_ALG_GET_HASH(alg) \
    (((alg) & 0x000000ff) == 0 ? ((psa_algorithm_t) 0) : 0x02000000 | ((alg) & 0x000000ff))

/**@}*/

/** \defgroup key_lifetimes Key lifetimes
 * @{
 */

/* Note that location and persistence level values are embedded in the
 * persistent key store, as part of key metadata. As a consequence, they
 * must not be changed (unless the storage format version changes).
 */

/** The default lifetime for volatile keys.
 *
 * A volatile key only exists as long as the identifier to it is not destroyed.
 * The key material is guaranteed to be erased on a power reset.
 *
 * A key with this lifetime is typically stored in the RAM area of the
 * PSA Crypto subsystem. However this is an implementation choice.
 * If an implementation stores data about the key in a non-volatile memory,
 * it must release all the resources associated with the key and erase the
 * key material if the calling application terminates.
 */
#define PSA_KEY_LIFETIME_VOLATILE               ((psa_key_lifetime_t) 0x00000000)

/** The default lifetime for persistent keys.
 *
 * A persistent key remains in storage until it is explicitly destroyed or
 * until the corresponding storage area is wiped. This specification does
 * not define any mechanism to wipe a storage area, but integrations may
 * provide their own mechanism (for example to perform a factory reset,
 * to prepare for device refurbishment, or to uninstall an application).
 *
 * This lifetime value is the default storage area for the calling
 * application. Integrations of Mbed TLS may support other persistent lifetimes.
 * See ::psa_key_lifetime_t for more information.
 */
#define PSA_KEY_LIFETIME_PERSISTENT             ((psa_key_lifetime_t) 0x00000001)

/** The persistence level of volatile keys.
 *
 * See ::psa_key_persistence_t for more information.
 */
#define PSA_KEY_PERSISTENCE_VOLATILE            ((psa_key_persistence_t) 0x00)

/** The default persistence level for persistent keys.
 *
 * See ::psa_key_persistence_t for more information.
 */
#define PSA_KEY_PERSISTENCE_DEFAULT             ((psa_key_persistence_t) 0x01)

/** A persistence level indicating that a key is never destroyed.
 *
 * See ::psa_key_persistence_t for more information.
 */
#define PSA_KEY_PERSISTENCE_READ_ONLY           ((psa_key_persistence_t) 0xff)

#define PSA_KEY_LIFETIME_GET_PERSISTENCE(lifetime)      \
    ((psa_key_persistence_t) ((lifetime) & 0x000000ff))

#define PSA_KEY_LIFETIME_GET_LOCATION(lifetime)      \
    ((psa_key_location_t) ((lifetime) >> 8))

/** Whether a key lifetime indicates that the key is volatile.
 *
 * A volatile key is automatically destroyed by the implementation when
 * the application instance terminates. In particular, a volatile key
 * is automatically destroyed on a power reset of the device.
 *
 * A key that is not volatile is persistent. Persistent keys are
 * preserved until the application explicitly destroys them or until an
 * implementation-specific device management event occurs (for example,
 * a factory reset).
 *
 * \param lifetime      The lifetime value to query (value of type
 *                      ::psa_key_lifetime_t).
 *
 * \return \c 1 if the key is volatile, otherwise \c 0.
 */
#define PSA_KEY_LIFETIME_IS_VOLATILE(lifetime)  \
    (PSA_KEY_LIFETIME_GET_PERSISTENCE(lifetime) == \
     PSA_KEY_PERSISTENCE_VOLATILE)

/** Whether a key lifetime indicates that the key is read-only.
 *
 * Read-only keys cannot be created or destroyed through the PSA Crypto API.
 * They must be created through platform-specific means that bypass the API.
 *
 * Some platforms may offer ways to destroy read-only keys. For example,
 * consider a platform with multiple levels of privilege, where a
 * low-privilege application can use a key but is not allowed to destroy
 * it, and the platform exposes the key to the application with a read-only
 * lifetime. High-privilege code can destroy the key even though the
 * application sees the key as read-only.
 *
 * \param lifetime      The lifetime value to query (value of type
 *                      ::psa_key_lifetime_t).
 *
 * \return \c 1 if the key is read-only, otherwise \c 0.
 */
#define PSA_KEY_LIFETIME_IS_READ_ONLY(lifetime)  \
    (PSA_KEY_LIFETIME_GET_PERSISTENCE(lifetime) == \
     PSA_KEY_PERSISTENCE_READ_ONLY)

/** Construct a lifetime from a persistence level and a location.
 *
 * \param persistence   The persistence level
 *                      (value of type ::psa_key_persistence_t).
 * \param location      The location indicator
 *                      (value of type ::psa_key_location_t).
 *
 * \return The constructed lifetime value.
 */
#define PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(persistence, location) \
    ((location) << 8 | (persistence))

/** The local storage area for persistent keys.
 *
 * This storage area is available on all systems that can store persistent
 * keys without delegating the storage to a third-party cryptoprocessor.
 *
 * See ::psa_key_location_t for more information.
 */
#define PSA_KEY_LOCATION_LOCAL_STORAGE          ((psa_key_location_t) 0x000000)

#define PSA_KEY_LOCATION_VENDOR_FLAG            ((psa_key_location_t) 0x800000)

/* Note that key identifier values are embedded in the
 * persistent key store, as part of key metadata. As a consequence, they
 * must not be changed (unless the storage format version changes).
 */

/** The null key identifier.
 */
/* *INDENT-OFF* (https://github.com/ARM-software/psa-arch-tests/issues/337) */
#define PSA_KEY_ID_NULL                         ((psa_key_id_t)0)
/* *INDENT-ON* */
/** The minimum value for a key identifier chosen by the application.
 */
#define PSA_KEY_ID_USER_MIN                     ((psa_key_id_t) 0x00000001)
/** The maximum value for a key identifier chosen by the application.
 */
#define PSA_KEY_ID_USER_MAX                     ((psa_key_id_t) 0x3fffffff)
/** The minimum value for a key identifier chosen by the implementation.
 */
#define PSA_KEY_ID_VENDOR_MIN                   ((psa_key_id_t) 0x40000000)
/** The maximum value for a key identifier chosen by the implementation.
 */
#define PSA_KEY_ID_VENDOR_MAX                   ((psa_key_id_t) 0x7fffffff)


#if !defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)

#define MBEDTLS_SVC_KEY_ID_INIT ((psa_key_id_t) 0)
#define MBEDTLS_SVC_KEY_ID_GET_KEY_ID(id) (id)
#define MBEDTLS_SVC_KEY_ID_GET_OWNER_ID(id) (0)

/** Utility to initialize a key identifier at runtime.
 *
 * \param unused  Unused parameter.
 * \param key_id  Identifier of the key.
 */
static inline mbedtls_svc_key_id_t mbedtls_svc_key_id_make(
    unsigned int unused, psa_key_id_t key_id)
{
    (void) unused;

    return key_id;
}

/** Compare two key identifiers.
 *
 * \param id1 First key identifier.
 * \param id2 Second key identifier.
 *
 * \return Non-zero if the two key identifier are equal, zero otherwise.
 */
static inline int mbedtls_svc_key_id_equal(mbedtls_svc_key_id_t id1,
                                           mbedtls_svc_key_id_t id2)
{
    return id1 == id2;
}

/** Check whether a key identifier is null.
 *
 * \param key Key identifier.
 *
 * \return Non-zero if the key identifier is null, zero otherwise.
 */
static inline int mbedtls_svc_key_id_is_null(mbedtls_svc_key_id_t key)
{
    return key == 0;
}

#else /* MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */

#define MBEDTLS_SVC_KEY_ID_INIT ((mbedtls_svc_key_id_t){ 0, 0 })
#define MBEDTLS_SVC_KEY_ID_GET_KEY_ID(id) ((id).MBEDTLS_PRIVATE(key_id))
#define MBEDTLS_SVC_KEY_ID_GET_OWNER_ID(id) ((id).MBEDTLS_PRIVATE(owner))

/** Utility to initialize a key identifier at runtime.
 *
 * \param owner_id Identifier of the key owner.
 * \param key_id   Identifier of the key.
 */
static inline mbedtls_svc_key_id_t mbedtls_svc_key_id_make(
    mbedtls_key_owner_id_t owner_id, psa_key_id_t key_id)
{
    return (mbedtls_svc_key_id_t){ .MBEDTLS_PRIVATE(key_id) = key_id,
                                   .MBEDTLS_PRIVATE(owner) = owner_id };
}

/** Compare two key identifiers.
 *
 * \param id1 First key identifier.
 * \param id2 Second key identifier.
 *
 * \return Non-zero if the two key identifier are equal, zero otherwise.
 */
static inline int mbedtls_svc_key_id_equal(mbedtls_svc_key_id_t id1,
                                           mbedtls_svc_key_id_t id2)
{
    return (id1.MBEDTLS_PRIVATE(key_id) == id2.MBEDTLS_PRIVATE(key_id)) &&
           mbedtls_key_owner_id_equal(id1.MBEDTLS_PRIVATE(owner), id2.MBEDTLS_PRIVATE(owner));
}

/** Check whether a key identifier is null.
 *
 * \param key Key identifier.
 *
 * \return Non-zero if the key identifier is null, zero otherwise.
 */
static inline int mbedtls_svc_key_id_is_null(mbedtls_svc_key_id_t key)
{
    return key.MBEDTLS_PRIVATE(key_id) == 0;
}

#endif /* !MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER */

/**@}*/

/** \defgroup policy Key policies
 * @{
 */

/* Note that key usage flags are embedded in the
 * persistent key store, as part of key metadata. As a consequence, they
 * must not be changed (unless the storage format version changes).
 */

/** Whether the key may be exported.
 *
 * A public key or the public part of a key pair may always be exported
 * regardless of the value of this permission flag.
 *
 * If a key does not have export permission, implementations shall not
 * allow the key to be exported in plain form from the cryptoprocessor,
 * whether through psa_export_key() or through a proprietary interface.
 * The key may however be exportable in a wrapped form, i.e. in a form
 * where it is encrypted by another key.
 */
#define PSA_KEY_USAGE_EXPORT                    ((psa_key_usage_t) 0x00000001)

/** Whether the key may be copied.
 *
 * This flag allows the use of psa_copy_key() to make a copy of the key
 * with the same policy or a more restrictive policy.
 *
 * For lifetimes for which the key is located in a secure element which
 * enforce the non-exportability of keys, copying a key outside the secure
 * element also requires the usage flag #PSA_KEY_USAGE_EXPORT.
 * Copying the key inside the secure element is permitted with just
 * #PSA_KEY_USAGE_COPY if the secure element supports it.
 * For keys with the lifetime #PSA_KEY_LIFETIME_VOLATILE or
 * #PSA_KEY_LIFETIME_PERSISTENT, the usage flag #PSA_KEY_USAGE_COPY
 * is sufficient to permit the copy.
 */
#define PSA_KEY_USAGE_COPY                      ((psa_key_usage_t) 0x00000002)

/** Whether the key may be used to encrypt a message.
 *
 * This flag allows the key to be used for a symmetric encryption operation,
 * for an AEAD encryption-and-authentication operation,
 * or for an asymmetric encryption operation,
 * if otherwise permitted by the key's type and policy.
 *
 * For a key pair, this concerns the public key.
 */
#define PSA_KEY_USAGE_ENCRYPT                   ((psa_key_usage_t) 0x00000100)

/** Whether the key may be used to decrypt a message.
 *
 * This flag allows the key to be used for a symmetric decryption operation,
 * for an AEAD decryption-and-verification operation,
 * or for an asymmetric decryption operation,
 * if otherwise permitted by the key's type and policy.
 *
 * For a key pair, this concerns the private key.
 */
#define PSA_KEY_USAGE_DECRYPT                   ((psa_key_usage_t) 0x00000200)

/** Whether the key may be used to sign a message.
 *
 * This flag allows the key to be used for a MAC calculation operation or for
 * an asymmetric message signature operation, if otherwise permitted by the
 * key’s type and policy.
 *
 * For a key pair, this concerns the private key.
 */
#define PSA_KEY_USAGE_SIGN_MESSAGE              ((psa_key_usage_t) 0x00000400)

/** Whether the key may be used to verify a message.
 *
 * This flag allows the key to be used for a MAC verification operation or for
 * an asymmetric message signature verification operation, if otherwise
 * permitted by the key’s type and policy.
 *
 * For a key pair, this concerns the public key.
 */
#define PSA_KEY_USAGE_VERIFY_MESSAGE            ((psa_key_usage_t) 0x00000800)

/** Whether the key may be used to sign a message.
 *
 * This flag allows the key to be used for a MAC calculation operation
 * or for an asymmetric signature operation,
 * if otherwise permitted by the key's type and policy.
 *
 * For a key pair, this concerns the private key.
 */
#define PSA_KEY_USAGE_SIGN_HASH                 ((psa_key_usage_t) 0x00001000)

/** Whether the key may be used to verify a message signature.
 *
 * This flag allows the key to be used for a MAC verification operation
 * or for an asymmetric signature verification operation,
 * if otherwise permitted by the key's type and policy.
 *
 * For a key pair, this concerns the public key.
 */
#define PSA_KEY_USAGE_VERIFY_HASH               ((psa_key_usage_t) 0x00002000)

/** Whether the key may be used to derive other keys or produce a password
 * hash.
 *
 * This flag allows the key to be used for a key derivation operation or for
 * a key agreement operation, if otherwise permitted by the key's type and
 * policy.
 *
 * If this flag is present on all keys used in calls to
 * psa_key_derivation_input_key() for a key derivation operation, then it
 * permits calling psa_key_derivation_output_bytes() or
 * psa_key_derivation_output_key() at the end of the operation.
 */
#define PSA_KEY_USAGE_DERIVE                    ((psa_key_usage_t) 0x00004000)

/** Whether the key may be used to verify the result of a key derivation,
 * including password hashing.
 *
 * This flag allows the key to be used:
 *
 * This flag allows the key to be used in a key derivation operation, if
 * otherwise permitted by the key's type and policy.
 *
 * If this flag is present on all keys used in calls to
 * psa_key_derivation_input_key() for a key derivation operation, then it
 * permits calling psa_key_derivation_verify_bytes() or
 * psa_key_derivation_verify_key() at the end of the operation.
 */
#define PSA_KEY_USAGE_VERIFY_DERIVATION         ((psa_key_usage_t) 0x00008000)

/**@}*/

/** \defgroup derivation Key derivation
 * @{
 */

/* Key input steps are not embedded in the persistent storage, so you can
 * change them if needed: it's only an ABI change. */

/** A secret input for key derivation.
 *
 * This should be a key of type #PSA_KEY_TYPE_DERIVE
 * (passed to psa_key_derivation_input_key())
 * or the shared secret resulting from a key agreement
 * (obtained via psa_key_derivation_key_agreement()).
 *
 * The secret can also be a direct input (passed to
 * key_derivation_input_bytes()). In this case, the derivation operation
 * may not be used to derive keys: the operation will only allow
 * psa_key_derivation_output_bytes(),
 * psa_key_derivation_verify_bytes(), or
 * psa_key_derivation_verify_key(), but not
 * psa_key_derivation_output_key().
 */
#define PSA_KEY_DERIVATION_INPUT_SECRET     ((psa_key_derivation_step_t) 0x0101)

/** A low-entropy secret input for password hashing / key stretching.
 *
 * This is usually a key of type #PSA_KEY_TYPE_PASSWORD (passed to
 * psa_key_derivation_input_key()) or a direct input (passed to
 * psa_key_derivation_input_bytes()) that is a password or passphrase. It can
 * also be high-entropy secret such as a key of type #PSA_KEY_TYPE_DERIVE or
 * the shared secret resulting from a key agreement.
 *
 * The secret can also be a direct input (passed to
 * key_derivation_input_bytes()). In this case, the derivation operation
 * may not be used to derive keys: the operation will only allow
 * psa_key_derivation_output_bytes(),
 * psa_key_derivation_verify_bytes(), or
 * psa_key_derivation_verify_key(), but not
 * psa_key_derivation_output_key().
 */
#define PSA_KEY_DERIVATION_INPUT_PASSWORD   ((psa_key_derivation_step_t) 0x0102)

/** A high-entropy additional secret input for key derivation.
 *
 * This is typically the shared secret resulting from a key agreement obtained
 * via `psa_key_derivation_key_agreement()`. It may alternatively be a key of
 * type `PSA_KEY_TYPE_DERIVE` passed to `psa_key_derivation_input_key()`, or
 * a direct input passed to `psa_key_derivation_input_bytes()`.
 */
#define PSA_KEY_DERIVATION_INPUT_OTHER_SECRET \
    ((psa_key_derivation_step_t) 0x0103)

/** A label for key derivation.
 *
 * This should be a direct input.
 * It can also be a key of type #PSA_KEY_TYPE_RAW_DATA.
 */
#define PSA_KEY_DERIVATION_INPUT_LABEL      ((psa_key_derivation_step_t) 0x0201)

/** A salt for key derivation.
 *
 * This should be a direct input.
 * It can also be a key of type #PSA_KEY_TYPE_RAW_DATA or
 * #PSA_KEY_TYPE_PEPPER.
 */
#define PSA_KEY_DERIVATION_INPUT_SALT       ((psa_key_derivation_step_t) 0x0202)

/** An information string for key derivation.
 *
 * This should be a direct input.
 * It can also be a key of type #PSA_KEY_TYPE_RAW_DATA.
 */
#define PSA_KEY_DERIVATION_INPUT_INFO       ((psa_key_derivation_step_t) 0x0203)

/** A seed for key derivation.
 *
 * This should be a direct input.
 * It can also be a key of type #PSA_KEY_TYPE_RAW_DATA.
 */
#define PSA_KEY_DERIVATION_INPUT_SEED       ((psa_key_derivation_step_t) 0x0204)

/** A cost parameter for password hashing / key stretching.
 *
 * This must be a direct input, passed to psa_key_derivation_input_integer().
 */
#define PSA_KEY_DERIVATION_INPUT_COST       ((psa_key_derivation_step_t) 0x0205)

/**@}*/

/** \defgroup helper_macros Helper macros
 * @{
 */

/* Helper macros */

/** Check if two AEAD algorithm identifiers refer to the same AEAD algorithm
 *  regardless of the tag length they encode.
 *
 * \param aead_alg_1 An AEAD algorithm identifier.
 * \param aead_alg_2 An AEAD algorithm identifier.
 *
 * \return           1 if both identifiers refer to the same AEAD algorithm,
 *                   0 otherwise.
 *                   Unspecified if neither \p aead_alg_1 nor \p aead_alg_2 are
 *                   a supported AEAD algorithm.
 */
#define MBEDTLS_PSA_ALG_AEAD_EQUAL(aead_alg_1, aead_alg_2) \
    (!(((aead_alg_1) ^ (aead_alg_2)) & \
       ~(PSA_ALG_AEAD_TAG_LENGTH_MASK | PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG)))

/**@}*/

/**@}*/

/** \defgroup interruptible Interruptible operations
 * @{
 */

/** Maximum value for use with \c psa_interruptible_set_max_ops() to determine
 *  the maximum number of ops allowed to be executed by an interruptible
 *  function in a single call.
 */
#define PSA_INTERRUPTIBLE_MAX_OPS_UNLIMITED UINT32_MAX

/**@}*/

#endif /* PSA_CRYPTO_VALUES_H */
