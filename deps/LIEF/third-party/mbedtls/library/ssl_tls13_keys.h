/*
 *  TLS 1.3 key schedule
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */
#if !defined(MBEDTLS_SSL_TLS1_3_KEYS_H)
#define MBEDTLS_SSL_TLS1_3_KEYS_H

/* This requires MBEDTLS_SSL_TLS1_3_LABEL( idx, name, string ) to be defined at
 * the point of use. See e.g. the definition of mbedtls_ssl_tls13_labels_union
 * below. */
#define MBEDTLS_SSL_TLS1_3_LABEL_LIST                                             \
    MBEDTLS_SSL_TLS1_3_LABEL(finished, "finished") \
    MBEDTLS_SSL_TLS1_3_LABEL(resumption, "resumption") \
    MBEDTLS_SSL_TLS1_3_LABEL(traffic_upd, "traffic upd") \
    MBEDTLS_SSL_TLS1_3_LABEL(exporter, "exporter") \
    MBEDTLS_SSL_TLS1_3_LABEL(key, "key") \
    MBEDTLS_SSL_TLS1_3_LABEL(iv, "iv") \
    MBEDTLS_SSL_TLS1_3_LABEL(c_hs_traffic, "c hs traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(c_ap_traffic, "c ap traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(c_e_traffic, "c e traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(s_hs_traffic, "s hs traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(s_ap_traffic, "s ap traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(s_e_traffic, "s e traffic") \
    MBEDTLS_SSL_TLS1_3_LABEL(e_exp_master, "e exp master") \
    MBEDTLS_SSL_TLS1_3_LABEL(res_master, "res master") \
    MBEDTLS_SSL_TLS1_3_LABEL(exp_master, "exp master") \
    MBEDTLS_SSL_TLS1_3_LABEL(ext_binder, "ext binder") \
    MBEDTLS_SSL_TLS1_3_LABEL(res_binder, "res binder") \
    MBEDTLS_SSL_TLS1_3_LABEL(derived, "derived") \
    MBEDTLS_SSL_TLS1_3_LABEL(client_cv, "TLS 1.3, client CertificateVerify") \
    MBEDTLS_SSL_TLS1_3_LABEL(server_cv, "TLS 1.3, server CertificateVerify")

#define MBEDTLS_SSL_TLS1_3_CONTEXT_UNHASHED 0
#define MBEDTLS_SSL_TLS1_3_CONTEXT_HASHED   1

#define MBEDTLS_SSL_TLS1_3_PSK_EXTERNAL   0
#define MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION 1

#if defined(MBEDTLS_SSL_PROTO_TLS1_3)

/* We need to tell the compiler that we meant to leave out the null character. */
#define MBEDTLS_SSL_TLS1_3_LABEL(name, string)       \
    const unsigned char name    [sizeof(string) - 1] MBEDTLS_ATTRIBUTE_UNTERMINATED_STRING;

union mbedtls_ssl_tls13_labels_union {
    MBEDTLS_SSL_TLS1_3_LABEL_LIST
};
struct mbedtls_ssl_tls13_labels_struct {
    MBEDTLS_SSL_TLS1_3_LABEL_LIST
};
#undef MBEDTLS_SSL_TLS1_3_LABEL

extern const struct mbedtls_ssl_tls13_labels_struct mbedtls_ssl_tls13_labels;

#define MBEDTLS_SSL_TLS1_3_LBL_LEN(LABEL)  \
    sizeof(mbedtls_ssl_tls13_labels.LABEL)

#define MBEDTLS_SSL_TLS1_3_LBL_WITH_LEN(LABEL)  \
    mbedtls_ssl_tls13_labels.LABEL,              \
    MBEDTLS_SSL_TLS1_3_LBL_LEN(LABEL)

/* Maximum length of the label field in the HkdfLabel struct defined in
 * RFC 8446, Section 7.1, excluding the "tls13 " prefix. */
#define MBEDTLS_SSL_TLS1_3_HKDF_LABEL_MAX_LABEL_LEN 249

/* The maximum length of HKDF contexts used in the TLS 1.3 standard.
 * Since contexts are always hashes of message transcripts, this can
 * be approximated from above by the maximum hash size. */
#define MBEDTLS_SSL_TLS1_3_KEY_SCHEDULE_MAX_CONTEXT_LEN  \
    PSA_HASH_MAX_SIZE

/* Maximum desired length for expanded key material generated
 * by HKDF-Expand-Label. This algorithm can output up to 255 * hash_size
 * bytes of key material where hash_size is the output size of the
 * underlying hash function. */
#define MBEDTLS_SSL_TLS1_3_KEY_SCHEDULE_MAX_EXPANSION_LEN \
    (255 * MBEDTLS_TLS1_3_MD_MAX_SIZE)

/**
 * \brief            The \c HKDF-Expand-Label function from
 *                   the TLS 1.3 standard RFC 8446.
 *
 * <tt>
 *                   HKDF-Expand-Label( Secret, Label, Context, Length ) =
 *                       HKDF-Expand( Secret, HkdfLabel, Length )
 * </tt>
 *
 * \param hash_alg   The identifier for the hash algorithm to use.
 * \param secret     The \c Secret argument to \c HKDF-Expand-Label.
 *                   This must be a readable buffer of length
 *                   \p secret_len Bytes.
 * \param secret_len The length of \p secret in Bytes.
 * \param label      The \c Label argument to \c HKDF-Expand-Label.
 *                   This must be a readable buffer of length
 *                   \p label_len Bytes.
 * \param label_len  The length of \p label in Bytes.
 * \param ctx        The \c Context argument to \c HKDF-Expand-Label.
 *                   This must be a readable buffer of length \p ctx_len Bytes.
 * \param ctx_len    The length of \p context in Bytes.
 * \param buf        The destination buffer to hold the expanded secret.
 *                   This must be a writable buffer of length \p buf_len Bytes.
 * \param buf_len    The desired size of the expanded secret in Bytes.
 *
 * \returns          \c 0 on success.
 * \return           A negative error code on failure.
 */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_hkdf_expand_label(
    psa_algorithm_t hash_alg,
    const unsigned char *secret, size_t secret_len,
    const unsigned char *label, size_t label_len,
    const unsigned char *ctx, size_t ctx_len,
    unsigned char *buf, size_t buf_len);

/**
 * \brief           This function is part of the TLS 1.3 key schedule.
 *                  It extracts key and IV for the actual client/server traffic
 *                  from the client/server traffic secrets.
 *
 * From RFC 8446:
 *
 * <tt>
 *   [sender]_write_key = HKDF-Expand-Label(Secret, "key", "", key_length)
 *   [sender]_write_iv  = HKDF-Expand-Label(Secret, "iv", "", iv_length)*
 * </tt>
 *
 * \param hash_alg      The identifier for the hash algorithm to be used
 *                      for the HKDF-based expansion of the secret.
 * \param client_secret The client traffic secret.
 *                      This must be a readable buffer of size
 *                      \p secret_len Bytes
 * \param server_secret The server traffic secret.
 *                      This must be a readable buffer of size
 *                      \p secret_len Bytes
 * \param secret_len    Length of the secrets \p client_secret and
 *                      \p server_secret in Bytes.
 * \param key_len       The desired length of the key to be extracted in Bytes.
 * \param iv_len        The desired length of the IV to be extracted in Bytes.
 * \param keys          The address of the structure holding the generated
 *                      keys and IVs.
 *
 * \returns             \c 0 on success.
 * \returns             A negative error code on failure.
 */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_make_traffic_keys(
    psa_algorithm_t hash_alg,
    const unsigned char *client_secret,
    const unsigned char *server_secret, size_t secret_len,
    size_t key_len, size_t iv_len,
    mbedtls_ssl_key_set *keys);

/**
 * \brief The \c Derive-Secret function from the TLS 1.3 standard RFC 8446.
 *
 * <tt>
 *   Derive-Secret( Secret, Label, Messages ) =
 *      HKDF-Expand-Label( Secret, Label,
 *                         Hash( Messages ),
 *                         Hash.Length ) )
 * </tt>
 *
 * \param hash_alg   The identifier for the hash function used for the
 *                   applications of HKDF.
 * \param secret     The \c Secret argument to the \c Derive-Secret function.
 *                   This must be a readable buffer of length
 *                   \p secret_len Bytes.
 * \param secret_len The length of \p secret in Bytes.
 * \param label      The \c Label argument to the \c Derive-Secret function.
 *                   This must be a readable buffer of length
 *                   \p label_len Bytes.
 * \param label_len  The length of \p label in Bytes.
 * \param ctx        The hash of the \c Messages argument to the
 *                   \c Derive-Secret function, or the \c Messages argument
 *                   itself, depending on \p ctx_hashed.
 * \param ctx_len    The length of \p ctx in Bytes.
 * \param ctx_hashed This indicates whether the \p ctx contains the hash of
 *                   the \c Messages argument in the application of the
 *                   \c Derive-Secret function
 *                   (value MBEDTLS_SSL_TLS1_3_CONTEXT_HASHED), or whether
 *                   it is the content of \c Messages itself, in which case
 *                   the function takes care of the hashing
 *                   (value MBEDTLS_SSL_TLS1_3_CONTEXT_UNHASHED).
 * \param dstbuf     The target buffer to write the output of
 *                   \c Derive-Secret to. This must be a writable buffer of
 *                   size \p dtsbuf_len Bytes.
 * \param dstbuf_len The length of \p dstbuf in Bytes.
 *
 * \returns        \c 0 on success.
 * \returns        A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_derive_secret(
    psa_algorithm_t hash_alg,
    const unsigned char *secret, size_t secret_len,
    const unsigned char *label, size_t label_len,
    const unsigned char *ctx, size_t ctx_len,
    int ctx_hashed,
    unsigned char *dstbuf, size_t dstbuf_len);

/**
 * \brief Derive TLS 1.3 early data key material from early secret.
 *
 *        This is a small wrapper invoking mbedtls_ssl_tls13_derive_secret()
 *        with the appropriate labels.
 *
 * <tt>
 *        Early Secret
 *             |
 *             +-----> Derive-Secret(., "c e traffic", ClientHello)
 *             |                      = client_early_traffic_secret
 *             |
 *             +-----> Derive-Secret(., "e exp master", ClientHello)
 *             .                      = early_exporter_master_secret
 *             .
 *             .
 * </tt>
 *
 * \note  To obtain the actual key and IV for the early data traffic,
 *        the client secret derived by this function need to be
 *        further processed by mbedtls_ssl_tls13_make_traffic_keys().
 *
 * \note  The binder key, which is also generated from the early secret,
 *        is omitted here. Its calculation is part of the separate routine
 *        mbedtls_ssl_tls13_create_psk_binder().
 *
 * \param hash_alg     The hash algorithm associated with the PSK for which
 *                     early data key material is being derived.
 * \param early_secret The early secret from which the early data key material
 *                     should be derived. This must be a readable buffer whose
 *                     length is the digest size of the hash algorithm
 *                     represented by \p md_size.
 * \param transcript   The transcript of the handshake so far, calculated with
 *                     respect to \p hash_alg. This must be a readable buffer
 *                     whose length is the digest size of the hash algorithm
 *                     represented by \p md_size.
 * \param derived      The address of the structure in which to store
 *                     the early data key material.
 *
 * \returns        \c 0 on success.
 * \returns        A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_derive_early_secrets(
    psa_algorithm_t hash_alg,
    unsigned char const *early_secret,
    unsigned char const *transcript, size_t transcript_len,
    mbedtls_ssl_tls13_early_secrets *derived);

/**
 * \brief Derive TLS 1.3 handshake key material from the handshake secret.
 *
 *        This is a small wrapper invoking mbedtls_ssl_tls13_derive_secret()
 *        with the appropriate labels from the standard.
 *
 * <tt>
 *        Handshake Secret
 *              |
 *              +-----> Derive-Secret( ., "c hs traffic",
 *              |                      ClientHello...ServerHello )
 *              |                      = client_handshake_traffic_secret
 *              |
 *              +-----> Derive-Secret( ., "s hs traffic",
 *              .                      ClientHello...ServerHello )
 *              .                      = server_handshake_traffic_secret
 *              .
 * </tt>
 *
 * \note  To obtain the actual key and IV for the encrypted handshake traffic,
 *        the client and server secret derived by this function need to be
 *        further processed by mbedtls_ssl_tls13_make_traffic_keys().
 *
 * \param hash_alg          The hash algorithm associated with the ciphersuite
 *                          that's being used for the connection.
 * \param handshake_secret  The handshake secret from which the handshake key
 *                          material should be derived. This must be a readable
 *                          buffer whose length is the digest size of the hash
 *                          algorithm represented by \p md_size.
 * \param transcript        The transcript of the handshake so far, calculated
 *                          with respect to \p hash_alg. This must be a readable
 *                          buffer whose length is the digest size of the hash
 *                          algorithm represented by \p md_size.
 * \param derived           The address of the structure in which to
 *                          store the handshake key material.
 *
 * \returns        \c 0 on success.
 * \returns        A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_derive_handshake_secrets(
    psa_algorithm_t hash_alg,
    unsigned char const *handshake_secret,
    unsigned char const *transcript, size_t transcript_len,
    mbedtls_ssl_tls13_handshake_secrets *derived);

/**
 * \brief Derive TLS 1.3 application key material from the master secret.
 *
 *        This is a small wrapper invoking mbedtls_ssl_tls13_derive_secret()
 *        with the appropriate labels from the standard.
 *
 * <tt>
 *        Master Secret
 *              |
 *              +-----> Derive-Secret( ., "c ap traffic",
 *              |                      ClientHello...server Finished )
 *              |                      = client_application_traffic_secret_0
 *              |
 *              +-----> Derive-Secret( ., "s ap traffic",
 *              |                      ClientHello...Server Finished )
 *              |                      = server_application_traffic_secret_0
 *              |
 *              +-----> Derive-Secret( ., "exp master",
 *              .                      ClientHello...server Finished)
 *              .                      = exporter_master_secret
 *              .
 * </tt>
 *
 * \note  To obtain the actual key and IV for the (0-th) application traffic,
 *        the client and server secret derived by this function need to be
 *        further processed by mbedtls_ssl_tls13_make_traffic_keys().
 *
 * \param hash_alg          The hash algorithm associated with the ciphersuite
 *                          that's being used for the connection.
 * \param master_secret     The master secret from which the application key
 *                          material should be derived. This must be a readable
 *                          buffer whose length is the digest size of the hash
 *                          algorithm represented by \p md_size.
 * \param transcript        The transcript of the handshake up to and including
 *                          the ServerFinished message, calculated with respect
 *                          to \p hash_alg. This must be a readable buffer whose
 *                          length is the digest size of the hash algorithm
 *                          represented by \p hash_alg.
 * \param derived           The address of the structure in which to
 *                          store the application key material.
 *
 * \returns        \c 0 on success.
 * \returns        A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_derive_application_secrets(
    psa_algorithm_t hash_alg,
    unsigned char const *master_secret,
    unsigned char const *transcript, size_t transcript_len,
    mbedtls_ssl_tls13_application_secrets *derived);

/**
 * \brief Derive TLS 1.3 resumption master secret from the master secret.
 *
 *        This is a small wrapper invoking mbedtls_ssl_tls13_derive_secret()
 *        with the appropriate labels from the standard.
 *
 * \param hash_alg          The hash algorithm used in the application for which
 *                          key material is being derived.
 * \param application_secret The application secret from which the resumption master
 *                          secret should be derived. This must be a readable
 *                          buffer whose length is the digest size of the hash
 *                          algorithm represented by \p md_size.
 * \param transcript        The transcript of the handshake up to and including
 *                          the ClientFinished message, calculated with respect
 *                          to \p hash_alg. This must be a readable buffer whose
 *                          length is the digest size of the hash algorithm
 *                          represented by \p hash_alg.
 * \param transcript_len    The length of \p transcript in Bytes.
 * \param derived           The address of the structure in which to
 *                          store the resumption master secret.
 *
 * \returns        \c 0 on success.
 * \returns        A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_derive_resumption_master_secret(
    psa_algorithm_t hash_alg,
    unsigned char const *application_secret,
    unsigned char const *transcript, size_t transcript_len,
    mbedtls_ssl_tls13_application_secrets *derived);

/**
 * \brief Compute the next secret in the TLS 1.3 key schedule
 *
 * The TLS 1.3 key schedule proceeds as follows to compute
 * the three main secrets during the handshake: The early
 * secret for early data, the handshake secret for all
 * other encrypted handshake messages, and the master
 * secret for all application traffic.
 *
 * <tt>
 *                    0
 *                    |
 *                    v
 *     PSK ->  HKDF-Extract = Early Secret
 *                    |
 *                    v
 *     Derive-Secret( ., "derived", "" )
 *                    |
 *                    v
 *  (EC)DHE -> HKDF-Extract = Handshake Secret
 *                    |
 *                    v
 *     Derive-Secret( ., "derived", "" )
 *                    |
 *                    v
 *     0 -> HKDF-Extract = Master Secret
 * </tt>
 *
 * Each of the three secrets in turn is the basis for further
 * key derivations, such as the derivation of traffic keys and IVs;
 * see e.g. mbedtls_ssl_tls13_make_traffic_keys().
 *
 * This function implements one step in this evolution of secrets:
 *
 * <tt>
 *                old_secret
 *                    |
 *                    v
 *     Derive-Secret( ., "derived", "" )
 *                    |
 *                    v
 *     input -> HKDF-Extract = new_secret
 * </tt>
 *
 * \param hash_alg    The identifier for the hash function used for the
 *                    applications of HKDF.
 * \param secret_old  The address of the buffer holding the old secret
 *                    on function entry. If not \c NULL, this must be a
 *                    readable buffer whose size matches the output size
 *                    of the hash function represented by \p hash_alg.
 *                    If \c NULL, an all \c 0 array will be used instead.
 * \param input       The address of the buffer holding the additional
 *                    input for the key derivation (e.g., the PSK or the
 *                    ephemeral (EC)DH secret). If not \c NULL, this must be
 *                    a readable buffer whose size \p input_len Bytes.
 *                    If \c NULL, an all \c 0 array will be used instead.
 * \param input_len   The length of \p input in Bytes.
 * \param secret_new  The address of the buffer holding the new secret
 *                    on function exit. This must be a writable buffer
 *                    whose size matches the output size of the hash
 *                    function represented by \p hash_alg.
 *                    This may be the same as \p secret_old.
 *
 * \returns           \c 0 on success.
 * \returns           A negative error code on failure.
 */

MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_evolve_secret(
    psa_algorithm_t hash_alg,
    const unsigned char *secret_old,
    const unsigned char *input, size_t input_len,
    unsigned char *secret_new);

/**
 * \brief             Calculate a TLS 1.3 PSK binder.
 *
 * \param ssl         The SSL context. This is used for debugging only and may
 *                    be \c NULL if MBEDTLS_DEBUG_C is disabled.
 * \param hash_alg    The hash algorithm associated to the PSK \p psk.
 * \param psk         The buffer holding the PSK for which to create a binder.
 * \param psk_len     The size of \p psk in bytes.
 * \param psk_type    This indicates whether the PSK \p psk is externally
 *                    provisioned (#MBEDTLS_SSL_TLS1_3_PSK_EXTERNAL) or a
 *                    resumption PSK (#MBEDTLS_SSL_TLS1_3_PSK_RESUMPTION).
 * \param transcript  The handshake transcript up to the point where the
 *                    PSK binder calculation happens. This must be readable,
 *                    and its size must be equal to the digest size of
 *                    the hash algorithm represented by \p hash_alg.
 * \param result      The address at which to store the PSK binder on success.
 *                    This must be writable, and its size must be equal to the
 *                    digest size of  the hash algorithm represented by
 *                    \p hash_alg.
 *
 * \returns           \c 0 on success.
 * \returns           A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_create_psk_binder(mbedtls_ssl_context *ssl,
                                        const psa_algorithm_t hash_alg,
                                        unsigned char const *psk, size_t psk_len,
                                        int psk_type,
                                        unsigned char const *transcript,
                                        unsigned char *result);

/**
 * \bref Setup an SSL transform structure representing the
 *       record protection mechanism used by TLS 1.3
 *
 * \param transform    The SSL transform structure to be created. This must have
 *                     been initialized through mbedtls_ssl_transform_init() and
 *                     not used in any other way prior to calling this function.
 *                     In particular, this function does not clean up the
 *                     transform structure prior to installing the new keys.
 * \param endpoint     Indicates whether the transform is for the client
 *                     (value #MBEDTLS_SSL_IS_CLIENT) or the server
 *                     (value #MBEDTLS_SSL_IS_SERVER).
 * \param ciphersuite  The numerical identifier for the ciphersuite to use.
 *                     This must be one of the identifiers listed in
 *                     ssl_ciphersuites.h.
 * \param traffic_keys The key material to use. No reference is stored in
 *                     the SSL transform being generated, and the caller
 *                     should destroy the key material afterwards.
 * \param ssl          (Debug-only) The SSL context to use for debug output
 *                     in case of failure. This parameter is only needed if
 *                     #MBEDTLS_DEBUG_C is set, and is ignored otherwise.
 *
 * \return             \c 0 on success. In this case, \p transform is ready to
 *                     be used with mbedtls_ssl_transform_decrypt() and
 *                     mbedtls_ssl_transform_encrypt().
 * \return             A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_populate_transform(mbedtls_ssl_transform *transform,
                                         int endpoint,
                                         int ciphersuite,
                                         mbedtls_ssl_key_set const *traffic_keys,
                                         mbedtls_ssl_context *ssl);

/*
 * TLS 1.3 key schedule evolutions
 *
 *   Early -> Handshake -> Application
 *
 * Small wrappers around mbedtls_ssl_tls13_evolve_secret().
 */

/**
 * \brief Begin TLS 1.3 key schedule by calculating early secret.
 *
 *        The TLS 1.3 key schedule can be viewed as a simple state machine
 *        with states Initial -> Early -> Handshake -> Application, and
 *        this function represents the Initial -> Early transition.
 *
 * \param ssl  The SSL context to operate on.
 *
 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_key_schedule_stage_early(mbedtls_ssl_context *ssl);

/**
 * \brief Compute TLS 1.3 resumption master secret.
 *
 * \param ssl  The SSL context to operate on. This must be in
 *             key schedule stage \c Application, see
 *             mbedtls_ssl_tls13_key_schedule_stage_application().
 *
 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_compute_resumption_master_secret(mbedtls_ssl_context *ssl);

/**
 * \brief Calculate the verify_data value for the client or server TLS 1.3
 * Finished message.
 *
 * \param ssl  The SSL context to operate on. This must be in
 *             key schedule stage \c Handshake, see
 *             mbedtls_ssl_tls13_key_schedule_stage_application().
 * \param dst        The address at which to write the verify_data value.
 * \param dst_len    The size of \p dst in bytes.
 * \param actual_len The address at which to store the amount of data
 *                   actually written to \p dst upon success.
 * \param which      The message to calculate the `verify_data` for:
 *                   - #MBEDTLS_SSL_IS_CLIENT for the Client's Finished message
 *                   - #MBEDTLS_SSL_IS_SERVER for the Server's Finished message
 *
 * \note       Both client and server call this function twice, once to
 *             generate their own Finished message, and once to verify the
 *             peer's Finished message.

 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_calculate_verify_data(mbedtls_ssl_context *ssl,
                                            unsigned char *dst,
                                            size_t dst_len,
                                            size_t *actual_len,
                                            int which);

#if defined(MBEDTLS_SSL_EARLY_DATA)
/**
 * \brief Compute TLS 1.3 early transform
 *
 * \param ssl  The SSL context to operate on.
 *
 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 *
 * \warning    The function does not compute the early master secret. Call
 *             mbedtls_ssl_tls13_key_schedule_stage_early() before to
 *             call this function to generate the early master secret.
 * \note       For a client/server endpoint, the function computes only the
 *             encryption/decryption part of the transform as the decryption/
 *             encryption part is not defined by the specification (no early
 *             traffic from the server to the client).
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_compute_early_transform(mbedtls_ssl_context *ssl);
#endif /* MBEDTLS_SSL_EARLY_DATA */

/**
 * \brief Compute TLS 1.3 handshake transform
 *
 * \param ssl  The SSL context to operate on. The early secret must have been
 *             computed.
 *
 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_compute_handshake_transform(mbedtls_ssl_context *ssl);

/**
 * \brief Compute TLS 1.3 application transform
 *
 * \param ssl  The SSL context to operate on. The early secret must have been
 *             computed.
 *
 * \returns    \c 0 on success.
 * \returns    A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_compute_application_transform(mbedtls_ssl_context *ssl);

#if defined(MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_SOME_PSK_ENABLED)
/**
 * \brief Export TLS 1.3 PSK from handshake context
 *
 * \param[in]   ssl  The SSL context to operate on.
 * \param[out]  psk  PSK output pointer.
 * \param[out]  psk_len Length of PSK.
 *
 * \returns     \c 0 if there is a configured PSK and it was exported
 *              successfully.
 * \returns     A negative error code on failure.
 */
MBEDTLS_CHECK_RETURN_CRITICAL
int mbedtls_ssl_tls13_export_handshake_psk(mbedtls_ssl_context *ssl,
                                           unsigned char **psk,
                                           size_t *psk_len);
#endif

/**
 * \brief Calculate TLS-Exporter function as defined in RFC 8446, Section 7.5.
 *
 * \param[in]   hash_alg  The hash algorithm.
 * \param[in]   secret  The secret to use. (Should be the exporter master secret.)
 * \param[in]   secret_len  Length of secret.
 * \param[in]   label  The label of the exported key.
 * \param[in]   label_len  The length of label.
 * \param[out]  out  The output buffer for the exported key. Must have room for at least out_len bytes.
 * \param[in]   out_len  Length of the key to generate.
 */
int mbedtls_ssl_tls13_exporter(const psa_algorithm_t hash_alg,
                               const unsigned char *secret, const size_t secret_len,
                               const unsigned char *label, const size_t label_len,
                               const unsigned char *context_value, const size_t context_len,
                               uint8_t *out, const size_t out_len);

#endif /* MBEDTLS_SSL_PROTO_TLS1_3 */

#endif /* MBEDTLS_SSL_TLS1_3_KEYS_H */
