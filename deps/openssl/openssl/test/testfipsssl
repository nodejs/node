#!/bin/sh

if [ "$1" = "" ]; then
  key=../apps/server.pem
else
  key="$1"
fi
if [ "$2" = "" ]; then
  cert=../apps/server.pem
else
  cert="$2"
fi

ciphers="DHE-RSA-AES256-SHA:DHE-DSS-AES256-SHA:AES256-SHA:EDH-RSA-DES-CBC3-SHA:EDH-DSS-DES-CBC3-SHA:DES-CBC3-SHA:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA:AES128-SHA:EXP1024-DHE-DSS-DES-CBC-SHA:EXP1024-DES-CBC-SHA:EDH-RSA-DES-CBC-SHA:EDH-DSS-DES-CBC-SHA:DES-CBC-SHA:EXP-EDH-RSA-DES-CBC-SHA:EXP-EDH-DSS-DES-CBC-SHA:EXP-DES-CBC-SHA"

ssltest="../util/shlib_wrap.sh ./ssltest -F -key $key -cert $cert -c_key $key -c_cert $cert -cipher $ciphers"

if ../util/shlib_wrap.sh ../apps/openssl x509 -in $cert -text -noout | fgrep 'DSA Public Key' >/dev/null; then
  dsa_cert=YES
else
  dsa_cert=NO
fi

if [ "$3" = "" ]; then
  CA="-CApath ../certs"
else
  CA="-CAfile $3"
fi

if [ "$4" = "" ]; then
  extra=""
else
  extra="$4"
fi

#############################################################################

echo test ssl3 is forbidden in FIPS mode
$ssltest -ssl3 $extra && exit 1

if ../util/shlib_wrap.sh ../apps/openssl ciphers SSLv2 >/dev/null 2>&1; then
    echo test ssl2 is forbidden in FIPS mode
    $ssltest -ssl2 $extra && exit 1
else
    echo ssl2 disabled: skipping test
fi

echo test tls1
$ssltest -tls1 $extra || exit 1

echo test tls1 with server authentication
$ssltest -tls1 -server_auth $CA $extra || exit 1

echo test tls1 with client authentication
$ssltest -tls1 -client_auth $CA $extra || exit 1

echo test tls1 with both client and server authentication
$ssltest -tls1 -server_auth -client_auth $CA $extra || exit 1

echo test tls1 via BIO pair
$ssltest -bio_pair -tls1 $extra || exit 1

echo test tls1 with server authentication via BIO pair
$ssltest -bio_pair -tls1 -server_auth $CA $extra || exit 1

echo test tls1 with client authentication via BIO pair
$ssltest -bio_pair -tls1 -client_auth $CA $extra || exit 1

echo test tls1 with both client and server authentication via BIO pair
$ssltest -bio_pair -tls1 -server_auth -client_auth $CA $extra || exit 1

# note that all the below actually choose TLS...

if [ $dsa_cert = NO ]; then
  echo test sslv2/sslv3 w/o DHE via BIO pair
  $ssltest -bio_pair -no_dhe $extra || exit 1
fi

echo test sslv2/sslv3 with 1024bit DHE via BIO pair
$ssltest -bio_pair -dhe1024dsa -v $extra || exit 1

echo test sslv2/sslv3 with server authentication
$ssltest -bio_pair -server_auth $CA $extra || exit 1

echo test sslv2/sslv3 with client authentication via BIO pair
$ssltest -bio_pair -client_auth $CA $extra || exit 1

echo test sslv2/sslv3 with both client and server authentication via BIO pair
$ssltest -bio_pair -server_auth -client_auth $CA $extra || exit 1

echo test sslv2/sslv3 with both client and server authentication via BIO pair and app verify
$ssltest -bio_pair -server_auth -client_auth -app_verify $CA $extra || exit 1

#############################################################################

if ../util/shlib_wrap.sh ../apps/openssl no-dh; then
  echo skipping anonymous DH tests
else
  echo test tls1 with 1024bit anonymous DH, multiple handshakes
  $ssltest -v -bio_pair -tls1 -cipher ADH -dhe1024dsa -num 10 -f -time $extra || exit 1
fi

if ../util/shlib_wrap.sh ../apps/openssl no-rsa; then
  echo skipping RSA tests
else
  echo test tls1 with 1024bit RSA, no DHE, multiple handshakes
  ../util/shlib_wrap.sh ./ssltest -v -bio_pair -tls1 -cert ../apps/server2.pem -no_dhe -num 10 -f -time $extra || exit 1

  if ../util/shlib_wrap.sh ../apps/openssl no-dh; then
    echo skipping RSA+DHE tests
  else
    echo test tls1 with 1024bit RSA, 1024bit DHE, multiple handshakes
    ../util/shlib_wrap.sh ./ssltest -v -bio_pair -tls1 -cert ../apps/server2.pem -dhe1024dsa -num 10 -f -time $extra || exit 1
  fi
fi

exit 0
