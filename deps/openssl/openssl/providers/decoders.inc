/*
 * Copyright 2020-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifndef DECODER_PROVIDER
# error Macro DECODER_PROVIDER undefined
#endif

#define DECODER_STRUCTURE_type_specific_keypair         "type-specific"
#define DECODER_STRUCTURE_type_specific_params          "type-specific"
#define DECODER_STRUCTURE_type_specific                 "type-specific"
#define DECODER_STRUCTURE_type_specific_no_pub          "type-specific"
#define DECODER_STRUCTURE_EncryptedPrivateKeyInfo       "EncryptedPrivateKeyInfo"
#define DECODER_STRUCTURE_PrivateKeyInfo                "PrivateKeyInfo"
#define DECODER_STRUCTURE_SubjectPublicKeyInfo          "SubjectPublicKeyInfo"
#define DECODER_STRUCTURE_DH                            "dh"
#define DECODER_STRUCTURE_DHX                           "dhx"
#define DECODER_STRUCTURE_DSA                           "dsa"
#define DECODER_STRUCTURE_EC                            "ec"
#define DECODER_STRUCTURE_RSA                           "rsa"

/* Arguments are prefixed with '_' to avoid build breaks on certain platforms */
#define DECODER(_name, _input, _output, _fips, desc)                    \
    { _name,                                                            \
      "provider=" DECODER_PROVIDER ",fips=" #_fips ",input=" #_input,   \
      (ossl_##_input##_to_##_output##_decoder_functions), desc }
#define DECODER_w_structure(_name, _input, _structure, _output, _fips, desc)  \
    { _name,                                                            \
      "provider=" DECODER_PROVIDER ",fips=" #_fips ",input=" #_input    \
      ",structure=" DECODER_STRUCTURE_##_structure,                     \
      (ossl_##_structure##_##_input##_to_##_output##_decoder_functions),\
      desc }

#ifndef OPENSSL_NO_DH
DECODER_w_structure("DH", der, PrivateKeyInfo, dh, yes, "PKItoDH-DER"),
DECODER_w_structure("DH", der, SubjectPublicKeyInfo, dh, yes, "SPKItoDH-DER"),
DECODER_w_structure("DH", der, type_specific_params, dh, yes, "TStoDH-DER"),
DECODER_w_structure("DH", der, DH, dh, yes, "DERtoDH"),
DECODER_w_structure("DHX", der, PrivateKeyInfo, dhx, yes,"PKItoDHX-DER"),
DECODER_w_structure("DHX", der, SubjectPublicKeyInfo, dhx, yes, "SPKItoDHX-DER"),
DECODER_w_structure("DHX", der, type_specific_params, dhx, yes, "TStoDHX-DER"),
DECODER_w_structure("DHX", der, DHX, dhx, yes, "DERtoDHX"),
#endif
#ifndef OPENSSL_NO_DSA
DECODER_w_structure("DSA", der, PrivateKeyInfo, dsa, yes, "PKItoDSA-DER"),
DECODER_w_structure("DSA", der, SubjectPublicKeyInfo, dsa, yes, "SPKItoDSA-DER"),
DECODER_w_structure("DSA", der, type_specific, dsa, yes, "TStoDSA-DER"),
DECODER_w_structure("DSA", der, DSA, dsa, yes, "DERtoDSA"),
DECODER("DSA", msblob, dsa, yes, "MSBLOBtoDSA"),
DECODER("DSA", pvk, dsa, yes, "PVKtoDSA"),
#endif
#ifndef OPENSSL_NO_EC
DECODER_w_structure("EC", der, PrivateKeyInfo, ec, yes, "PKItoEC-DER"),
DECODER_w_structure("EC", der, SubjectPublicKeyInfo, ec, yes, "SPKItoEC-DER"),
DECODER_w_structure("EC", der, type_specific_no_pub, ec, yes, "TStoEC-DER"),
DECODER_w_structure("EC", der, EC, ec, yes, "DERtoEC"),
# ifndef OPENSSL_NO_ECX
DECODER_w_structure("ED25519", der, PrivateKeyInfo, ed25519, yes, "PKItoED25519-DER"),
DECODER_w_structure("ED25519", der, SubjectPublicKeyInfo, ed25519, yes, "SPKItoED25519-DER"),
DECODER_w_structure("ED448", der, PrivateKeyInfo, ed448, yes, "PKItoED448-DER"),
DECODER_w_structure("ED448", der, SubjectPublicKeyInfo, ed448, yes, "SPKItoED448-DER"),
DECODER_w_structure("X25519", der, PrivateKeyInfo, x25519, yes, "PKItoX25519-DER"),
DECODER_w_structure("X25519", der, SubjectPublicKeyInfo, x25519, yes, "SPKItoX25519-DER"),
DECODER_w_structure("X448", der, PrivateKeyInfo, x448, yes, "PKItoX448-DER"),
DECODER_w_structure("X448", der, SubjectPublicKeyInfo, x448, yes, "SPKItoX448-DER"),
# endif
# ifndef OPENSSL_NO_SM2
DECODER_w_structure("SM2", der, PrivateKeyInfo, sm2, no, "PKItoSM2-DER"),
DECODER_w_structure("SM2", der, SubjectPublicKeyInfo, sm2, no, "SPKItoSM2-DER"),
DECODER_w_structure("SM2", der, type_specific_no_pub, sm2, no, "TStoSM2-DER"),
# endif
#endif
#ifndef OPENSSL_NO_ML_KEM
DECODER_w_structure("ML-KEM-512", der, PrivateKeyInfo, ml_kem_512, yes, "PKItoML-KEM-512-DER"),
DECODER_w_structure("ML-KEM-512", der, SubjectPublicKeyInfo, ml_kem_512, yes, "SPKItoML-KEM-512-DER"),
DECODER_w_structure("ML-KEM-768", der, PrivateKeyInfo, ml_kem_768, yes, "PKItoML-KEM-768-DER"),
DECODER_w_structure("ML-KEM-768", der, SubjectPublicKeyInfo, ml_kem_768, yes, "SPKItoML-KEM-768-DER"),
DECODER_w_structure("ML-KEM-1024", der, PrivateKeyInfo, ml_kem_1024, yes, "PKItoML-KEM-1024-DER"),
DECODER_w_structure("ML-KEM-1024", der, SubjectPublicKeyInfo, ml_kem_1024, yes, "SPKItoML-KEM-1024-DER"),
#endif
#ifndef OPENSSL_NO_SLH_DSA
DECODER_w_structure( "SLH-DSA-SHA2-128s", der, PrivateKeyInfo, slh_dsa_sha2_128s, yes, "PKItoSLH-DSA-SHA2-128s"),
DECODER_w_structure( "SLH-DSA-SHA2-128f", der, PrivateKeyInfo, slh_dsa_sha2_128f, yes, "PKItoSLH-DSA-SHA2-128f"),
DECODER_w_structure( "SLH-DSA-SHA2-192s", der, PrivateKeyInfo, slh_dsa_sha2_192s, yes, "PKItoSLH-DSA-SHA2-192s"),
DECODER_w_structure( "SLH-DSA-SHA2-192f", der, PrivateKeyInfo, slh_dsa_sha2_192f, yes, "PKItoSLH-DSA-SHA2-192f"),
DECODER_w_structure( "SLH-DSA-SHA2-256s", der, PrivateKeyInfo, slh_dsa_sha2_256s, yes, "PKItoSLH-DSA-SHA2-256s"),
DECODER_w_structure( "SLH-DSA-SHA2-256f", der, PrivateKeyInfo, slh_dsa_sha2_256f, yes, "PKItoSLH-DSA-SHA2-256f"),
DECODER_w_structure("SLH-DSA-SHAKE-128s", der, PrivateKeyInfo, slh_dsa_shake_128s, yes, "PKItoSLH-DSA-SHAKE-128s"),
DECODER_w_structure("SLH-DSA-SHAKE-128f", der, PrivateKeyInfo, slh_dsa_shake_128f, yes, "PKItoSLH-DSA-SHAKE-128f"),
DECODER_w_structure("SLH-DSA-SHAKE-192s", der, PrivateKeyInfo, slh_dsa_shake_192s, yes, "PKItoSLH-DSA-SHAKE-192s"),
DECODER_w_structure("SLH-DSA-SHAKE-192f", der, PrivateKeyInfo, slh_dsa_shake_192f, yes, "PKItoSLH-DSA-SHAKE-192f"),
DECODER_w_structure("SLH-DSA-SHAKE-256s", der, PrivateKeyInfo, slh_dsa_shake_256s, yes, "PKItoSLH-DSA-SHAKE-256s"),
DECODER_w_structure("SLH-DSA-SHAKE-256f", der, PrivateKeyInfo, slh_dsa_shake_256f, yes, "PKItoSLH-DSA-SHAKE-256f"),
DECODER_w_structure( "SLH-DSA-SHA2-128s", der, SubjectPublicKeyInfo, slh_dsa_sha2_128s, yes, "SPKItoSLH-DSA-SHA2-128s"),
DECODER_w_structure( "SLH-DSA-SHA2-128f", der, SubjectPublicKeyInfo, slh_dsa_sha2_128f, yes, "SPKItoSLH-DSA-SHA2-128f"),
DECODER_w_structure( "SLH-DSA-SHA2-192s", der, SubjectPublicKeyInfo, slh_dsa_sha2_192s, yes, "SPKItoSLH-DSA-SHA2-192s"),
DECODER_w_structure( "SLH-DSA-SHA2-192f", der, SubjectPublicKeyInfo, slh_dsa_sha2_192f, yes, "SPKItoSLH-DSA-SHA2-192f"),
DECODER_w_structure( "SLH-DSA-SHA2-256s", der, SubjectPublicKeyInfo, slh_dsa_sha2_256s, yes, "SPKItoSLH-DSA-SHA2-256s"),
DECODER_w_structure( "SLH-DSA-SHA2-256f", der, SubjectPublicKeyInfo, slh_dsa_sha2_256f, yes, "SPKItoSLH-DSA-SHA2-256f"),
DECODER_w_structure("SLH-DSA-SHAKE-128s", der, SubjectPublicKeyInfo, slh_dsa_shake_128s, yes, "SPKItoSLH-DSA-SHAKE-128s"),
DECODER_w_structure("SLH-DSA-SHAKE-128f", der, SubjectPublicKeyInfo, slh_dsa_shake_128f, yes, "SPKItoSLH-DSA-SHAKE-128f"),
DECODER_w_structure("SLH-DSA-SHAKE-192s", der, SubjectPublicKeyInfo, slh_dsa_shake_192s, yes, "SPKItoSLH-DSA-SHAKE-192s"),
DECODER_w_structure("SLH-DSA-SHAKE-192f", der, SubjectPublicKeyInfo, slh_dsa_shake_192f, yes, "SPKItoSLH-DSA-SHAKE-192f"),
DECODER_w_structure("SLH-DSA-SHAKE-256s", der, SubjectPublicKeyInfo, slh_dsa_shake_256s, yes, "SPKItoSLH-DSA-SHAKE-256s"),
DECODER_w_structure("SLH-DSA-SHAKE-256f", der, SubjectPublicKeyInfo, slh_dsa_shake_256f, yes, "SPKItoSLH-DSA-SHAKE-256f"),
#endif /* OPENSSL_NO_SLH_DSA */
DECODER_w_structure("RSA", der, PrivateKeyInfo, rsa, yes, "PKItoRSA-DER"),
DECODER_w_structure("RSA", der, SubjectPublicKeyInfo, rsa, yes, "SPKItoRSA-DER"),
DECODER_w_structure("RSA", der, type_specific_keypair, rsa, yes, "TStoKEYPAIRtoRSA-DER"),
DECODER_w_structure("RSA", der, RSA, rsa, yes, "DERtoRSA"),
DECODER_w_structure("RSA-PSS", der, PrivateKeyInfo, rsapss, yes, "PKItoRSA-PSS-DER"),
DECODER_w_structure("RSA-PSS", der, SubjectPublicKeyInfo, rsapss, yes, "SPKItoRSA-PSS-DER"),
DECODER("RSA", msblob, rsa, yes, "MSBLOBtoRSA"),
DECODER("RSA", pvk, rsa, yes, "PVKtoRSA"),
#ifndef OPENSSL_NO_ML_DSA
DECODER_w_structure("ML-DSA-44", der, PrivateKeyInfo, ml_dsa_44, yes, "PKItoML-DSA-44-DER"),
DECODER_w_structure("ML-DSA-65", der, PrivateKeyInfo, ml_dsa_65, yes, "PKItoML-DSA-65-DER"),
DECODER_w_structure("ML-DSA-87", der, PrivateKeyInfo, ml_dsa_87, yes, "PKItoML-DSA-87-DER"),
DECODER_w_structure("ML-DSA-44", der, SubjectPublicKeyInfo, ml_dsa_44, yes, "SPKItoML-DSA-44-DER"),
DECODER_w_structure("ML-DSA-65", der, SubjectPublicKeyInfo, ml_dsa_65, yes, "SPKItoML-DSA-65-DER"),
DECODER_w_structure("ML-DSA-87", der, SubjectPublicKeyInfo, ml_dsa_87, yes, "SPKItoML-DSA-87-DER"),
#endif /* OPENSSL_NO_ML_DSA */

#ifndef OPENSSL_NO_LMS
DECODER("LMS", xdr, lms, yes, "XDRtoLMS"),
DECODER_w_structure("LMS", der, SubjectPublicKeyInfo, lms, yes, "SPKItoLMS-DER"),
#endif

/*
 * A decoder that takes a SubjectPublicKeyInfo and figures out the types of key
 * that it contains. The output is the same SubjectPublicKeyInfo.
 */
DECODER_w_structure("DER", der, SubjectPublicKeyInfo, der, yes, "SPKItoDER"),
/*
 * General-purpose PEM to DER decoder.  When the user-specified data structure
 * is a possibly encrypted PKCS#8 PrivateKeyInfo or a SubjectPublicKeyInfo
 * public key, or interest in such a key is indicated via the "selection", and
 * the current object is of that type, decodes the PKCS#8 or SPKI identifying
 * algorithm name or OID, and delegates further decoding in DER form to the
 * identified algorithm.
 */
DECODER("DER", pem, der, yes, "PEMtoDER"),
/*
 * A decoder that recognises PKCS#8 EncryptedPrivateKeyInfo structure and
 * decrypts it, obtaining the algorithm name or OID, and delegates the
 * unencrypted PrivateKeyInfo in DER form to the identified algorithm.
 */
DECODER_w_structure("DER", der, EncryptedPrivateKeyInfo, der, yes, "EPKItoDER"),
