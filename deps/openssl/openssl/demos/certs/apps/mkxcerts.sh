
# Create certificates using various algorithms to test multi-certificate
# functionality.

OPENSSL=../../../apps/openssl
CN="OpenSSL Test RSA SHA-1 cert" $OPENSSL req \
	-config apps.cnf -extensions usr_cert -x509 -nodes \
	-keyout tsha1.pem -out tsha1.pem -new -days 3650 -sha1
CN="OpenSSL Test RSA SHA-256 cert" $OPENSSL req \
	-config apps.cnf -extensions usr_cert -x509 -nodes \
	-keyout tsha256.pem -out tsha256.pem -new -days 3650 -sha256
CN="OpenSSL Test RSA SHA-512 cert" $OPENSSL req \
	-config apps.cnf -extensions usr_cert -x509 -nodes \
	-keyout tsha512.pem -out tsha512.pem -new -days 3650 -sha512

# Create EC parameters 

$OPENSSL ecparam -name P-256 -out ecp256.pem
$OPENSSL ecparam -name P-384 -out ecp384.pem

CN="OpenSSL Test P-256 SHA-256 cert" $OPENSSL req \
	-config apps.cnf -extensions ec_cert -x509 -nodes \
	-nodes -keyout tecp256.pem -out tecp256.pem -newkey ec:ecp256.pem \
	-days 3650 -sha256

CN="OpenSSL Test P-384 SHA-384 cert" $OPENSSL req \
	-config apps.cnf -extensions ec_cert -x509 -nodes \
	-nodes -keyout tecp384.pem -out tecp384.pem -newkey ec:ecp384.pem \
	-days 3650 -sha384
