#! /usr/bin/env perl
# Copyright 2023-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

package OpenSSL::paramnames;

use strict;
use warnings;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT_OK = qw(generate_public_macros
                    produce_param_decoder);

my $case_sensitive = 1;
my $need_break = 0;
my $invalid_param = "invalid param";

my %params = (
# Well known parameter names that core passes to providers
    'OSSL_PROV_PARAM_CORE_VERSION' =>         "openssl-version",# utf8_ptr
    'OSSL_PROV_PARAM_CORE_PROV_NAME' =>       "provider-name",  # utf8_ptr
    'OSSL_PROV_PARAM_CORE_MODULE_FILENAME' => "module-filename",# utf8_ptr

# Well known parameter names that Providers can define
    'OSSL_PROV_PARAM_NAME' =>               "name",               # utf8_ptr
    'OSSL_PROV_PARAM_VERSION' =>            "version",            # utf8_ptr
    'OSSL_PROV_PARAM_BUILDINFO' =>          "buildinfo",          # utf8_ptr
    'OSSL_PROV_PARAM_STATUS' =>             "status",             # uint
    'OSSL_PROV_PARAM_SECURITY_CHECKS' =>    "security-checks",    # uint
    'OSSL_PROV_PARAM_HMAC_KEY_CHECK' =>     "hmac-key-check",     # uint
    'OSSL_PROV_PARAM_KMAC_KEY_CHECK' =>     "kmac-key-check",     # uint
    'OSSL_PROV_PARAM_TLS1_PRF_EMS_CHECK' => "tls1-prf-ems-check", # uint
    'OSSL_PROV_PARAM_NO_SHORT_MAC' =>       "no-short-mac",       # uint
    'OSSL_PROV_PARAM_DRBG_TRUNC_DIGEST' =>  "drbg-no-trunc-md",   # uint
    'OSSL_PROV_PARAM_HKDF_DIGEST_CHECK' =>      "hkdf-digest-check",      # uint
    'OSSL_PROV_PARAM_TLS13_KDF_DIGEST_CHECK' => "tls13-kdf-digest-check", # uint
    'OSSL_PROV_PARAM_TLS1_PRF_DIGEST_CHECK' =>  "tls1-prf-digest-check",  # uint
    'OSSL_PROV_PARAM_SSHKDF_DIGEST_CHECK' =>    "sshkdf-digest-check",    # uint
    'OSSL_PROV_PARAM_SSKDF_DIGEST_CHECK' =>     "sskdf-digest-check",     # uint
    'OSSL_PROV_PARAM_X963KDF_DIGEST_CHECK' =>   "x963kdf-digest-check",   # uint
    'OSSL_PROV_PARAM_DSA_SIGN_DISABLED' =>      "dsa-sign-disabled",      # uint
    'OSSL_PROV_PARAM_TDES_ENCRYPT_DISABLED' =>  "tdes-encrypt-disabled",  # uint
    'OSSL_PROV_PARAM_RSA_PSS_SALTLEN_CHECK' =>  "rsa-pss-saltlen-check",  # uint
    'OSSL_PROV_PARAM_RSA_SIGN_X931_PAD_DISABLED' =>  "rsa-sign-x931-pad-disabled",   # uint
    'OSSL_PROV_PARAM_RSA_PKCS15_PAD_DISABLED' => "rsa-pkcs15-pad-disabled", # uint
    'OSSL_PROV_PARAM_HKDF_KEY_CHECK' =>         "hkdf-key-check",         # uint
    'OSSL_PROV_PARAM_KBKDF_KEY_CHECK' =>        "kbkdf-key-check",        # uint
    'OSSL_PROV_PARAM_TLS13_KDF_KEY_CHECK' =>    "tls13-kdf-key-check",    # uint
    'OSSL_PROV_PARAM_TLS1_PRF_KEY_CHECK' =>     "tls1-prf-key-check",     # uint
    'OSSL_PROV_PARAM_SSHKDF_KEY_CHECK' =>       "sshkdf-key-check",       # uint
    'OSSL_PROV_PARAM_SSKDF_KEY_CHECK' =>        "sskdf-key-check",        # uint
    'OSSL_PROV_PARAM_X963KDF_KEY_CHECK' =>      "x963kdf-key-check",      # uint
    'OSSL_PROV_PARAM_X942KDF_KEY_CHECK' =>      "x942kdf-key-check",      # uint
    'OSSL_PROV_PARAM_PBKDF2_LOWER_BOUND_CHECK' => "pbkdf2-lower-bound-check", # uint
    'OSSL_PROV_PARAM_ECDH_COFACTOR_CHECK' =>    "ecdh-cofactor-check",    # uint
    'OSSL_PROV_PARAM_SIGNATURE_DIGEST_CHECK' => "signature-digest-check", # uint

# Self test callback parameters
    'OSSL_PROV_PARAM_SELF_TEST_PHASE' =>  "st-phase",# utf8_string
    'OSSL_PROV_PARAM_SELF_TEST_TYPE' =>   "st-type", # utf8_string
    'OSSL_PROV_PARAM_SELF_TEST_DESC' =>   "st-desc", # utf8_string

# Provider-native object abstractions
#
# These are used when a provider wants to pass object data or an object
# reference back to libcrypto.  This is only useful for provider functions
# that take a callback to which an PARAM array with these parameters
# can be passed.
#
# This set of parameter names is explained in detail in provider-object(7)
# (doc/man7/provider-object.pod)

    'OSSL_OBJECT_PARAM_TYPE' =>              "type",     # INTEGER
    'OSSL_OBJECT_PARAM_DATA_TYPE' =>         "data-type",# UTF8_STRING
    'OSSL_OBJECT_PARAM_DATA_STRUCTURE' =>    "data-structure",# UTF8_STRING
    'OSSL_OBJECT_PARAM_REFERENCE' =>         "reference",# OCTET_STRING
    'OSSL_OBJECT_PARAM_DATA' =>              "data",# OCTET_STRING or UTF8_STRING
    'OSSL_OBJECT_PARAM_DESC' =>              "desc",     # UTF8_STRING
    'OSSL_OBJECT_PARAM_INPUT_TYPE' =>        "input-type", # UTF8_STRING

# Algorithm parameters
# If "engine",or "properties",are specified, they should always be paired
# with the algorithm type.
# Note these are common names that are shared by many types (such as kdf, mac,
# and pkey) e.g: see MAC_PARAM_DIGEST below.

    'OSSL_ALG_PARAM_DIGEST' =>       "digest",       # utf8_string
    'OSSL_ALG_PARAM_CIPHER' =>       "cipher",       # utf8_string
    'OSSL_ALG_PARAM_ENGINE' =>       "engine",       # utf8_string
    'OSSL_ALG_PARAM_MAC' =>          "mac",          # utf8_string
    'OSSL_ALG_PARAM_PROPERTIES' =>   "properties",   # utf8_string
    'OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR' => 'fips-indicator',   # int, -1, 0 or 1
    'OSSL_ALG_PARAM_SECURITY_CATEGORY' => "security-category", # int, 0 .. 5

    # For any operation that deals with AlgorithmIdentifier, they should
    # implement both of these.
    # ALG_PARAM_ALGORITHM_ID is intended to be gettable, and is the
    # implementation's idea of what its full AlgID should look like.
    # ALG_PARAM_ALGORITHM_ID_PARAMS is intended to be both settable
    # and gettable, to allow the calling application to pass or get
    # AlgID parameters to and from the provided implementation.
    'OSSL_ALG_PARAM_ALGORITHM_ID' => "algorithm-id", # octet_string (DER)
    'OSSL_ALG_PARAM_ALGORITHM_ID_PARAMS' =>  "algorithm-id-params", # octet_string

# cipher parameters
    'OSSL_CIPHER_PARAM_PADDING' =>              "padding",     # uint
    'OSSL_CIPHER_PARAM_USE_BITS' =>             "use-bits",    # uint
    'OSSL_CIPHER_PARAM_TLS_VERSION' =>          "tls-version", # uint
    'OSSL_CIPHER_PARAM_TLS_MAC' =>              "tls-mac",     # octet_ptr
    'OSSL_CIPHER_PARAM_TLS_MAC_SIZE' =>         "tls-mac-size",# size_t
    'OSSL_CIPHER_PARAM_MODE' =>                 "mode",        # uint
    'OSSL_CIPHER_PARAM_BLOCK_SIZE' =>           "blocksize",   # size_t
    'OSSL_CIPHER_PARAM_AEAD' =>                 "aead",        # int, 0 or 1
    'OSSL_CIPHER_PARAM_CUSTOM_IV' =>            "custom-iv",   # int, 0 or 1
    'OSSL_CIPHER_PARAM_CTS' =>                  "cts",         # int, 0 or 1
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK' =>      "tls-multi",   # int, 0 or 1
    'OSSL_CIPHER_PARAM_HAS_RAND_KEY' =>         "has-randkey", # int, 0 or 1
    'OSSL_CIPHER_PARAM_KEYLEN' =>               "keylen",      # size_t
    'OSSL_CIPHER_PARAM_IVLEN' =>                "ivlen",       # size_t
    'OSSL_CIPHER_PARAM_IV' =>                   "iv",          # octet_string OR octet_ptr
    'OSSL_CIPHER_PARAM_UPDATED_IV' =>           "updated-iv",  # octet_string OR octet_ptr
    'OSSL_CIPHER_PARAM_NUM' =>                  "num",         # uint
    'OSSL_CIPHER_PARAM_ROUNDS' =>               "rounds",      # uint
    'OSSL_CIPHER_PARAM_AEAD_TAG' =>             "tag",         # octet_string
    'OSSL_CIPHER_PARAM_PIPELINE_AEAD_TAG' =>    "pipeline-tag",# octet_ptr
    'OSSL_CIPHER_PARAM_AEAD_TLS1_AAD' =>        "tlsaad",      # octet_string
    'OSSL_CIPHER_PARAM_AEAD_TLS1_AAD_PAD' =>    "tlsaadpad",   # size_t
    'OSSL_CIPHER_PARAM_AEAD_TLS1_IV_FIXED' =>   "tlsivfixed",  # octet_string
    'OSSL_CIPHER_PARAM_AEAD_TLS1_GET_IV_GEN' => "tlsivgen",    # octet_string
    'OSSL_CIPHER_PARAM_AEAD_TLS1_SET_IV_INV' => "tlsivinv",    # octet_string
    'OSSL_CIPHER_PARAM_AEAD_IVLEN' =>           '*OSSL_CIPHER_PARAM_IVLEN',
    'OSSL_CIPHER_PARAM_AEAD_IV_GENERATED' => "iv-generated",   # uint
    'OSSL_CIPHER_PARAM_AEAD_TAGLEN' =>          "taglen",      # size_t
    'OSSL_CIPHER_PARAM_AEAD_MAC_KEY' =>         "mackey",      # octet_string
    'OSSL_CIPHER_PARAM_RANDOM_KEY' =>           "randkey",     # octet_string
    'OSSL_CIPHER_PARAM_RC2_KEYBITS' =>          "keybits",     # size_t
    'OSSL_CIPHER_PARAM_SPEED' =>                "speed",       # uint
    'OSSL_CIPHER_PARAM_CTS_MODE' =>             "cts_mode",    # utf8_string
    'OSSL_CIPHER_PARAM_DECRYPT_ONLY' =>         "decrypt-only",  # int, 0 or 1
    'OSSL_CIPHER_PARAM_FIPS_ENCRYPT_CHECK' =>   "encrypt-check", # int
    'OSSL_CIPHER_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',
    'OSSL_CIPHER_PARAM_ALGORITHM_ID' =>         '*OSSL_ALG_PARAM_ALGORITHM_ID',
    # Historically, CIPHER_PARAM_ALGORITHM_ID_PARAMS_OLD was used.  For the
    # time being, the old libcrypto functions will use both, so old providers
    # continue to work.
    # New providers are encouraged to use CIPHER_PARAM_ALGORITHM_ID_PARAMS.
    'OSSL_CIPHER_PARAM_ALGORITHM_ID_PARAMS' =>  '*OSSL_ALG_PARAM_ALGORITHM_ID_PARAMS',
    'OSSL_CIPHER_PARAM_ALGORITHM_ID_PARAMS_OLD' => "alg_id_param", # octet_string
    'OSSL_CIPHER_PARAM_XTS_STANDARD' =>         "xts_standard",# utf8_string
    'OSSL_CIPHER_PARAM_ENCRYPT_THEN_MAC' =>     "encrypt-then-mac",# int, 0 or 1
    'OSSL_CIPHER_HMAC_PARAM_MAC' =>             "*OSSL_CIPHER_PARAM_AEAD_TAG",

    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_MAX_SEND_FRAGMENT' =>  "tls1multi_maxsndfrag",# uint
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_MAX_BUFSIZE' =>        "tls1multi_maxbufsz",  # size_t
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_INTERLEAVE' =>         "tls1multi_interleave",# uint
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_AAD' =>                "tls1multi_aad",       # octet_string
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_AAD_PACKLEN' =>        "tls1multi_aadpacklen",# uint
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC' =>                "tls1multi_enc",       # octet_string
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC_IN' =>             "tls1multi_encin",     # octet_string
    'OSSL_CIPHER_PARAM_TLS1_MULTIBLOCK_ENC_LEN' =>            "tls1multi_enclen",    # size_t

# digest parameters
    'OSSL_DIGEST_PARAM_XOFLEN' =>       "xoflen",       # size_t
    'OSSL_DIGEST_PARAM_SSL3_MS' =>      "ssl3-ms",      # octet string
    'OSSL_DIGEST_PARAM_PAD_TYPE' =>     "pad-type",     # uint
    'OSSL_DIGEST_PARAM_MICALG' =>       "micalg",       # utf8 string
    'OSSL_DIGEST_PARAM_BLOCK_SIZE' =>   "blocksize",    # size_t
    'OSSL_DIGEST_PARAM_SIZE' =>         "size",         # size_t
    'OSSL_DIGEST_PARAM_XOF' =>          "xof",          # int, 0 or 1
    'OSSL_DIGEST_PARAM_ALGID_ABSENT' => "algid-absent", # int, 0 or 1

# MAC parameters
    'OSSL_MAC_PARAM_KEY' =>            "key",           # octet string
    'OSSL_MAC_PARAM_IV' =>             "iv",            # octet string
    'OSSL_MAC_PARAM_CUSTOM' =>         "custom",        # utf8 string
    'OSSL_MAC_PARAM_SALT' =>           "salt",          # octet string
    'OSSL_MAC_PARAM_XOF' =>            "xof",           # int, 0 or 1
    'OSSL_MAC_PARAM_DIGEST_NOINIT' =>  "digest-noinit", # int, 0 or 1
    'OSSL_MAC_PARAM_DIGEST_ONESHOT' => "digest-oneshot",# int, 0 or 1
    'OSSL_MAC_PARAM_C_ROUNDS' =>       "c-rounds",      # unsigned int
    'OSSL_MAC_PARAM_D_ROUNDS' =>       "d-rounds",      # unsigned int

# If "engine",or "properties",are specified, they should always be paired
# with "cipher",or "digest".

    'OSSL_MAC_PARAM_CIPHER' =>           '*OSSL_ALG_PARAM_CIPHER',        # utf8 string
    'OSSL_MAC_PARAM_DIGEST' =>           '*OSSL_ALG_PARAM_DIGEST',        # utf8 string
    'OSSL_MAC_PARAM_PROPERTIES' =>       '*OSSL_ALG_PARAM_PROPERTIES',    # utf8 string
    'OSSL_MAC_PARAM_SIZE' =>             "size",                     # size_t
    'OSSL_MAC_PARAM_BLOCK_SIZE' =>       "block-size",               # size_t
    'OSSL_MAC_PARAM_TLS_DATA_SIZE' =>    "tls-data-size",            # size_t
    'OSSL_MAC_PARAM_FIPS_NO_SHORT_MAC' =>'*OSSL_PROV_PARAM_NO_SHORT_MAC',
    'OSSL_MAC_PARAM_FIPS_KEY_CHECK' =>   '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_MAC_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',
    'OSSL_MAC_PARAM_FIPS_NO_SHORT_MAC' => '*OSSL_PROV_PARAM_NO_SHORT_MAC',

# KDF / PRF parameters
    'OSSL_KDF_PARAM_SECRET' =>       "secret",                   # octet string
    'OSSL_KDF_PARAM_KEY' =>          "key",                      # octet string
    'OSSL_KDF_PARAM_SALT' =>         "salt",                     # octet string
    'OSSL_KDF_PARAM_PASSWORD' =>     "pass",                     # octet string
    'OSSL_KDF_PARAM_PREFIX' =>       "prefix",                   # octet string
    'OSSL_KDF_PARAM_LABEL' =>        "label",                    # octet string
    'OSSL_KDF_PARAM_DATA' =>         "data",                     # octet string
    'OSSL_KDF_PARAM_DIGEST' =>       '*OSSL_ALG_PARAM_DIGEST',        # utf8 string
    'OSSL_KDF_PARAM_CIPHER' =>       '*OSSL_ALG_PARAM_CIPHER',        # utf8 string
    'OSSL_KDF_PARAM_MAC' =>          '*OSSL_ALG_PARAM_MAC',           # utf8 string
    'OSSL_KDF_PARAM_MAC_SIZE' =>     "maclen",                   # size_t
    'OSSL_KDF_PARAM_PROPERTIES' =>   '*OSSL_ALG_PARAM_PROPERTIES',    # utf8 string
    'OSSL_KDF_PARAM_ITER' =>         "iter",                     # unsigned int
    'OSSL_KDF_PARAM_MODE' =>         "mode",                     # utf8 string or int
    'OSSL_KDF_PARAM_PKCS5' =>        "pkcs5",                    # int
    'OSSL_KDF_PARAM_UKM' =>          "ukm",                      # octet string
    'OSSL_KDF_PARAM_CEK_ALG' =>      "cekalg",                   # utf8 string
    'OSSL_KDF_PARAM_SCRYPT_N' =>     "n",                        # uint64_t
    'OSSL_KDF_PARAM_SCRYPT_R' =>     "r",                        # uint32_t
    'OSSL_KDF_PARAM_SCRYPT_P' =>     "p",                        # uint32_t
    'OSSL_KDF_PARAM_SCRYPT_MAXMEM' => "maxmem_bytes",            # uint64_t
    'OSSL_KDF_PARAM_INFO' =>         "info",                     # octet string
    'OSSL_KDF_PARAM_SEED' =>         "seed",                     # octet string
    'OSSL_KDF_PARAM_SSHKDF_XCGHASH' => "xcghash",                # octet string
    'OSSL_KDF_PARAM_SSHKDF_SESSION_ID' => "session_id",          # octet string
    'OSSL_KDF_PARAM_SSHKDF_TYPE' =>  "type",                     # int
    'OSSL_KDF_PARAM_SIZE' =>         "size",                     # size_t
    'OSSL_KDF_PARAM_CONSTANT' =>     "constant",                 # octet string
    'OSSL_KDF_PARAM_PKCS12_ID' =>    "id",                       # int
    'OSSL_KDF_PARAM_KBKDF_USE_L' =>          "use-l",            # int
    'OSSL_KDF_PARAM_KBKDF_USE_SEPARATOR' =>  "use-separator",    # int
    'OSSL_KDF_PARAM_KBKDF_R' =>      "r",                        # int
    'OSSL_KDF_PARAM_X942_ACVPINFO' =>        "acvp-info",
    'OSSL_KDF_PARAM_X942_PARTYUINFO' =>      "partyu-info",
    'OSSL_KDF_PARAM_X942_PARTYVINFO' =>      "partyv-info",
    'OSSL_KDF_PARAM_X942_SUPP_PUBINFO' =>    "supp-pubinfo",
    'OSSL_KDF_PARAM_X942_SUPP_PRIVINFO' =>   "supp-privinfo",
    'OSSL_KDF_PARAM_X942_USE_KEYBITS' =>     "use-keybits",
    'OSSL_KDF_PARAM_HMACDRBG_ENTROPY' =>     "entropy",
    'OSSL_KDF_PARAM_HMACDRBG_NONCE' =>       "nonce",
    'OSSL_KDF_PARAM_THREADS' =>        "threads",                # uint32_t
    'OSSL_KDF_PARAM_EARLY_CLEAN' =>    "early_clean",            # uint32_t
    'OSSL_KDF_PARAM_ARGON2_AD' =>      "ad",                     # octet string
    'OSSL_KDF_PARAM_ARGON2_LANES' =>   "lanes",                  # uint32_t
    'OSSL_KDF_PARAM_ARGON2_MEMCOST' => "memcost",                # uint32_t
    'OSSL_KDF_PARAM_ARGON2_VERSION' => "version",                # uint32_t
    'OSSL_KDF_PARAM_FIPS_EMS_CHECK' => "ems_check",              # int
    'OSSL_KDF_PARAM_FIPS_DIGEST_CHECK' => '*OSSL_PKEY_PARAM_FIPS_DIGEST_CHECK',
    'OSSL_KDF_PARAM_FIPS_KEY_CHECK' => '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_KDF_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# Known RAND names
    'OSSL_RAND_PARAM_STATE' =>                   "state",
    'OSSL_RAND_PARAM_STRENGTH' =>                "strength",
    'OSSL_RAND_PARAM_MAX_REQUEST' =>             "max_request",
    'OSSL_RAND_PARAM_TEST_ENTROPY' =>            "test_entropy",
    'OSSL_RAND_PARAM_TEST_NONCE' =>              "test_nonce",
    'OSSL_RAND_PARAM_GENERATE' =>                "generate",
    'OSSL_RAND_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# RAND/DRBG names
    'OSSL_DRBG_PARAM_RESEED_REQUESTS' =>         "reseed_requests",
    'OSSL_DRBG_PARAM_RESEED_TIME_INTERVAL' =>    "reseed_time_interval",
    'OSSL_DRBG_PARAM_MIN_ENTROPYLEN' =>          "min_entropylen",
    'OSSL_DRBG_PARAM_MAX_ENTROPYLEN' =>          "max_entropylen",
    'OSSL_DRBG_PARAM_MIN_NONCELEN' =>            "min_noncelen",
    'OSSL_DRBG_PARAM_MAX_NONCELEN' =>            "max_noncelen",
    'OSSL_DRBG_PARAM_MAX_PERSLEN' =>             "max_perslen",
    'OSSL_DRBG_PARAM_MAX_ADINLEN' =>             "max_adinlen",
    'OSSL_DRBG_PARAM_RESEED_COUNTER' =>          "reseed_counter",
    'OSSL_DRBG_PARAM_RESEED_TIME' =>             "reseed_time",
    'OSSL_DRBG_PARAM_PROPERTIES' =>              '*OSSL_ALG_PARAM_PROPERTIES',
    'OSSL_DRBG_PARAM_DIGEST' =>                  '*OSSL_ALG_PARAM_DIGEST',
    'OSSL_DRBG_PARAM_CIPHER' =>                  '*OSSL_ALG_PARAM_CIPHER',
    'OSSL_DRBG_PARAM_MAC' =>                     '*OSSL_ALG_PARAM_MAC',
    'OSSL_DRBG_PARAM_USE_DF' =>                  "use_derivation_function",
    'OSSL_DRBG_PARAM_FIPS_DIGEST_CHECK' =>       '*OSSL_PKEY_PARAM_FIPS_DIGEST_CHECK',
    'OSSL_DRBG_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# DRBG call back parameters
    'OSSL_DRBG_PARAM_ENTROPY_REQUIRED' =>        "entropy_required",
    'OSSL_DRBG_PARAM_PREDICTION_RESISTANCE' =>   "prediction_resistance",
    'OSSL_DRBG_PARAM_MIN_LENGTH' =>              "minium_length",
    'OSSL_DRBG_PARAM_MAX_LENGTH' =>              "maxium_length",
    'OSSL_DRBG_PARAM_RANDOM_DATA' =>             "random_data",
    'OSSL_DRBG_PARAM_SIZE' =>                    "size",

# PKEY parameters
# Common PKEY parameters
    'OSSL_PKEY_PARAM_BITS' =>                "bits",# integer
    'OSSL_PKEY_PARAM_MAX_SIZE' =>            "max-size",# integer
    'OSSL_PKEY_PARAM_SECURITY_BITS' =>       "security-bits",# integer
    'OSSL_PKEY_PARAM_SECURITY_CATEGORY' =>   '*OSSL_ALG_PARAM_SECURITY_CATEGORY',
    'OSSL_PKEY_PARAM_DIGEST' =>              '*OSSL_ALG_PARAM_DIGEST',
    'OSSL_PKEY_PARAM_CIPHER' =>              '*OSSL_ALG_PARAM_CIPHER', # utf8 string
    'OSSL_PKEY_PARAM_ENGINE' =>              '*OSSL_ALG_PARAM_ENGINE', # utf8 string
    'OSSL_PKEY_PARAM_PROPERTIES' =>          '*OSSL_ALG_PARAM_PROPERTIES',
    'OSSL_PKEY_PARAM_DEFAULT_DIGEST' =>      "default-digest",# utf8 string
    'OSSL_PKEY_PARAM_MANDATORY_DIGEST' =>    "mandatory-digest",# utf8 string
    'OSSL_PKEY_PARAM_PAD_MODE' =>            "pad-mode",
    'OSSL_PKEY_PARAM_DIGEST_SIZE' =>         "digest-size",
    'OSSL_PKEY_PARAM_MASKGENFUNC' =>         "mgf",
    'OSSL_PKEY_PARAM_MGF1_DIGEST' =>         "mgf1-digest",
    'OSSL_PKEY_PARAM_MGF1_PROPERTIES' =>     "mgf1-properties",
    'OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY' =>  "encoded-pub-key",
    'OSSL_PKEY_PARAM_GROUP_NAME' =>          "group",
    'OSSL_PKEY_PARAM_DIST_ID' =>             "distid",
    'OSSL_PKEY_PARAM_PUB_KEY' =>             "pub",
    'OSSL_PKEY_PARAM_PRIV_KEY' =>            "priv",
    # PKEY_PARAM_IMPLICIT_REJECTION isn't actually used, or meaningful.  We keep
    # it for API stability, but please use ASYM_CIPHER_PARAM_IMPLICIT_REJECTION
    # instead.
    'OSSL_PKEY_PARAM_IMPLICIT_REJECTION' =>  "implicit-rejection",
    'OSSL_PKEY_PARAM_FIPS_DIGEST_CHECK' =>   "digest-check",
    'OSSL_PKEY_PARAM_FIPS_KEY_CHECK' =>      "key-check",
    'OSSL_PKEY_PARAM_ALGORITHM_ID' =>        '*OSSL_ALG_PARAM_ALGORITHM_ID',
    'OSSL_PKEY_PARAM_ALGORITHM_ID_PARAMS' => '*OSSL_ALG_PARAM_ALGORITHM_ID_PARAMS',
    'OSSL_PKEY_PARAM_CMS_RI_TYPE' =>         "ri-type", # integer
    'OSSL_PKEY_PARAM_CMS_KEMRI_KDF_ALGORITHM' => "kemri-kdf-alg",

# Diffie-Hellman/DSA Parameters
    'OSSL_PKEY_PARAM_FFC_P' =>               "p",
    'OSSL_PKEY_PARAM_FFC_G' =>               "g",
    'OSSL_PKEY_PARAM_FFC_Q' =>               "q",
    'OSSL_PKEY_PARAM_FFC_GINDEX' =>          "gindex",
    'OSSL_PKEY_PARAM_FFC_PCOUNTER' =>        "pcounter",
    'OSSL_PKEY_PARAM_FFC_SEED' =>            "seed",
    'OSSL_PKEY_PARAM_FFC_COFACTOR' =>        "j",
    'OSSL_PKEY_PARAM_FFC_H' =>               "hindex",
    'OSSL_PKEY_PARAM_FFC_VALIDATE_PQ' =>     "validate-pq",
    'OSSL_PKEY_PARAM_FFC_VALIDATE_G' =>      "validate-g",
    'OSSL_PKEY_PARAM_FFC_VALIDATE_LEGACY' => "validate-legacy",

# Diffie-Hellman params
    'OSSL_PKEY_PARAM_DH_GENERATOR' =>        "safeprime-generator",
    'OSSL_PKEY_PARAM_DH_PRIV_LEN' =>         "priv_len",

# Elliptic Curve Domain Parameters
    'OSSL_PKEY_PARAM_EC_PUB_X' =>     "qx",
    'OSSL_PKEY_PARAM_EC_PUB_Y' =>     "qy",

# Elliptic Curve Explicit Domain Parameters
    'OSSL_PKEY_PARAM_EC_FIELD_TYPE' =>                   "field-type",
    'OSSL_PKEY_PARAM_EC_P' =>                            "p",
    'OSSL_PKEY_PARAM_EC_A' =>                            "a",
    'OSSL_PKEY_PARAM_EC_B' =>                            "b",
    'OSSL_PKEY_PARAM_EC_GENERATOR' =>                    "generator",
    'OSSL_PKEY_PARAM_EC_ORDER' =>                        "order",
    'OSSL_PKEY_PARAM_EC_COFACTOR' =>                     "cofactor",
    'OSSL_PKEY_PARAM_EC_SEED' =>                         "seed",
    'OSSL_PKEY_PARAM_EC_CHAR2_M' =>                      "m",
    'OSSL_PKEY_PARAM_EC_CHAR2_TYPE' =>                   "basis-type",
    'OSSL_PKEY_PARAM_EC_CHAR2_TP_BASIS' =>               "tp",
    'OSSL_PKEY_PARAM_EC_CHAR2_PP_K1' =>                  "k1",
    'OSSL_PKEY_PARAM_EC_CHAR2_PP_K2' =>                  "k2",
    'OSSL_PKEY_PARAM_EC_CHAR2_PP_K3' =>                  "k3",
    'OSSL_PKEY_PARAM_EC_DECODED_FROM_EXPLICIT_PARAMS' => "decoded-from-explicit",

# Elliptic Curve Key Parameters
    'OSSL_PKEY_PARAM_USE_COFACTOR_FLAG' => "use-cofactor-flag",
    'OSSL_PKEY_PARAM_USE_COFACTOR_ECDH' => '*OSSL_PKEY_PARAM_USE_COFACTOR_FLAG',

# RSA Keys
#
# n, e, d are the usual public and private key components
#
# rsa-num is the number of factors, including p and q
# rsa-factor is used for each factor: p, q, r_i (i = 3, ...)
# rsa-exponent is used for each exponent: dP, dQ, d_i (i = 3, ...)
# rsa-coefficient is used for each coefficient: qInv, t_i (i = 3, ...)
#
# The number of rsa-factor items must be equal to the number of rsa-exponent
# items, and the number of rsa-coefficients must be one less.
# (the base i for the coefficients is 2, not 1, at least as implied by
# RFC 8017)

    'OSSL_PKEY_PARAM_RSA_N' =>           "n",
    'OSSL_PKEY_PARAM_RSA_E' =>           "e",
    'OSSL_PKEY_PARAM_RSA_D' =>           "d",
    'OSSL_PKEY_PARAM_RSA_FACTOR' =>      "rsa-factor",
    'OSSL_PKEY_PARAM_RSA_EXPONENT' =>    "rsa-exponent",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT' => "rsa-coefficient",
    'OSSL_PKEY_PARAM_RSA_FACTOR1' =>      "rsa-factor1",
    'OSSL_PKEY_PARAM_RSA_FACTOR2' =>      "rsa-factor2",
    'OSSL_PKEY_PARAM_RSA_FACTOR3' =>      "rsa-factor3",
    'OSSL_PKEY_PARAM_RSA_FACTOR4' =>      "rsa-factor4",
    'OSSL_PKEY_PARAM_RSA_FACTOR5' =>      "rsa-factor5",
    'OSSL_PKEY_PARAM_RSA_FACTOR6' =>      "rsa-factor6",
    'OSSL_PKEY_PARAM_RSA_FACTOR7' =>      "rsa-factor7",
    'OSSL_PKEY_PARAM_RSA_FACTOR8' =>      "rsa-factor8",
    'OSSL_PKEY_PARAM_RSA_FACTOR9' =>      "rsa-factor9",
    'OSSL_PKEY_PARAM_RSA_FACTOR10' =>     "rsa-factor10",
    'OSSL_PKEY_PARAM_RSA_EXPONENT1' =>    "rsa-exponent1",
    'OSSL_PKEY_PARAM_RSA_EXPONENT2' =>    "rsa-exponent2",
    'OSSL_PKEY_PARAM_RSA_EXPONENT3' =>    "rsa-exponent3",
    'OSSL_PKEY_PARAM_RSA_EXPONENT4' =>    "rsa-exponent4",
    'OSSL_PKEY_PARAM_RSA_EXPONENT5' =>    "rsa-exponent5",
    'OSSL_PKEY_PARAM_RSA_EXPONENT6' =>    "rsa-exponent6",
    'OSSL_PKEY_PARAM_RSA_EXPONENT7' =>    "rsa-exponent7",
    'OSSL_PKEY_PARAM_RSA_EXPONENT8' =>    "rsa-exponent8",
    'OSSL_PKEY_PARAM_RSA_EXPONENT9' =>    "rsa-exponent9",
    'OSSL_PKEY_PARAM_RSA_EXPONENT10' =>   "rsa-exponent10",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT1' => "rsa-coefficient1",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT2' => "rsa-coefficient2",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT3' => "rsa-coefficient3",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT4' => "rsa-coefficient4",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT5' => "rsa-coefficient5",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT6' => "rsa-coefficient6",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT7' => "rsa-coefficient7",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT8' => "rsa-coefficient8",
    'OSSL_PKEY_PARAM_RSA_COEFFICIENT9' => "rsa-coefficient9",

# Key generation parameters
    'OSSL_PKEY_PARAM_RSA_BITS' =>             '*OSSL_PKEY_PARAM_BITS',
    'OSSL_PKEY_PARAM_RSA_PRIMES' =>           "primes",
    'OSSL_PKEY_PARAM_RSA_DIGEST' =>           '*OSSL_PKEY_PARAM_DIGEST',
    'OSSL_PKEY_PARAM_RSA_DIGEST_PROPS' =>     '*OSSL_PKEY_PARAM_PROPERTIES',
    'OSSL_PKEY_PARAM_RSA_MASKGENFUNC' =>      '*OSSL_PKEY_PARAM_MASKGENFUNC',
    'OSSL_PKEY_PARAM_RSA_MGF1_DIGEST' =>      '*OSSL_PKEY_PARAM_MGF1_DIGEST',
    'OSSL_PKEY_PARAM_RSA_PSS_SALTLEN' =>      "saltlen",
    'OSSL_PKEY_PARAM_RSA_DERIVE_FROM_PQ'    =>     "rsa-derive-from-pq",
    'OSSL_PKEY_PARAM_RSA_A' =>                "rsa-a",
    'OSSL_PKEY_PARAM_RSA_B' =>                "rsa-b",

# EC, X25519 and X448 Key generation parameters
    'OSSL_PKEY_PARAM_DHKEM_IKM' =>        "dhkem-ikm",

# ML-KEM parameters
    'OSSL_PKEY_PARAM_ML_KEM_SEED' => "seed",
    'OSSL_PKEY_PARAM_ML_KEM_PREFER_SEED' => "ml-kem.prefer_seed",
    'OSSL_PKEY_PARAM_ML_KEM_RETAIN_SEED' => "ml-kem.retain_seed",
    'OSSL_PKEY_PARAM_ML_KEM_INPUT_FORMATS' => "ml-kem.input_formats",
    'OSSL_PKEY_PARAM_ML_KEM_OUTPUT_FORMATS' => "ml-kem.output_formats",
    'OSSL_PKEY_PARAM_ML_KEM_IMPORT_PCT_TYPE' => "ml-kem.import_pct_type",

# Key generation parameters
    'OSSL_PKEY_PARAM_FFC_TYPE' =>         "type",
    'OSSL_PKEY_PARAM_FFC_PBITS' =>        "pbits",
    'OSSL_PKEY_PARAM_FFC_QBITS' =>        "qbits",
    'OSSL_PKEY_PARAM_FFC_DIGEST' =>       '*OSSL_PKEY_PARAM_DIGEST',
    'OSSL_PKEY_PARAM_FFC_DIGEST_PROPS' => '*OSSL_PKEY_PARAM_PROPERTIES',

    'OSSL_PKEY_PARAM_EC_ENCODING' =>                "encoding",# utf8_string
    'OSSL_PKEY_PARAM_EC_POINT_CONVERSION_FORMAT' => "point-format",
    'OSSL_PKEY_PARAM_EC_GROUP_CHECK_TYPE' =>        "group-check",
    'OSSL_PKEY_PARAM_EC_INCLUDE_PUBLIC' =>          "include-public",
    'OSSL_PKEY_PARAM_FIPS_SIGN_CHECK' =>            "sign-check",
    'OSSL_PKEY_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# ML_DSA Key generation parameter
    'OSSL_PKEY_PARAM_ML_DSA_SEED' =>             "seed",
    'OSSL_PKEY_PARAM_ML_DSA_RETAIN_SEED' =>      "ml-dsa.retain_seed",
    'OSSL_PKEY_PARAM_ML_DSA_PREFER_SEED' =>      "ml-dsa.prefer_seed",
    'OSSL_PKEY_PARAM_ML_DSA_INPUT_FORMATS' =>    "ml-dsa.input_formats",
    'OSSL_PKEY_PARAM_ML_DSA_OUTPUT_FORMATS' =>   "ml-dsa.output_formats",

# SLH_DSA Key generation parameters
    'OSSL_PKEY_PARAM_SLH_DSA_SEED' =>              "seed",

# Key Exchange parameters
    'OSSL_EXCHANGE_PARAM_PAD' =>                   "pad",# uint
    'OSSL_EXCHANGE_PARAM_EC_ECDH_COFACTOR_MODE' => "ecdh-cofactor-mode",# int
    'OSSL_EXCHANGE_PARAM_KDF_TYPE' =>              "kdf-type",# utf8_string
    'OSSL_EXCHANGE_PARAM_KDF_DIGEST' =>            "kdf-digest",# utf8_string
    'OSSL_EXCHANGE_PARAM_KDF_DIGEST_PROPS' =>      "kdf-digest-props",# utf8_string
    'OSSL_EXCHANGE_PARAM_KDF_OUTLEN' =>            "kdf-outlen",# size_t
# The following parameter is an octet_string on set and an octet_ptr on get
    'OSSL_EXCHANGE_PARAM_KDF_UKM' =>               "kdf-ukm",
    'OSSL_EXCHANGE_PARAM_FIPS_DIGEST_CHECK' =>     '*OSSL_PKEY_PARAM_FIPS_DIGEST_CHECK',
    'OSSL_EXCHANGE_PARAM_FIPS_KEY_CHECK' =>        '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_EXCHANGE_PARAM_FIPS_ECDH_COFACTOR_CHECK' => '*OSSL_PROV_PARAM_ECDH_COFACTOR_CHECK',
    'OSSL_EXCHANGE_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# Signature parameters
    'OSSL_SIGNATURE_PARAM_ALGORITHM_ID' =>         '*OSSL_PKEY_PARAM_ALGORITHM_ID',
    'OSSL_SIGNATURE_PARAM_ALGORITHM_ID_PARAMS' =>  '*OSSL_PKEY_PARAM_ALGORITHM_ID_PARAMS',
    'OSSL_SIGNATURE_PARAM_PAD_MODE' =>             '*OSSL_PKEY_PARAM_PAD_MODE',
    'OSSL_SIGNATURE_PARAM_DIGEST' =>               '*OSSL_PKEY_PARAM_DIGEST',
    'OSSL_SIGNATURE_PARAM_PROPERTIES' =>           '*OSSL_PKEY_PARAM_PROPERTIES',
    'OSSL_SIGNATURE_PARAM_PSS_SALTLEN' =>          "saltlen",
    'OSSL_SIGNATURE_PARAM_MGF1_DIGEST' =>          '*OSSL_PKEY_PARAM_MGF1_DIGEST',
    'OSSL_SIGNATURE_PARAM_MGF1_PROPERTIES' =>      '*OSSL_PKEY_PARAM_MGF1_PROPERTIES',
    'OSSL_SIGNATURE_PARAM_DIGEST_SIZE' =>          '*OSSL_PKEY_PARAM_DIGEST_SIZE',
    'OSSL_SIGNATURE_PARAM_NONCE_TYPE' =>           "nonce-type",
    'OSSL_SIGNATURE_PARAM_INSTANCE' =>             "instance",
    'OSSL_SIGNATURE_PARAM_CONTEXT_STRING' =>       "context-string",
    'OSSL_SIGNATURE_PARAM_FIPS_DIGEST_CHECK' =>    '*OSSL_PKEY_PARAM_FIPS_DIGEST_CHECK',
    'OSSL_SIGNATURE_PARAM_FIPS_VERIFY_MESSAGE' =>  'verify-message',
    'OSSL_SIGNATURE_PARAM_FIPS_KEY_CHECK' =>       '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_SIGNATURE_PARAM_FIPS_SIGN_CHECK' =>      '*OSSL_PKEY_PARAM_FIPS_SIGN_CHECK',
    'OSSL_SIGNATURE_PARAM_FIPS_RSA_PSS_SALTLEN_CHECK' => "rsa-pss-saltlen-check",
    'OSSL_SIGNATURE_PARAM_FIPS_SIGN_X931_PAD_CHECK' => "sign-x931-pad-check",
    'OSSL_SIGNATURE_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',
    'OSSL_SIGNATURE_PARAM_SIGNATURE' =>          "signature",
    'OSSL_SIGNATURE_PARAM_MESSAGE_ENCODING' =>   "message-encoding",
    'OSSL_SIGNATURE_PARAM_DETERMINISTIC' =>      "deterministic",
    'OSSL_SIGNATURE_PARAM_MU' =>                 "mu", # int
    'OSSL_SIGNATURE_PARAM_TEST_ENTROPY' =>       "test-entropy",
    'OSSL_SIGNATURE_PARAM_ADD_RANDOM' =>         "additional-random",

# Asym cipher parameters
    'OSSL_ASYM_CIPHER_PARAM_DIGEST' =>                   '*OSSL_PKEY_PARAM_DIGEST',
    'OSSL_ASYM_CIPHER_PARAM_PROPERTIES' =>               '*OSSL_PKEY_PARAM_PROPERTIES',
    'OSSL_ASYM_CIPHER_PARAM_ENGINE' =>                   '*OSSL_PKEY_PARAM_ENGINE',
    'OSSL_ASYM_CIPHER_PARAM_PAD_MODE' =>                 '*OSSL_PKEY_PARAM_PAD_MODE',
    'OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST' =>              '*OSSL_PKEY_PARAM_MGF1_DIGEST',
    'OSSL_ASYM_CIPHER_PARAM_MGF1_DIGEST_PROPS' =>        '*OSSL_PKEY_PARAM_MGF1_PROPERTIES',
    'OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST' =>              '*OSSL_ALG_PARAM_DIGEST',
    'OSSL_ASYM_CIPHER_PARAM_OAEP_DIGEST_PROPS' =>        "digest-props",
# The following parameter is an octet_string on set and an octet_ptr on get
    'OSSL_ASYM_CIPHER_PARAM_OAEP_LABEL' =>               "oaep-label",
    'OSSL_ASYM_CIPHER_PARAM_TLS_CLIENT_VERSION' =>       "tls-client-version",
    'OSSL_ASYM_CIPHER_PARAM_TLS_NEGOTIATED_VERSION' =>   "tls-negotiated-version",
    'OSSL_ASYM_CIPHER_PARAM_IMPLICIT_REJECTION' =>       "implicit-rejection",
    'OSSL_ASYM_CIPHER_PARAM_FIPS_RSA_PKCS15_PAD_DISABLED' => '*OSSL_PROV_PARAM_RSA_PKCS15_PAD_DISABLED',
    'OSSL_ASYM_CIPHER_PARAM_FIPS_KEY_CHECK' =>           '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_ASYM_CIPHER_PARAM_FIPS_APPROVED_INDICATOR' =>  '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# Encoder / decoder parameters

    'OSSL_ENCODER_PARAM_CIPHER' =>           '*OSSL_ALG_PARAM_CIPHER',
    'OSSL_ENCODER_PARAM_PROPERTIES' =>       '*OSSL_ALG_PARAM_PROPERTIES',
# Currently PVK only, but reusable for others as needed
    'OSSL_ENCODER_PARAM_ENCRYPT_LEVEL' =>    "encrypt-level",
    'OSSL_ENCODER_PARAM_SAVE_PARAMETERS' =>  "save-parameters",# integer

    'OSSL_DECODER_PARAM_PROPERTIES' =>       '*OSSL_ALG_PARAM_PROPERTIES',

# Passphrase callback parameters
    'OSSL_PASSPHRASE_PARAM_INFO' =>          "info",

# Keygen callback parameters, from provider to libcrypto
    'OSSL_GEN_PARAM_POTENTIAL' =>            "potential",# integer
    'OSSL_GEN_PARAM_ITERATION' =>            "iteration",# integer

# ACVP Test parameters : These should not be used normally
    'OSSL_PKEY_PARAM_RSA_TEST_XP1' => "xp1",
    'OSSL_PKEY_PARAM_RSA_TEST_XP2' => "xp2",
    'OSSL_PKEY_PARAM_RSA_TEST_XP' =>  "xp",
    'OSSL_PKEY_PARAM_RSA_TEST_XQ1' => "xq1",
    'OSSL_PKEY_PARAM_RSA_TEST_XQ2' => "xq2",
    'OSSL_PKEY_PARAM_RSA_TEST_XQ' =>  "xq",
    'OSSL_PKEY_PARAM_RSA_TEST_P1' =>  "p1",
    'OSSL_PKEY_PARAM_RSA_TEST_P2' =>  "p2",
    'OSSL_PKEY_PARAM_RSA_TEST_Q1' =>  "q1",
    'OSSL_PKEY_PARAM_RSA_TEST_Q2' =>  "q2",
    'OSSL_SIGNATURE_PARAM_KAT' =>     "kat",

# KEM parameters
    'OSSL_KEM_PARAM_OPERATION' =>            "operation",
    'OSSL_KEM_PARAM_IKME' =>                 "ikme",
    'OSSL_KEM_PARAM_FIPS_KEY_CHECK' =>       '*OSSL_PKEY_PARAM_FIPS_KEY_CHECK',
    'OSSL_KEM_PARAM_FIPS_APPROVED_INDICATOR' => '*OSSL_ALG_PARAM_FIPS_APPROVED_INDICATOR',

# Capabilities

# TLS-GROUP Capability
    'OSSL_CAPABILITY_TLS_GROUP_NAME' =>              "tls-group-name",
    'OSSL_CAPABILITY_TLS_GROUP_NAME_INTERNAL' =>     "tls-group-name-internal",
    'OSSL_CAPABILITY_TLS_GROUP_ID' =>                "tls-group-id",
    'OSSL_CAPABILITY_TLS_GROUP_ALG' =>               "tls-group-alg",
    'OSSL_CAPABILITY_TLS_GROUP_SECURITY_BITS' =>     "tls-group-sec-bits",
    'OSSL_CAPABILITY_TLS_GROUP_IS_KEM' =>            "tls-group-is-kem",
    'OSSL_CAPABILITY_TLS_GROUP_MIN_TLS' =>           "tls-min-tls",
    'OSSL_CAPABILITY_TLS_GROUP_MAX_TLS' =>           "tls-max-tls",
    'OSSL_CAPABILITY_TLS_GROUP_MIN_DTLS' =>          "tls-min-dtls",
    'OSSL_CAPABILITY_TLS_GROUP_MAX_DTLS' =>          "tls-max-dtls",

# TLS-SIGALG Capability
    'OSSL_CAPABILITY_TLS_SIGALG_IANA_NAME' =>         "tls-sigalg-iana-name",
    'OSSL_CAPABILITY_TLS_SIGALG_CODE_POINT' =>        "tls-sigalg-code-point",
    'OSSL_CAPABILITY_TLS_SIGALG_NAME' =>              "tls-sigalg-name",
    'OSSL_CAPABILITY_TLS_SIGALG_OID' =>               "tls-sigalg-oid",
    'OSSL_CAPABILITY_TLS_SIGALG_SIG_NAME' =>          "tls-sigalg-sig-name",
    'OSSL_CAPABILITY_TLS_SIGALG_SIG_OID' =>           "tls-sigalg-sig-oid",
    'OSSL_CAPABILITY_TLS_SIGALG_HASH_NAME' =>         "tls-sigalg-hash-name",
    'OSSL_CAPABILITY_TLS_SIGALG_HASH_OID' =>          "tls-sigalg-hash-oid",
    'OSSL_CAPABILITY_TLS_SIGALG_KEYTYPE' =>           "tls-sigalg-keytype",
    'OSSL_CAPABILITY_TLS_SIGALG_KEYTYPE_OID' =>       "tls-sigalg-keytype-oid",
    'OSSL_CAPABILITY_TLS_SIGALG_SECURITY_BITS' =>     "tls-sigalg-sec-bits",
    'OSSL_CAPABILITY_TLS_SIGALG_MIN_TLS' =>           "tls-min-tls",
    'OSSL_CAPABILITY_TLS_SIGALG_MAX_TLS' =>           "tls-max-tls",
    'OSSL_CAPABILITY_TLS_SIGALG_MIN_DTLS' =>          "tls-min-dtls",
    'OSSL_CAPABILITY_TLS_SIGALG_MAX_DTLS' =>          "tls-max-dtls",

# storemgmt parameters


# Used by storemgmt_ctx_set_params():
#
# - STORE_PARAM_EXPECT is an INTEGER, and the value is any of the
#   STORE_INFO numbers.  This is used to set the expected type of
#   object loaded.
#
# - STORE_PARAM_SUBJECT, STORE_PARAM_ISSUER,
#   STORE_PARAM_SERIAL, STORE_PARAM_FINGERPRINT,
#   STORE_PARAM_DIGEST, STORE_PARAM_ALIAS
#   are used as search criteria.
#   (STORE_PARAM_DIGEST is used with STORE_PARAM_FINGERPRINT)

    'OSSL_STORE_PARAM_EXPECT' =>     "expect",       # INTEGER
    'OSSL_STORE_PARAM_SUBJECT' =>    "subject",      # DER blob => OCTET_STRING
    'OSSL_STORE_PARAM_ISSUER' =>     "name",         # DER blob => OCTET_STRING
    'OSSL_STORE_PARAM_SERIAL' =>     "serial",       # INTEGER
    'OSSL_STORE_PARAM_DIGEST' =>     "digest",       # UTF8_STRING
    'OSSL_STORE_PARAM_FINGERPRINT' => "fingerprint", # OCTET_STRING
    'OSSL_STORE_PARAM_ALIAS' =>      "alias",        # UTF8_STRING

# You may want to pass properties for the provider implementation to use
    'OSSL_STORE_PARAM_PROPERTIES' => "properties",   # utf8_string
# DECODER input type if a decoder is used by the store
    'OSSL_STORE_PARAM_INPUT_TYPE' => "input-type",   # UTF8_STRING


# Libssl record layer
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_OPTIONS' =>        "options",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_MODE' =>           "mode",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_READ_AHEAD' =>     "read_ahead",
    'OSSL_LIBSSL_RECORD_LAYER_READ_BUFFER_LEN' =>      "read_buffer_len",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_USE_ETM' =>        "use_etm",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_STREAM_MAC' =>     "stream_mac",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_TLSTREE' =>        "tlstree",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_MAX_FRAG_LEN' =>   "max_frag_len",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_MAX_EARLY_DATA' => "max_early_data",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_BLOCK_PADDING' =>  "block_padding",
    'OSSL_LIBSSL_RECORD_LAYER_PARAM_HS_PADDING' =>     "hs_padding",

# Symmetric Key parameters
    'OSSL_SKEY_PARAM_RAW_BYTES' => "raw-bytes",
    'OSSL_SKEY_PARAM_KEY_LENGTH' => "key-length",
);

sub output_ifdef {
    my $cond = shift;

    if (defined($cond)) {
        print "# if" . $cond . "\n";
    }
}

sub output_else {
    my $cond = shift;
    my $body = shift;

    if (defined($cond)) {
        print "# else\n";
        print($body);
    }
}

sub output_endifdef {
    my $cond = shift;

    if (defined($cond)) {
        print "# endif\n";
        $need_break = 1;
    }
}

# Generate string based macros for public consumption
sub generate_public_macros {
    my @macros = ();

    foreach my $name (keys %params) {
        my $val = $params{$name};
        my $def = '# define ' . $name . ' ';

        if (substr($val, 0, 1) eq '*') {
            $def .= substr($val, 1);
        } else {
            $def .= '"' . $val . '"';
        }
        push(@macros, $def)
    }
    return join("\n", sort @macros);
}

sub trie_matched {
  my $field = shift;
  my $num = shift;
  my $indent1 = shift;
  my $indent2 = shift;

  if ($field eq $invalid_param) {
    printf "%sERR_raise_data(ERR_LIB_PROV, ERR_R_UNSUPPORTED,\n", $indent1;
    printf "%s               \"param %%s is unsupported\", s);\n", $indent1;
    printf "%sreturn 0;\n", $indent1;
  } elsif (defined($num)) {
    printf "%sif (ossl_unlikely(r->num_%s >= %s)) {\n", $indent1, $field, $num;
    printf "%sERR_raise_data(ERR_LIB_PROV, PROV_R_TOO_MANY_RECORDS,\n", $indent2;
    printf "%s               \"param %%s present >%%d times\", s, $num);\n", $indent2;
    printf "%sreturn 0;\n", $indent2;
    printf "%s}\n", $indent1;
    printf "%sr->%s[r->num_%s++] = (OSSL_PARAM *)p;\n", $indent1, $field, $field;
  } else {
    printf "%sif (ossl_unlikely(r->%s != NULL)) {\n", $indent1, $field;
    printf "%sERR_raise_data(ERR_LIB_PROV, PROV_R_REPEATED_PARAMETER,\n", $indent2;
    printf "%s               \"param %%s is repeated\", s);\n", $indent2;
    printf "%sreturn 0;\n", $indent2;
    printf "%s}\n", $indent1;
    printf "%sr->%s = (OSSL_PARAM *)p;\n", $indent1, $field;
  }
}

sub generate_decoder_from_trie {
    my $n = shift;
    my $trieref = shift;
    my $identmap = shift;
    my $concat_num = shift;
    my $ifdefs = shift;
    my $idt = "    ";
    my $indent0 = $idt x ($n + 3);
    my $indent1 = $indent0 . $idt;
    my $indent2 = $indent1 . $idt;
    my $strcmp = $case_sensitive ? 'strcmp' : 'strcasecmp';
    my $field;

    if ($trieref->{'suffix'}) {
        my $suf = $trieref->{'suffix'};

        $field = $identmap->{$trieref->{'name'}};
        my $num = $concat_num->{$field};
        output_ifdef($ifdefs->{$field});
        printf "%sif (ossl_likely($strcmp(\"$suf\", s + $n) == 0", $indent0;
        if (not $case_sensitive) {
            $suf =~ tr/_/-/;
            print " || $strcmp(\"$suf\", s + $n) == 0"
                if ($suf ne $trieref->{'suffix'});
        }
        print ")) {\n";
        printf "%s/* %s */\n", $indent1, $trieref->{'name'};
        trie_matched($field, $num, $indent1, $indent2);
        printf "%s}\n", $indent0;

        # If this is at the top level and it's conditional, we have to
        # insert an empty statement in an else branch to avoid badness.
        # This isn't a problem at any other level since those are always
        # followed by a break statement.
        output_else($ifdefs->{$field}, $indent0 . ";\n") if ($n == 0);
        output_endifdef($ifdefs->{$field});
        return;
    }

    printf "%sswitch(s\[%d\]) {\n", $indent0, $n;
    printf "%sdefault:\n", $indent0;
    for my $l (sort keys %$trieref) {
        $need_break = 0;
        if ($l eq 'val') {
            $field = $identmap->{$trieref->{'val'}};
            my $num = $concat_num->{$field};
            printf "%sbreak;\n", $indent1;
            printf "%scase '\\0':\n", $indent0;
            output_ifdef($ifdefs->{$field});
            trie_matched($field, $num, $indent1, $indent2);
            output_endifdef($ifdefs->{$field});
        } else {
            printf "%sbreak;\n", $indent1;
            printf "%scase '%s':", $indent0, $l;
            if (not $case_sensitive) {
                print "   case '-':" if ($l eq '_');
                printf "   case '%s':", uc $l if ($l =~ /[a-z]/);
            }
            print "\n";
            generate_decoder_from_trie($n + 1, $trieref->{$l}, $identmap, $concat_num, $ifdefs);
        }
    }
    if ($need_break) {
        printf "%sbreak;\n", $indent1;
    }
    printf "%s}\n", $indent0;
    return;
}

sub generate_trie {
    my @keys = @_;
    my %trie;
    my $nodes = 0;
    my $chars = 0;

    foreach my $name (sort @keys) {
        my $val = $params{$name};
        die("Unknown parameter name '$name'\n") if !defined $val;
        while (substr($val, 0, 1) eq '*') {
            $val = $params{substr($val, 1)};
            die("Unknown referenced parameter from '$name'\n")
                if !defined $val;
        }
        my $cursor = \%trie;

        $chars += length($val);
        for my $i (0 .. length($val) - 1) {
            my $c = substr($val, $i, 1);

            if (not $case_sensitive) {
                $c = '_' if $c eq '-';
                $c = lc $c;
            }

            if (not defined $$cursor{$c}) {
                $cursor->{$c} = {};
                $nodes++;
            }
            $cursor = $cursor->{$c};
        }
        $cursor->{'val'} = $name;
    }
    return %trie;
}

# Find long endings and cache what they resolve to
sub locate_long_endings {
    my $trieref = shift;
    my @names = keys %$trieref;
    my $num = @names;

    return (1, '', $trieref->{$names[0]}) if ($num == 1 and $names[0] eq 'val');

    if ($num == 1) {
        my ($res, $suffix, $name) = locate_long_endings($trieref->{$names[0]});
        my $e = $names[0] . $suffix;
        if ($res) {
            $trieref->{'suffix'} = $e;
            $trieref->{'name'} = $name;
        }
        return $res, $e, $name;
    }

    for my $l (@names) {
        if ($l ne 'val') {
            my ($res, $suffix, $name) = locate_long_endings($trieref->{$l});
        }
    }
    return 0, '';
}

sub output_param_decoder {
    my $decoder_name_base = shift;
    my @params = @_;
    my @keys = ();
    my %prms = ();
    my %concat_num = ();
    my %ifdefs = ();

    print "/* Machine generated by util/perl/OpenSSL/paramnames.pm */\n";
    # Output ettable param array
    printf "#ifndef %s_list\n", $decoder_name_base;
    printf "static const OSSL_PARAM %s_list[] = {\n", $decoder_name_base;
    for (my $i = 0; $i <= $#params; $i++) {
        my $pname = $params[$i][0];
        my $pident = $params[$i][1];
        my $ptype = $params[$i][2];
        my $pnum = $params[$i][3];

        $prms{$pname} = $pident;

        if ($pident eq $invalid_param) {
            # Skip error cases in parameter list
            next;
        }
        if (defined $pnum) {
            if ($pnum eq 'hidden') {
                next;
            } elsif ($pnum eq 'fips') {
                # The `#if' is added on output
                $ifdefs{$pident} = ' defined(FIPS_MODULE)';
            } elsif ($pnum eq '!fips') {
                $ifdefs{$pident} = ' !defined(FIPS_MODULE)';
            } elsif (substr($pnum, 0, 3) eq '#if') {
                # Trim the `#if' from the front
                $ifdefs{$pident} = substr($pnum, 3);
            } elsif (not defined $concat_num{$pident}) {
                $concat_num{$pident} = $pnum;
            }
        }
        output_ifdef($ifdefs{$pident});
        print "    OSSL_PARAM_$ptype($pname, NULL";
        print ", 0" if $ptype eq "octet_string" || $ptype eq "octet_ptr"
                       || $ptype eq "utf8_string" || $ptype eq "utf8_ptr"
                       || $ptype eq "BN";
        printf "),\n";
        output_endifdef($ifdefs{$pident});
    }
    print "    OSSL_PARAM_END\n};\n#endif\n\n";

    # Output param pointer structure
    printf "#ifndef %s_st\n", $decoder_name_base;
    printf "struct %s_st {\n", $decoder_name_base;
    my %done_prms = ();
    foreach my $pident (sort values %prms) {
        if ($pident eq $invalid_param) {
            # Skip error cases in structure
            next;
        }
        if (not defined $done_prms{$pident}) {
            $done_prms{$pident} = 1;
            output_ifdef($ifdefs{$pident});
            if (defined($concat_num{$pident})) {
                printf "    OSSL_PARAM *%s[%s];\n", $pident, $concat_num{$pident};
                printf "    int num_%s;\n", $pident;
            } else {
                printf "    OSSL_PARAM *%s;\n", $pident;
            }

            # If this is the only field and it's conditional, we have to
            # insert a dummy field to avoid an empty struct
            output_else($ifdefs{$pident}, "    int dummy; /* unused */\n")
                if (keys(%prms) == 1);
            output_endifdef($ifdefs{$pident});
        }
    }
    print "};\n#endif\n\n";

    # Output param decoder
    my %t = generate_trie(keys(%prms));
    locate_long_endings(\%t);

    printf "#ifndef %s_decoder\n", $decoder_name_base;
    printf "static int %s_decoder\n", $decoder_name_base;
    printf "    (const OSSL_PARAM *p, struct %s_st *r)\n", $decoder_name_base;
    print "{\n";
    print "    const char *s;\n\n";
    print "    memset(r, 0, sizeof(*r));\n";
    print "    if (p != NULL)\n";
    print "        for (; (s = p->key) != NULL; p++)\n";
    generate_decoder_from_trie(0, \%t, \%prms, \%concat_num, \%ifdefs);
    print "    return 1;\n";
    print "}\n#endif\n";
    print "/* End of machine generated */";
}

sub produce_param_decoder {
    my $s;

    open(local *STDOUT, '>', \$s);
    output_param_decoder(@_);
    return $s;
}
