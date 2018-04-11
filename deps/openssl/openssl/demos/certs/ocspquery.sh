# Example querying OpenSSL test responder. Assumes ocsprun.sh has been
# called.

OPENSSL=../../apps/openssl
OPENSSL_CONF=../../apps/openssl.cnf
export OPENSSL_CONF

# Send responder queries for each certificate.

echo "Requesting OCSP status for each certificate"
$OPENSSL ocsp -issuer intca.pem -cert client.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
$OPENSSL ocsp -issuer intca.pem -cert server.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
$OPENSSL ocsp -issuer intca.pem -cert rev.pem -CAfile root.pem \
			-url http://127.0.0.1:8888/
# One query for all three certificates.
echo "Requesting OCSP status for three certificates in one request"
$OPENSSL ocsp -issuer intca.pem \
	-cert client.pem -cert server.pem -cert rev.pem \
	-CAfile root.pem -url http://127.0.0.1:8888/
