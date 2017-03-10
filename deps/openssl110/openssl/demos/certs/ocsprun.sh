# Example of running an querying OpenSSL test OCSP responder.
# This assumes "mkcerts.sh" or similar has been run to set up the
# necessary file structure.

OPENSSL=../../apps/openssl
OPENSSL_CONF=../../apps/openssl.cnf
export OPENSSL_CONF

# Run OCSP responder.

PORT=8888

$OPENSSL ocsp -port $PORT -index index.txt -CA intca.pem \
	-rsigner resp.pem -rkey respkey.pem -rother intca.pem $*
