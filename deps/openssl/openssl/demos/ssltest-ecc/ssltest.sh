#! /bin/sh
# Tests ECC cipher suites using ssltest. Requires one argument which could
# be aecdh or ecdh-ecdsa or ecdhe-ecdsa or ecdh-rsa or ecdhe-rsa.
# A second optional argument can be one of ssl2 ssl3 or tls1

if [ "$1" = "" ]; then
  (echo "Usage: $0 test [ protocol ]"
   echo "   where test is one of aecdh, ecdh-ecdsa, ecdhe-ecdsa, ecdh-rsa, ecdhe-rsa"
   echo "   and protocol (optional) is one of ssl2, ssl3, tls1"
   echo "Run RSAcertgen.sh, ECC-RSAcertgen.sh, ECCcertgen.sh first."
  ) >&2
  exit 1
fi


OPENSSL_DIR=../..
CERTS_DIR=./Certs
SSLTEST=$OPENSSL_DIR/test/ssltest
# SSL protocol version to test (one of ssl2 ssl3 or tls1)"
SSLVERSION=

# These don't really require any certificates
AECDH_CIPHER_LIST="AECDH-AES256-SHA AECDH-AES128-SHA AECDH-DES-CBC3-SHA AECDH-RC4-SHA AECDH-NULL-SHA"

# These require ECC certificates signed with ECDSA
# The EC public key must be authorized for key agreement.
ECDH_ECDSA_CIPHER_LIST="ECDH-ECDSA-AES256-SHA ECDH-ECDSA-AES128-SHA ECDH-ECDSA-DES-CBC3-SHA ECDH-ECDSA-RC4-SHA ECDH-ECDSA-NULL-SHA"

# These require ECC certificates.
# The EC public key must be authorized for digital signature.
ECDHE_ECDSA_CIPHER_LIST="ECDHE-ECDSA-AES256-SHA ECDHE-ECDSA-AES128-SHA ECDHE-ECDSA-DES-CBC3-SHA ECDHE-ECDSA-RC4-SHA ECDHE-ECDSA-NULL-SHA"

# These require ECC certificates signed with RSA.
# The EC public key must be authorized for key agreement.
ECDH_RSA_CIPHER_LIST="ECDH-RSA-AES256-SHA ECDH-RSA-AES128-SHA ECDH-RSA-DES-CBC3-SHA ECDH-RSA-RC4-SHA ECDH-RSA-NULL-SHA"

# These require RSA certificates.
# The RSA public key must be authorized for digital signature.
ECDHE_RSA_CIPHER_LIST="ECDHE-RSA-AES256-SHA ECDHE-RSA-AES128-SHA ECDHE-RSA-DES-CBC3-SHA ECDHE-RSA-RC4-SHA ECDHE-RSA-NULL-SHA"

# List of Elliptic curves over which we wish to test generation of
# ephemeral ECDH keys when using AECDH or ECDHE ciphers
# NOTE: secp192r1 = prime192v1 and secp256r1 = prime256v1
#ELLIPTIC_CURVE_LIST="secp112r1 sect113r2 secp128r1 sect131r1 secp160k1 sect163r2 wap-wsg-idm-ecid-wtls7 c2pnb163v3 c2pnb176v3 c2tnb191v3 secp192r1 prime192v3 sect193r2 secp224r1 wap-wsg-idm-ecid-wtls10 sect239k1 prime239v2 secp256r1 prime256v1 sect283k1 secp384r1 sect409r1 secp521r1 sect571r1"
ELLIPTIC_CURVE_LIST="sect163k1 sect163r1 sect163r2 sect193r1 sect193r2 sect233k1 sect233r1 sect239k1 sect283k1 sect283r1 sect409k1 sect409r1 sect571k1 sect571r1 secp160k1 secp160r1 secp160r2 secp192k1 prime192v1 secp224k1 secp224r1 secp256k1 prime256v1 secp384r1 secp521r1"

DEFAULT_CURVE="sect163r2"

if [ "$2" = "" ]; then
    if [ "$SSL_VERSION" = "" ]; then
	SSL_VERSION=""
    else
	SSL_VERSION="-$SSL_VERSION"
    fi
else
    SSL_VERSION="-$2"
fi

#==============================================================
# Anonymous cipher suites do not require key or certificate files
# but ssltest expects a cert file and complains if it can't
# open the default one.
SERVER_PEM=$OPENSSL_DIR/apps/server.pem

if [ "$1" = "aecdh" ]; then
for cipher in $AECDH_CIPHER_LIST
do
    echo "Testing $cipher"
    $SSLTEST $SSL_VERSION -cert $SERVER_PEM -cipher $cipher 
done
#--------------------------------------------------------------
for curve in $ELLIPTIC_CURVE_LIST
do
    echo "Testing AECDH-NULL-SHA (with $curve)"
    $SSLTEST $SSL_VERSION -cert $SERVER_PEM \
	-named_curve $curve -cipher AECDH-NULL-SHA
done

for curve in $ELLIPTIC_CURVE_LIST
do
    echo "Testing AECDH-RC4-SHA (with $curve)"
    $SSLTEST $SSL_VERSION -cert $SERVER_PEM \
	-named_curve $curve -cipher AECDH-RC4-SHA
done
fi

#==============================================================
# Both ECDH-ECDSA and ECDHE-ECDSA cipher suites require 
# the server to have an ECC certificate signed with ECDSA.
CA_PEM=$CERTS_DIR/secp160r1TestCA.pem
SERVER_PEM=$CERTS_DIR/secp160r2TestServer.pem
CLIENT_PEM=$CERTS_DIR/secp160r2TestClient.pem

if [ "$1" = "ecdh-ecdsa" ]; then
for cipher in $ECDH_ECDSA_CIPHER_LIST
do
    echo "Testing $cipher (with server authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-cipher $cipher

    echo "Testing $cipher (with server and client authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-c_cert $CLIENT_PEM -client_auth \
	-cipher $cipher
done
fi

#==============================================================
if [ "$1" = "ecdhe-ecdsa" ]; then
for cipher in $ECDHE_ECDSA_CIPHER_LIST
do
    echo "Testing $cipher (with server authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-cipher $cipher -named_curve $DEFAULT_CURVE

    echo "Testing $cipher (with server and client authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-c_cert $CLIENT_PEM -client_auth \
	-cipher $cipher -named_curve $DEFAULT_CURVE
done

#--------------------------------------------------------------
for curve in $ELLIPTIC_CURVE_LIST
do
    echo "Testing ECDHE-ECDSA-AES128-SHA (2-way auth with $curve)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-c_cert $CLIENT_PEM -client_auth \
	-cipher ECDHE-ECDSA-AES128-SHA -named_curve $curve 
done
fi

#==============================================================
# ECDH-RSA cipher suites require the server to have an ECC
# certificate signed with RSA.
CA_PEM=$CERTS_DIR/rsa1024TestCA.pem
SERVER_PEM=$CERTS_DIR/sect163r1-rsaTestServer.pem
CLIENT_PEM=$CERTS_DIR/sect163r1-rsaTestClient.pem

if [ "$1" = "ecdh-rsa" ]; then
for cipher in $ECDH_RSA_CIPHER_LIST
do
    echo "Testing $cipher (with server authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-cipher $cipher

    echo "Testing $cipher (with server and client authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-c_cert $CLIENT_PEM -client_auth \
	-cipher $cipher
done
fi

#==============================================================
# ECDHE-RSA cipher suites require the server to have an RSA cert.
CA_PEM=$CERTS_DIR/rsa1024TestCA.pem
SERVER_PEM=$CERTS_DIR/rsa1024TestServer.pem
CLIENT_PEM=$CERTS_DIR/rsa1024TestClient.pem

if [ "$1" = "ecdhe-rsa" ]; then
for cipher in $ECDHE_RSA_CIPHER_LIST
do
    echo "Testing $cipher (with server authentication)"
    echo $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-cipher $cipher -named_curve $DEFAULT_CURVE
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-cipher $cipher -named_curve $DEFAULT_CURVE

    echo "Testing $cipher (with server and client authentication)"
    $SSLTEST $SSL_VERSION -CAfile $CA_PEM \
	-cert $SERVER_PEM -server_auth \
	-c_cert $CLIENT_PEM -client_auth \
	-cipher $cipher -named_curve $DEFAULT_CURVE
done
fi
#==============================================================




