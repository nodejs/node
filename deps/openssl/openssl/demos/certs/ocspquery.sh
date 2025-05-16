#!/bin/sh

# Example querying OpenSSL test responder. Assumes ocsprun.sh has been
# called.

opensslcmd() {
    LD_LIBRARY_PATH=../.. ../../apps/openssl $@
}

OPENSSL_CONF=../../apps/openssl.cnf
export OPENSSL_CONF

opensslcmd version

# Send responder queries for each certificate.

echo "Requesting OCSP status for each certificate"
opensslcmd ocsp -issuer intca.pem -cert client.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
opensslcmd ocsp -issuer intca.pem -cert server.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
opensslcmd ocsp -issuer intca.pem -cert rev.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
# One query for all three certificates.
echo "Requesting OCSP status for three certificates in one request"
opensslcmd ocsp -issuer intca.pem \
	-cert client.pem -cert server.pem -cert rev.pem \
	-CAfile root.pem -url http://127.0.0.1:8888/
