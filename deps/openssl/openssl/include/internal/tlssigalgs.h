/*
 * Copyright 2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef OSSL_INTERNAL_TLSSIGALGS_H
#define OSSL_INTERNAL_TLSSIGALGS_H
#pragma once

/* Sigalgs values */
#define TLSEXT_SIGALG_ecdsa_secp256r1_sha256 0x0403
#define TLSEXT_SIGALG_ecdsa_secp384r1_sha384 0x0503
#define TLSEXT_SIGALG_ecdsa_secp521r1_sha512 0x0603
#define TLSEXT_SIGALG_ecdsa_sha224 0x0303
#define TLSEXT_SIGALG_ecdsa_sha1 0x0203
#define TLSEXT_SIGALG_rsa_pss_rsae_sha256 0x0804
#define TLSEXT_SIGALG_rsa_pss_rsae_sha384 0x0805
#define TLSEXT_SIGALG_rsa_pss_rsae_sha512 0x0806
#define TLSEXT_SIGALG_rsa_pss_pss_sha256 0x0809
#define TLSEXT_SIGALG_rsa_pss_pss_sha384 0x080a
#define TLSEXT_SIGALG_rsa_pss_pss_sha512 0x080b
#define TLSEXT_SIGALG_rsa_pkcs1_sha256 0x0401
#define TLSEXT_SIGALG_rsa_pkcs1_sha384 0x0501
#define TLSEXT_SIGALG_rsa_pkcs1_sha512 0x0601
#define TLSEXT_SIGALG_rsa_pkcs1_sha224 0x0301
#define TLSEXT_SIGALG_rsa_pkcs1_sha1 0x0201
#define TLSEXT_SIGALG_dsa_sha256 0x0402
#define TLSEXT_SIGALG_dsa_sha384 0x0502
#define TLSEXT_SIGALG_dsa_sha512 0x0602
#define TLSEXT_SIGALG_dsa_sha224 0x0302
#define TLSEXT_SIGALG_dsa_sha1 0x0202
#define TLSEXT_SIGALG_gostr34102012_256_intrinsic 0x0840
#define TLSEXT_SIGALG_gostr34102012_512_intrinsic 0x0841
#define TLSEXT_SIGALG_gostr34102012_256_gostr34112012_256 0xeeee
#define TLSEXT_SIGALG_gostr34102012_512_gostr34112012_512 0xefef
#define TLSEXT_SIGALG_gostr34102001_gostr3411 0xeded

#define TLSEXT_SIGALG_sm2sig_sm3 0x0708
#define TLSEXT_SIGALG_ed25519 0x0807
#define TLSEXT_SIGALG_ed448 0x0808
#define TLSEXT_SIGALG_ecdsa_brainpoolP256r1_sha256 0x081a
#define TLSEXT_SIGALG_ecdsa_brainpoolP384r1_sha384 0x081b
#define TLSEXT_SIGALG_ecdsa_brainpoolP512r1_sha512 0x081c
#define TLSEXT_SIGALG_mldsa44 0x0904
#define TLSEXT_SIGALG_mldsa65 0x0905
#define TLSEXT_SIGALG_mldsa87 0x0906

#define TLSEXT_SIGALG_slhdsa_sha2_128s 0x0911
#define TLSEXT_SIGALG_slhdsa_sha2_128f 0x0912
#define TLSEXT_SIGALG_slhdsa_sha2_192s 0x0913
#define TLSEXT_SIGALG_slhdsa_sha2_192f 0x0914
#define TLSEXT_SIGALG_slhdsa_sha2_256s 0x0915
#define TLSEXT_SIGALG_slhdsa_sha2_256f 0x0916
#define TLSEXT_SIGALG_slhdsa_shake_128s 0x0917
#define TLSEXT_SIGALG_slhdsa_shake_128f 0x0918
#define TLSEXT_SIGALG_slhdsa_shake_192s 0x0919
#define TLSEXT_SIGALG_slhdsa_shake_192f 0x091a
#define TLSEXT_SIGALG_slhdsa_shake_256s 0x091b
#define TLSEXT_SIGALG_slhdsa_shake_256f 0x091c

/* Sigalgs names */
#define TLSEXT_SIGALG_ecdsa_secp256r1_sha256_name "ecdsa_secp256r1_sha256"
#define TLSEXT_SIGALG_ecdsa_secp384r1_sha384_name "ecdsa_secp384r1_sha384"
#define TLSEXT_SIGALG_ecdsa_secp521r1_sha512_name "ecdsa_secp521r1_sha512"
#define TLSEXT_SIGALG_ecdsa_sha224_name "ecdsa_sha224"
#define TLSEXT_SIGALG_ecdsa_sha1_name "ecdsa_sha1"
#define TLSEXT_SIGALG_rsa_pss_rsae_sha256_name "rsa_pss_rsae_sha256"
#define TLSEXT_SIGALG_rsa_pss_rsae_sha384_name "rsa_pss_rsae_sha384"
#define TLSEXT_SIGALG_rsa_pss_rsae_sha512_name "rsa_pss_rsae_sha512"
#define TLSEXT_SIGALG_rsa_pss_pss_sha256_name "rsa_pss_pss_sha256"
#define TLSEXT_SIGALG_rsa_pss_pss_sha384_name "rsa_pss_pss_sha384"
#define TLSEXT_SIGALG_rsa_pss_pss_sha512_name "rsa_pss_pss_sha512"
#define TLSEXT_SIGALG_rsa_pkcs1_sha256_name "rsa_pkcs1_sha256"
#define TLSEXT_SIGALG_rsa_pkcs1_sha384_name "rsa_pkcs1_sha384"
#define TLSEXT_SIGALG_rsa_pkcs1_sha512_name "rsa_pkcs1_sha512"
#define TLSEXT_SIGALG_rsa_pkcs1_sha224_name "rsa_pkcs1_sha224"
#define TLSEXT_SIGALG_rsa_pkcs1_sha1_name "rsa_pkcs1_sha1"
#define TLSEXT_SIGALG_dsa_sha256_name "dsa_sha256"
#define TLSEXT_SIGALG_dsa_sha384_name "dsa_sha384"
#define TLSEXT_SIGALG_dsa_sha512_name "dsa_sha512"
#define TLSEXT_SIGALG_dsa_sha224_name "dsa_sha224"
#define TLSEXT_SIGALG_dsa_sha1_name "dsa_sha1"
#define TLSEXT_SIGALG_gostr34102012_256_intrinsic_name "gostr34102012_256"
#define TLSEXT_SIGALG_gostr34102012_512_intrinsic_name "gostr34102012_512"
#define TLSEXT_SIGALG_gostr34102012_256_intrinsic_alias "gost2012_256"
#define TLSEXT_SIGALG_gostr34102012_512_intrinsic_alias "gost2012_512"
#define TLSEXT_SIGALG_gostr34102012_256_gostr34112012_256_name "gost2012_256"
#define TLSEXT_SIGALG_gostr34102012_512_gostr34112012_512_name "gost2012_512"
#define TLSEXT_SIGALG_gostr34102001_gostr3411_name "gost2001_gost94"

#define TLSEXT_SIGALG_sm2sig_sm3_name "sm2sig_sm3"
#define TLSEXT_SIGALG_ed25519_name "ed25519"
#define TLSEXT_SIGALG_ed448_name "ed448"
#define TLSEXT_SIGALG_ecdsa_brainpoolP256r1_sha256_name "ecdsa_brainpoolP256r1tls13_sha256"
#define TLSEXT_SIGALG_ecdsa_brainpoolP384r1_sha384_name "ecdsa_brainpoolP384r1tls13_sha384"
#define TLSEXT_SIGALG_ecdsa_brainpoolP512r1_sha512_name "ecdsa_brainpoolP512r1tls13_sha512"
#define TLSEXT_SIGALG_ecdsa_brainpoolP256r1_sha256_alias "ecdsa_brainpoolP256r1_sha256"
#define TLSEXT_SIGALG_ecdsa_brainpoolP384r1_sha384_alias "ecdsa_brainpoolP384r1_sha384"
#define TLSEXT_SIGALG_ecdsa_brainpoolP512r1_sha512_alias "ecdsa_brainpoolP512r1_sha512"
#define TLSEXT_SIGALG_mldsa44_name "mldsa44"
#define TLSEXT_SIGALG_mldsa65_name "mldsa65"
#define TLSEXT_SIGALG_mldsa87_name "mldsa87"

#define TLSEXT_SIGALG_slhdsa_sha2_128s_name "slhdsa_sha2_128s"
#define TLSEXT_SIGALG_slhdsa_sha2_128f_name "slhdsa_sha2_128f"
#define TLSEXT_SIGALG_slhdsa_sha2_192s_name "slhdsa_sha2_192s"
#define TLSEXT_SIGALG_slhdsa_sha2_192f_name "slhdsa_sha2_192f"
#define TLSEXT_SIGALG_slhdsa_sha2_256s_name "slhdsa_sha2_256s"
#define TLSEXT_SIGALG_slhdsa_sha2_256f_name "slhdsa_sha2_256f"
#define TLSEXT_SIGALG_slhdsa_shake_128s_name "slhdsa_shake_128s"
#define TLSEXT_SIGALG_slhdsa_shake_128f_name "slhdsa_shake_128f"
#define TLSEXT_SIGALG_slhdsa_shake_192s_name "slhdsa_shake_192s"
#define TLSEXT_SIGALG_slhdsa_shake_192f_name "slhdsa_shake_192f"
#define TLSEXT_SIGALG_slhdsa_shake_256s_name "slhdsa_shake_256s"
#define TLSEXT_SIGALG_slhdsa_shake_256f_name "slhdsa_shake_256f"

#endif
