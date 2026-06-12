#!/bin/bash

# Test openssl CA functionality using oqsprovider for alg $1

if [ $# -ne 1 ]; then
    echo "Usage: $0 <algorithmname>. Exiting."
    exit 1
fi

if [ -z "$OPENSSL_APP" ]; then
    echo "OPENSSL_APP env var not set. Exiting."
    exit 1
fi

if [ -z "$OPENSSL_MODULES" ]; then
    echo "Warning: OPENSSL_MODULES env var not set."
fi

if [ -z "$OPENSSL_CONF" ]; then
    echo "Warning: OPENSSL_CONF env var not set."
fi

# Set OSX DYLD_LIBRARY_PATH if not already externally set
if [ -z "$DYLD_LIBRARY_PATH" ]; then
    export DYLD_LIBRARY_PATH=$LD_LIBRARY_PATH
fi

echo "oqsprovider-ca.sh commencing..."

#rm -rf tmp
mkdir -p tmp && cd tmp
rm -rf demoCA && mkdir -p demoCA/newcerts
touch demoCA/index.txt
echo '01' > demoCA/serial
$OPENSSL_APP req -x509 -new -newkey $1 -keyout $1_rootCA.key -out $1_rootCA.crt -subj "/CN=test CA" -nodes

if [ $? -ne 0 ]; then
   echo "Failed to generate root CA. Exiting."
   exit 1
fi

$OPENSSL_APP req -new -newkey $1 -keyout $1.key -out $1.csr -nodes -subj "/CN=test Server"

if [ $? -ne 0 ]; then
   echo "Failed to generate test server CSR. Exiting."
   exit 1
fi

$OPENSSL_APP ca -batch -days 100 -keyfile $1_rootCA.key -cert $1_rootCA.crt -policy policy_anything -notext -out $1.crt -infiles $1.csr

if [ $? -ne 0 ]; then
   echo "Failed to generate server CRT. Exiting."
   exit 1
fi

# Don't forget to use provider(s) when not activated via config file
$OPENSSL_APP verify -CAfile $1_rootCA.crt $1.crt

