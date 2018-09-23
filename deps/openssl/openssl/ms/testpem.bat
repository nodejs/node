@echo off
set ssleay=%1%
set tmp1=pem.out
set cmp=fc.exe

call tpem.bat crl ..\test\testcrl.pem
if errorlevel 1 goto err

call tpem.bat pkcs7 ..\test\testp7.pem
if errorlevel 1 goto err

call tpem.bat req ..\test\testreq2.pem
if errorlevel 1 goto err

call tpem.bat rsa ..\test\testrsa.pem
if errorlevel 1 goto err

call tpem.bat x509 ..\test\testx509.pem
if errorlevel 1 goto err

call tpem.bat x509 ..\test\v3-cert1.pem
if errorlevel 1 goto err

call tpem.bat x509 ..\test\v3-cert1.pem
if errorlevel 1 goto err

call tpem.bat sess_id ..\test\testsid.pem
if errorlevel 1 goto err

echo OK
del %tmp1%
:err
