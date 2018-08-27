#!/bin/sh

# This script will re-make all the required certs.
# cd apps
# sh ../util/mkcerts.sh
# mv ca-cert.pem pca-cert.pem ../certs
# cd ..
# cat certs/*.pem >>apps/server.pem
# cat certs/*.pem >>apps/server2.pem
# SSLEAY=`pwd`/apps/ssleay; export SSLEAY
# sh tools/c_rehash certs
#
 
CAbits=1024
SSLEAY="../apps/openssl"
CONF="-config ../apps/openssl.cnf"

# create pca request.
echo creating $CAbits bit PCA cert request
$SSLEAY req $CONF \
	-new -sha256 -newkey $CAbits \
	-keyout pca-key.pem \
	-out pca-req.pem -nodes >/dev/null <<EOF
AU
Queensland
.
CryptSoft Pty Ltd
.
Test PCA (1024 bit)



EOF

if [ $? != 0 ]; then
	echo problems generating PCA request
	exit 1
fi

#sign it.
echo
echo self signing PCA
$SSLEAY x509 -sha256 -days 36525 \
	-req -signkey pca-key.pem \
	-CAcreateserial -CAserial pca-cert.srl \
	-in pca-req.pem -out pca-cert.pem

if [ $? != 0 ]; then
	echo problems self signing PCA cert
	exit 1
fi
echo

# create ca request.
echo creating $CAbits bit CA cert request
$SSLEAY req $CONF \
	-new -sha256 -newkey $CAbits \
	-keyout ca-key.pem \
	-out ca-req.pem -nodes >/dev/null <<EOF
AU
Queensland
.
CryptSoft Pty Ltd
.
Test CA (1024 bit)



EOF

if [ $? != 0 ]; then
	echo problems generating CA request
	exit 1
fi

#sign it.
echo
echo signing CA
$SSLEAY x509 -sha256 -days 36525 \
	-req \
	-CAcreateserial -CAserial pca-cert.srl \
	-CA pca-cert.pem -CAkey pca-key.pem \
	-in ca-req.pem -out ca-cert.pem

if [ $? != 0 ]; then
	echo problems signing CA cert
	exit 1
fi
echo

# create server request.
echo creating 512 bit server cert request
$SSLEAY req $CONF \
	-new -sha256 -newkey 512 \
	-keyout s512-key.pem \
	-out s512-req.pem -nodes >/dev/null <<EOF
AU
Queensland
.
CryptSoft Pty Ltd
.
Server test cert (512 bit)



EOF

if [ $? != 0 ]; then
	echo problems generating 512 bit server cert request
	exit 1
fi

#sign it.
echo
echo signing 512 bit server cert
$SSLEAY x509 -sha256 -days 36525 \
	-req \
	-CAcreateserial -CAserial ca-cert.srl \
	-CA ca-cert.pem -CAkey ca-key.pem \
	-in s512-req.pem -out server.pem

if [ $? != 0 ]; then
	echo problems signing 512 bit server cert
	exit 1
fi
echo

# create 1024 bit server request.
echo creating 1024 bit server cert request
$SSLEAY req $CONF \
	-new -sha256 -newkey 1024 \
	-keyout s1024key.pem \
	-out s1024req.pem -nodes >/dev/null <<EOF
AU
Queensland
.
CryptSoft Pty Ltd
.
Server test cert (1024 bit)



EOF

if [ $? != 0 ]; then
	echo problems generating 1024 bit server cert request
	exit 1
fi

#sign it.
echo
echo signing 1024 bit server cert
$SSLEAY x509 -sha256 -days 36525 \
	-req \
	-CAcreateserial -CAserial ca-cert.srl \
	-CA ca-cert.pem -CAkey ca-key.pem \
	-in s1024req.pem -out server2.pem

if [ $? != 0 ]; then
	echo problems signing 1024 bit server cert
	exit 1
fi
echo

# create 512 bit client request.
echo creating 512 bit client cert request
$SSLEAY req $CONF \
	-new -sha256 -newkey 512 \
	-keyout c512-key.pem \
	-out c512-req.pem -nodes >/dev/null <<EOF
AU
Queensland
.
CryptSoft Pty Ltd
.
Client test cert (512 bit)



EOF

if [ $? != 0 ]; then
	echo problems generating 512 bit client cert request
	exit 1
fi

#sign it.
echo
echo signing 512 bit client cert
$SSLEAY x509 -sha256 -days 36525 \
	-req \
	-CAcreateserial -CAserial ca-cert.srl \
	-CA ca-cert.pem -CAkey ca-key.pem \
	-in c512-req.pem -out client.pem

if [ $? != 0 ]; then
	echo problems signing 512 bit client cert
	exit 1
fi

echo cleanup

cat pca-key.pem  >> pca-cert.pem
cat ca-key.pem   >> ca-cert.pem
cat s512-key.pem >> server.pem
cat s1024key.pem >> server2.pem
cat c512-key.pem >> client.pem

for i in pca-cert.pem ca-cert.pem server.pem server2.pem client.pem
do
$SSLEAY x509 -issuer -subject -in $i -noout >$$
cat $$
/bin/cat $i >>$$
/bin/mv $$ $i
done

#/bin/rm -f *key.pem *req.pem *.srl

echo Finished

