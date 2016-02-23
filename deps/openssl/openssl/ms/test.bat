@echo off

set test=..\ms
set opath=%PATH%
PATH=..\ms;%PATH%
set OPENSSL_CONF=..\apps\openssl.cnf

rem run this from inside the bin directory

echo rsa_test
rsa_test
if errorlevel 1 goto done

echo destest
destest
if errorlevel 1 goto done

echo ideatest
ideatest
if errorlevel 1 goto done

echo bftest
bftest
if errorlevel 1 goto done

echo shatest
shatest
if errorlevel 1 goto done

echo sha1test
sha1test
if errorlevel 1 goto done

echo md5test
md5test
if errorlevel 1 goto done

echo rc2test
rc2test
if errorlevel 1 goto done

echo rc4test
rc4test
if errorlevel 1 goto done

echo randtest
randtest
if errorlevel 1 goto done

echo dhtest
dhtest
if errorlevel 1 goto done

echo exptest
exptest
if errorlevel 1 goto done

echo dsatest
dsatest
if errorlevel 1 goto done

echo ectest
ectest
if errorlevel 1 goto done

echo testenc
call %test%\testenc openssl
if errorlevel 1 goto done

echo testpem
call %test%\testpem openssl
if errorlevel 1 goto done

echo testss
call %test%\testss openssl
if errorlevel 1 goto done

set SSL_TEST=ssltest -key keyU.ss -cert certU.ss -c_key keyU.ss -c_cert certU.ss -CAfile certCA.ss

echo test sslv2
ssltest -ssl2
if errorlevel 1 goto done

echo test sslv2 with server authentication
%SSL_TEST% -ssl2 -server_auth
if errorlevel 1 goto done

echo test sslv2 with client authentication
%SSL_TEST% -ssl2 -client_auth
if errorlevel 1 goto done

echo test sslv2 with both client and server authentication
%SSL_TEST% -ssl2 -server_auth -client_auth
if errorlevel 1 goto done

echo test sslv3
ssltest -ssl3
if errorlevel 1 goto done

echo test sslv3 with server authentication
%SSL_TEST% -ssl3 -server_auth
if errorlevel 1 goto done

echo test sslv3 with client authentication
%SSL_TEST% -ssl3 -client_auth
if errorlevel 1 goto done

echo test sslv3 with both client and server authentication
%SSL_TEST% -ssl3 -server_auth -client_auth
if errorlevel 1 goto done

echo test sslv2/sslv3
ssltest
if errorlevel 1 goto done

echo test sslv2/sslv3 with server authentication
%SSL_TEST% -server_auth
if errorlevel 1 goto done

echo test sslv2/sslv3 with client authentication
%SSL_TEST% -client_auth
if errorlevel 1 goto done

echo test sslv2/sslv3 with both client and server authentication
%SSL_TEST% -server_auth -client_auth
if errorlevel 1 goto done

echo test sslv2 via BIO pair
ssltest -bio_pair -ssl2
if errorlevel 1 goto done

echo test sslv2/sslv3 with 1024 bit DHE via BIO pair
ssltest -bio_pair -dhe1024dsa -v
if errorlevel 1 goto done

echo test sslv2 with server authentication via BIO pair
%SSL_TEST% -bio_pair -ssl2 -server_auth
if errorlevel 1 goto done

echo test sslv2 with client authentication via BIO pair
%SSL_TEST% -bio_pair -ssl2 -client_auth
if errorlevel 1 goto done

echo test sslv2 with both client and server authentication via BIO pair
%SSL_TEST% -bio_pair -ssl2 -server_auth -client_auth
if errorlevel 1 goto done

echo test sslv3 via BIO pair
ssltest -bio_pair -ssl3
if errorlevel 1 goto done

echo test sslv3 with server authentication via BIO pair
%SSL_TEST% -bio_pair -ssl3 -server_auth
if errorlevel 1 goto done

echo test sslv3 with client authentication  via BIO pair
%SSL_TEST% -bio_pair -ssl3 -client_auth
if errorlevel 1 goto done

echo test sslv3 with both client and server authentication via BIO pair
%SSL_TEST% -bio_pair -ssl3 -server_auth -client_auth
if errorlevel 1 goto done

echo test sslv2/sslv3 via BIO pair
ssltest -bio_pair
if errorlevel 1 goto done

echo test sslv2/sslv3 with server authentication
%SSL_TEST% -bio_pair -server_auth
if errorlevel 1 goto done

echo test sslv2/sslv3 with client authentication via BIO pair
%SSL_TEST% -bio_pair -client_auth
if errorlevel 1 goto done

echo test sslv2/sslv3 with both client and server authentication via BIO pair
%SSL_TEST% -bio_pair -server_auth -client_auth
if errorlevel 1 goto done

echo passed all tests
goto end
:done
echo problems.....
:end
PATH=%opath%
