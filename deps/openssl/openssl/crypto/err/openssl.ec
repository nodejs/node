# crypto/err/openssl.ec

# configuration file for util/mkerr.pl

# files that may have to be rewritten by util/mkerr.pl
L ERR		NONE				NONE
L BN		crypto/bn/bn.h			crypto/bn/bn_err.c
L RSA		crypto/rsa/rsa.h		crypto/rsa/rsa_err.c
L DH		crypto/dh/dh.h			crypto/dh/dh_err.c
L EVP		crypto/evp/evp.h		crypto/evp/evp_err.c
L BUF		crypto/buffer/buffer.h		crypto/buffer/buf_err.c
L OBJ		crypto/objects/objects.h	crypto/objects/obj_err.c
L PEM		crypto/pem/pem.h		crypto/pem/pem_err.c
L DSA		crypto/dsa/dsa.h		crypto/dsa/dsa_err.c
L X509		crypto/x509/x509.h		crypto/x509/x509_err.c
L ASN1		crypto/asn1/asn1.h		crypto/asn1/asn1_err.c
L CONF		crypto/conf/conf.h		crypto/conf/conf_err.c
L CRYPTO	crypto/crypto.h			crypto/cpt_err.c
L EC		crypto/ec/ec.h			crypto/ec/ec_err.c
L SSL		ssl/ssl.h			ssl/ssl_err.c
L BIO		crypto/bio/bio.h		crypto/bio/bio_err.c
L PKCS7		crypto/pkcs7/pkcs7.h		crypto/pkcs7/pkcs7err.c
L X509V3	crypto/x509v3/x509v3.h		crypto/x509v3/v3err.c
L PKCS12	crypto/pkcs12/pkcs12.h		crypto/pkcs12/pk12err.c
L RAND		crypto/rand/rand.h		crypto/rand/rand_err.c
L DSO		crypto/dso/dso.h		crypto/dso/dso_err.c
L ENGINE	crypto/engine/engine.h		crypto/engine/eng_err.c
L OCSP		crypto/ocsp/ocsp.h		crypto/ocsp/ocsp_err.c
L UI		crypto/ui/ui.h			crypto/ui/ui_err.c
L COMP		crypto/comp/comp.h		crypto/comp/comp_err.c
L ECDSA		crypto/ecdsa/ecdsa.h		crypto/ecdsa/ecs_err.c
L ECDH		crypto/ecdh/ecdh.h		crypto/ecdh/ech_err.c
L STORE		crypto/store/store.h		crypto/store/str_err.c
L TS		crypto/ts/ts.h			crypto/ts/ts_err.c
L HMAC		crypto/hmac/hmac.h		crypto/hmac/hmac_err.c
L CMS		crypto/cms/cms.h		crypto/cms/cms_err.c
L JPAKE		crypto/jpake/jpake.h		crypto/jpake/jpake_err.c

# additional header files to be scanned for function names
L NONE		crypto/x509/x509_vfy.h		NONE
L NONE		crypto/ec/ec_lcl.h		NONE
L NONE		crypto/asn1/asn_lcl.h		NONE
L NONE		crypto/cms/cms_lcl.h		NONE


F RSAREF_F_RSA_BN2BIN
F RSAREF_F_RSA_PRIVATE_DECRYPT
F RSAREF_F_RSA_PRIVATE_ENCRYPT
F RSAREF_F_RSA_PUBLIC_DECRYPT
F RSAREF_F_RSA_PUBLIC_ENCRYPT
#F SSL_F_CLIENT_CERTIFICATE

R SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE		1010
R SSL_R_SSLV3_ALERT_BAD_RECORD_MAC		1020
R SSL_R_TLSV1_ALERT_DECRYPTION_FAILED		1021
R SSL_R_TLSV1_ALERT_RECORD_OVERFLOW		1022
R SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE	1030
R SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE		1040
R SSL_R_SSLV3_ALERT_NO_CERTIFICATE		1041
R SSL_R_SSLV3_ALERT_BAD_CERTIFICATE		1042
R SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE	1043
R SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED		1044
R SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED		1045
R SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN		1046
R SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER		1047
R SSL_R_TLSV1_ALERT_UNKNOWN_CA			1048
R SSL_R_TLSV1_ALERT_ACCESS_DENIED		1049
R SSL_R_TLSV1_ALERT_DECODE_ERROR		1050
R SSL_R_TLSV1_ALERT_DECRYPT_ERROR		1051
R SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION		1060
R SSL_R_TLSV1_ALERT_PROTOCOL_VERSION		1070
R SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY	1071
R SSL_R_TLSV1_ALERT_INTERNAL_ERROR		1080
R SSL_R_TLSV1_ALERT_USER_CANCELLED		1090
R SSL_R_TLSV1_ALERT_NO_RENEGOTIATION		1100
R SSL_R_TLSV1_UNSUPPORTED_EXTENSION		1110
R SSL_R_TLSV1_CERTIFICATE_UNOBTAINABLE		1111
R SSL_R_TLSV1_UNRECOGNIZED_NAME			1112
R SSL_R_TLSV1_BAD_CERTIFICATE_STATUS_RESPONSE	1113
R SSL_R_TLSV1_BAD_CERTIFICATE_HASH_VALUE	1114

R RSAREF_R_CONTENT_ENCODING			0x0400
R RSAREF_R_DATA					0x0401
R RSAREF_R_DIGEST_ALGORITHM			0x0402
R RSAREF_R_ENCODING				0x0403
R RSAREF_R_KEY					0x0404
R RSAREF_R_KEY_ENCODING				0x0405
R RSAREF_R_LEN					0x0406
R RSAREF_R_MODULUS_LEN				0x0407
R RSAREF_R_NEED_RANDOM				0x0408
R RSAREF_R_PRIVATE_KEY				0x0409
R RSAREF_R_PUBLIC_KEY				0x040a
R RSAREF_R_SIGNATURE				0x040b
R RSAREF_R_SIGNATURE_ENCODING			0x040c
R RSAREF_R_ENCRYPTION_ALGORITHM			0x040d

