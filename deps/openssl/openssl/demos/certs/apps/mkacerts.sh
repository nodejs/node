#!/bin/sh

# Recreate the demo certificates in the apps directory.

opensslcmd() {
    LD_LIBRARY_PATH=../../.. ../../../apps/openssl $@
}

opensslcmd version

# Root CA: create certificate directly
CN="OpenSSL Test Root CA" opensslcmd req -config apps.cnf -x509 -nodes \
	-keyout root.pem -out root.pem -key rootkey.pem -new -days 3650
# Intermediate CA: request first
CN="OpenSSL Test Intermediate CA" opensslcmd req -config apps.cnf -nodes \
	-key intkey.pem -out intreq.pem -new
# Sign request: CA extensions
opensslcmd x509 -req -in intreq.pem -CA root.pem -CAkey rootkey.pem -days 3630 \
	-extfile apps.cnf -extensions v3_ca -CAcreateserial -out intca.pem
# Client certificate: request first
CN="Test Client Cert" opensslcmd req -config apps.cnf -nodes \
	-key ckey.pem -out creq.pem -new
# Sign using intermediate CA
opensslcmd x509 -req -in creq.pem -CA intca.pem -CAkey intkey.pem -days 3600 \
	-extfile apps.cnf -extensions usr_cert -CAcreateserial | \
	opensslcmd x509 -nameopt oneline -subject -issuer >client.pem
# Server certificate: request first
CN="Test Server Cert" opensslcmd req -config apps.cnf -nodes \
	-key skey.pem -out sreq.pem -new
# Sign using intermediate CA
opensslcmd x509 -req -in sreq.pem -CA intca.pem -CAkey intkey.pem -days 3600 \
	-extfile apps.cnf -extensions usr_cert -CAcreateserial | \
	opensslcmd x509 -nameopt oneline -subject -issuer >server.pem
# Server certificate #2: request first
CN="Test Server Cert #2" opensslcmd req -config apps.cnf -nodes \
	-key skey2.pem -out sreq2.pem -new
# Sign using intermediate CA
opensslcmd x509 -req -in sreq2.pem -CA intca.pem -CAkey intkey.pem -days 3600 \
	-extfile apps.cnf -extensions usr_cert -CAcreateserial | \
	opensslcmd x509 -nameopt oneline -subject -issuer >server2.pem

# Append keys to file.

cat skey.pem >>server.pem
cat skey2.pem >>server2.pem
cat ckey.pem >>client.pem

opensslcmd verify -CAfile root.pem -untrusted intca.pem \
				server2.pem server.pem client.pem
