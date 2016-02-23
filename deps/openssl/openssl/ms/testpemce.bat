@echo off
set ssleay=%1%
set tmp1=pem.out
set cmp=fc.exe

cecopy %ssleay% CE:\OpenSSL

copy ..\test\testcrl.pem >nul
call tpemce.bat crl testcrl.pem
if errorlevel 1 goto err

copy ..\test\testp7.pem >nul
call tpemce.bat pkcs7 testp7.pem
if errorlevel 1 goto err

copy ..\test\testreq2.pem >nul
call tpemce.bat req testreq2.pem
if errorlevel 1 goto err

copy ..\test\testrsa.pem >nul
call tpemce.bat rsa testrsa.pem
if errorlevel 1 goto err

copy ..\test\testx509.pem >nul
call tpemce.bat x509 testx509.pem
if errorlevel 1 goto err

copy ..\test\v3-cert1.pem >nul
call tpemce.bat x509 v3-cert1.pem
if errorlevel 1 goto err

copy ..\test\v3-cert1.pem >nul
call tpemce.bat x509 v3-cert1.pem
if errorlevel 1 goto err

copy ..\test\testsid.pem >nul
call tpemce.bat sess_id testsid.pem
if errorlevel 1 goto err

echo OK
del %tmp1% >nul 2>&1
:err
