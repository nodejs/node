# configuration file for util/mkerr.pl

# The INPUT HEADER is scanned for declarations
# LIBNAME       INPUT HEADER                    ERROR-TABLE FILE
L ERR           NONE                            NONE
L BN            include/openssl/bn.h            crypto/bn/bn_err.c
L RSA           include/openssl/rsa.h           crypto/rsa/rsa_err.c
L DH            include/openssl/dh.h            crypto/dh/dh_err.c
L EVP           include/openssl/evp.h           crypto/evp/evp_err.c
L BUF           include/openssl/buffer.h        crypto/buffer/buf_err.c
L OBJ           include/openssl/objects.h       crypto/objects/obj_err.c
L PEM           include/openssl/pem.h           crypto/pem/pem_err.c
L DSA           include/openssl/dsa.h           crypto/dsa/dsa_err.c
L X509          include/openssl/x509.h          crypto/x509/x509_err.c
L ASN1          include/openssl/asn1.h          crypto/asn1/asn1_err.c
L CONF          include/openssl/conf.h          crypto/conf/conf_err.c
L CRYPTO        include/openssl/crypto.h        crypto/cpt_err.c
L EC            include/openssl/ec.h            crypto/ec/ec_err.c
L SSL           include/openssl/ssl.h           ssl/ssl_err.c
L BIO           include/openssl/bio.h           crypto/bio/bio_err.c
L PKCS7         include/openssl/pkcs7.h         crypto/pkcs7/pkcs7err.c
L X509V3        include/openssl/x509v3.h        crypto/x509v3/v3err.c
L PKCS12        include/openssl/pkcs12.h        crypto/pkcs12/pk12err.c
L RAND          include/openssl/rand.h          crypto/rand/rand_err.c
L DSO           include/internal/dso.h          crypto/dso/dso_err.c
L ENGINE        include/openssl/engine.h        crypto/engine/eng_err.c
L OCSP          include/openssl/ocsp.h          crypto/ocsp/ocsp_err.c
L UI            include/openssl/ui.h            crypto/ui/ui_err.c
L COMP          include/openssl/comp.h          crypto/comp/comp_err.c
L TS            include/openssl/ts.h            crypto/ts/ts_err.c
L CMS           include/openssl/cms.h           crypto/cms/cms_err.c
L CT            include/openssl/ct.h            crypto/ct/ct_err.c
L ASYNC         include/openssl/async.h         crypto/async/async_err.c
L KDF           include/openssl/kdf.h           crypto/kdf/kdf_err.c
L SM2           crypto/include/internal/sm2.h   crypto/sm2/sm2_err.c
L OSSL_STORE    include/openssl/store.h         crypto/store/store_err.c

# additional header files to be scanned for function names
L NONE          include/openssl/x509_vfy.h      NONE
L NONE          crypto/ec/ec_lcl.h              NONE
L NONE          crypto/cms/cms_lcl.h            NONE
L NONE          crypto/ct/ct_locl.h             NONE
L NONE          ssl/ssl_locl.h                  NONE

# SSL/TLS alerts
R SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE          1010
R SSL_R_SSLV3_ALERT_BAD_RECORD_MAC              1020
R SSL_R_TLSV1_ALERT_DECRYPTION_FAILED           1021
R SSL_R_TLSV1_ALERT_RECORD_OVERFLOW             1022
R SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE       1030
R SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE           1040
R SSL_R_SSLV3_ALERT_NO_CERTIFICATE              1041
R SSL_R_SSLV3_ALERT_BAD_CERTIFICATE             1042
R SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE     1043
R SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED         1044
R SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED         1045
R SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN         1046
R SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER           1047
R SSL_R_TLSV1_ALERT_UNKNOWN_CA                  1048
R SSL_R_TLSV1_ALERT_ACCESS_DENIED               1049
R SSL_R_TLSV1_ALERT_DECODE_ERROR                1050
R SSL_R_TLSV1_ALERT_DECRYPT_ERROR               1051
R SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION          1060
R SSL_R_TLSV1_ALERT_PROTOCOL_VERSION            1070
R SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY       1071
R SSL_R_TLSV1_ALERT_INTERNAL_ERROR              1080
R SSL_R_TLSV1_ALERT_INAPPROPRIATE_FALLBACK      1086
R SSL_R_TLSV1_ALERT_USER_CANCELLED              1090
R SSL_R_TLSV1_ALERT_NO_RENEGOTIATION            1100
R SSL_R_TLSV13_ALERT_MISSING_EXTENSION          1109
R SSL_R_TLSV1_UNSUPPORTED_EXTENSION             1110
R SSL_R_TLSV1_CERTIFICATE_UNOBTAINABLE          1111
R SSL_R_TLSV1_UNRECOGNIZED_NAME                 1112
R SSL_R_TLSV1_BAD_CERTIFICATE_STATUS_RESPONSE   1113
R SSL_R_TLSV1_BAD_CERTIFICATE_HASH_VALUE        1114
R TLS1_AD_UNKNOWN_PSK_IDENTITY                  1115
R SSL_R_TLSV13_ALERT_CERTIFICATE_REQUIRED       1116
R TLS1_AD_NO_APPLICATION_PROTOCOL               1120
