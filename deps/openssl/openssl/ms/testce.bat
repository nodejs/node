@echo off

cemkdir CE:\OpenSSL

set test=..\ms
set opath=%PATH%
PATH=..\ms;%PATH%
cecopy ..\apps\openssl.cnf CE:\OpenSSL
set OPENSSL_CONF=\OpenSSL\openssl.cnf
set HOME=\OpenSSL
set CERUN_PASS_ENV=OPENSSL_CONF HOME

rem run this from inside the bin directory

rem Copy the DLL's (though they'll only exist if we're in out32dll)
if exist libeay32.dll cecopy libeay32.dll CE:\OpenSSL
if exist ssleay32.dll cecopy ssleay32.dll CE:\OpenSSL

echo rsa_test
call %test%\testce2 rsa_test
if errorlevel 1 goto done

echo destest
call %test%\testce2 destest
if errorlevel 1 goto done

echo ideatest
call %test%\testce2 ideatest
if errorlevel 1 goto done

echo bftest
call %test%\testce2 bftest
if errorlevel 1 goto done

echo shatest
call %test%\testce2 shatest
if errorlevel 1 goto done

echo sha1test
call %test%\testce2 sha1test
if errorlevel 1 goto done

echo md5test
call %test%\testce2 md5test
if errorlevel 1 goto done

echo md2test
call %test%\testce2 md2test
if errorlevel 1 goto done

echo mdc2test
call %test%\testce2 mdc2test
if errorlevel 1 goto done

echo rc2test
call %test%\testce2 rc2test
if errorlevel 1 goto done

echo rc4test
call %test%\testce2 rc4test
if errorlevel 1 goto done

echo randtest
call %test%\testce2 randtest
if errorlevel 1 goto done

echo dhtest
call %test%\testce2 dhtest
if errorlevel 1 goto done

echo exptest
call %test%\testce2 exptest
if errorlevel 1 goto done

echo dsatest
call %test%\testce2 dsatest
if errorlevel 1 goto done

echo testenc
call %test%\testencce openssl.exe
if errorlevel 1 goto done

echo testpem
call %test%\testpemce openssl.exe
if errorlevel 1 goto done

cecopy openssl.exe CE:\OpenSSL

echo verify
copy ..\certs\*.pem cert.tmp >nul
cecopy cert.tmp CE:\OpenSSL
cemkdir CE:\OpenSSL\certs
rem cecopy ..\certs\*.pem CE:\OpenSSL\certs
cecopy ..\certs\ca-cert.pem CE:\OpenSSL\certs
cecopy ..\certs\dsa-ca.pem CE:\OpenSSL\certs
cecopy ..\certs\dsa-pca.pem CE:\OpenSSL\certs
cecopy ..\certs\factory.pem CE:\OpenSSL\certs
cecopy ..\certs\ICE-CA.pem CE:\OpenSSL\certs
cecopy ..\certs\ICE-root.pem CE:\OpenSSL\certs
cecopy ..\certs\ICE-user.pem CE:\OpenSSL\certs
cecopy ..\certs\nortelCA.pem CE:\OpenSSL\certs
cecopy ..\certs\pca-cert.pem CE:\OpenSSL\certs
cecopy ..\certs\RegTP-4R.pem CE:\OpenSSL\certs
cecopy ..\certs\RegTP-5R.pem CE:\OpenSSL\certs
cecopy ..\certs\RegTP-6R.pem CE:\OpenSSL\certs
cecopy ..\certs\rsa-cca.pem CE:\OpenSSL\certs
cecopy ..\certs\thawteCb.pem CE:\OpenSSL\certs
cecopy ..\certs\thawteCp.pem CE:\OpenSSL\certs
cecopy ..\certs\timCA.pem CE:\OpenSSL\certs
cecopy ..\certs\tjhCA.pem CE:\OpenSSL\certs
cecopy ..\certs\vsign1.pem CE:\OpenSSL\certs
cecopy ..\certs\vsign2.pem CE:\OpenSSL\certs
cecopy ..\certs\vsign3.pem CE:\OpenSSL\certs
cecopy ..\certs\vsignss.pem CE:\OpenSSL\certs
cecopy ..\certs\vsigntca.pem CE:\OpenSSL\certs
cerun CE:\OpenSSL\openssl verify -CAfile \OpenSSL\cert.tmp \OpenSSL\certs\*.pem

echo testss
call %test%\testssce openssl.exe
if errorlevel 1 goto done

cecopy ssltest.exe CE:\OpenSSL
cecopy ..\apps\server.pem CE:\OpenSSL
cecopy ..\apps\client.pem CE:\OpenSSL

echo test sslv2
cerun CE:\OpenSSL\ssltest -ssl2
if errorlevel 1 goto done

echo test sslv2 with server authentication
cerun CE:\OpenSSL\ssltest -ssl2 -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2 with client authentication
cerun CE:\OpenSSL\ssltest -ssl2 -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2 with both client and server authentication
cerun CE:\OpenSSL\ssltest -ssl2 -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3
cerun CE:\OpenSSL\ssltest -ssl3
if errorlevel 1 goto done

echo test sslv3 with server authentication
cerun CE:\OpenSSL\ssltest -ssl3 -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3 with client authentication
cerun CE:\OpenSSL\ssltest -ssl3 -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3 with both client and server authentication
cerun CE:\OpenSSL\ssltest -ssl3 -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3
cerun CE:\OpenSSL\ssltest
if errorlevel 1 goto done

echo test sslv2/sslv3 with server authentication
cerun CE:\OpenSSL\ssltest -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3 with client authentication
cerun CE:\OpenSSL\ssltest -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3 with both client and server authentication
cerun CE:\OpenSSL\ssltest -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2 via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl2
if errorlevel 1 goto done

echo test sslv2/sslv3 with 1024 bit DHE via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -dhe1024dsa -v
if errorlevel 1 goto done

echo test sslv2 with server authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl2 -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2 with client authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl2 -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2 with both client and server authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl2 -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3 via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl3
if errorlevel 1 goto done

echo test sslv3 with server authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl3 -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3 with client authentication  via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl3 -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv3 with both client and server authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -ssl3 -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3 via BIO pair
cerun CE:\OpenSSL\ssltest
if errorlevel 1 goto done

echo test sslv2/sslv3 with server authentication
cerun CE:\OpenSSL\ssltest -bio_pair -server_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3 with client authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

echo test sslv2/sslv3 with both client and server authentication via BIO pair
cerun CE:\OpenSSL\ssltest -bio_pair -server_auth -client_auth -CAfile \OpenSSL\cert.tmp
if errorlevel 1 goto done

del cert.tmp

echo passed all tests
goto end
:done
echo problems.....
:end
PATH=%opath%

