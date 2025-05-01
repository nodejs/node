#!/bin/sh

opensslcmd() {
    LD_LIBRARY_PATH=../.. ../../apps/openssl $@
}

# Example of running an querying OpenSSL test OCSP responder.
# This assumes "mkcerts.sh" or similar has been run to set up the
# necessary file structure.

OPENSSL_CONF=../../apps/openssl.cnf
export OPENSSL_CONF

opensslcmd version

# Run OCSP responder.

PORT=8888

opensslcmd ocsp -port $PORT -index index.txt -CA intca.pem \
	-rsigner resp.pem -rkey respkey.pem -rother intca.pem $*
