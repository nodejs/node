@echo off

rem   Batch file to copy OpenSSL stuff to a NetWare server for testing

rem   This batch file will create an "opensssl" directory at the root of the
rem   specified NetWare drive and copy the required files to run the tests.
rem   It should be run from inside the "openssl\netware" subdirectory.

rem   Usage:
rem      cpy_tests.bat <test subdirectory> <NetWare drive>
rem          <test subdirectory> - out_nw.dbg | out_nw
rem          <NetWare drive> - any mapped drive letter
rem
rem      example ( copy from debug build to m: dirve ):
rem              cpy_tests.bat out_nw.dbg m:
rem
rem      CAUTION:  If a directory named OpenSSL exists on the target drive
rem                it will be deleted first.


if "%1" == "" goto usage
if "%2" == "" goto usage

rem   Assume running in \openssl directory unless cpy_tests.bat exists then
rem   it must be the \openssl\netware directory
set loc=.
if exist cpy_tests.bat set loc=..

rem   make sure the local build subdirectory specified is valid
if not exist %loc%\%1\NUL goto invalid_dir

rem   make sure target drive is valid
if not exist %2\NUL goto invalid_drive

rem   If an OpenSSL directory exists on the target drive, remove it
if exist %2\openssl\NUL goto remove_openssl
goto do_copy

:remove_openssl
echo .
echo OpenSSL directory exists on %2 - it will be removed!
pause
rmdir %2\openssl /s /q

:do_copy
rem   make an "openssl" directory and others at the root of the NetWare drive
mkdir %2\openssl
mkdir %2\openssl\test_out
mkdir %2\openssl\apps
mkdir %2\openssl\certs
mkdir %2\openssl\test


rem   copy the test nlms
copy %loc%\%1\*.nlm %2\openssl\

rem   copy the test perl script
copy %loc%\netware\do_tests.pl %2\openssl\

rem   copy the certs directory stuff
xcopy %loc%\certs\*.*         %2\openssl\certs\ /s

rem   copy the test directory stuff
copy %loc%\test\CAss.cnf      %2\openssl\test\
copy %loc%\test\Uss.cnf       %2\openssl\test\
copy %loc%\test\pkcs7.pem     %2\openssl\test\
copy %loc%\test\pkcs7-1.pem   %2\openssl\test\
copy %loc%\test\testcrl.pem   %2\openssl\test\
copy %loc%\test\testp7.pem    %2\openssl\test\
copy %loc%\test\testreq2.pem  %2\openssl\test\
copy %loc%\test\testrsa.pem   %2\openssl\test\
copy %loc%\test\testsid.pem   %2\openssl\test\
copy %loc%\test\testx509.pem  %2\openssl\test\
copy %loc%\test\v3-cert1.pem  %2\openssl\test\
copy %loc%\test\v3-cert2.pem  %2\openssl\test\
copy %loc%\crypto\evp\evptests.txt %2\openssl\test\

rem   copy the apps directory stuff
copy %loc%\apps\client.pem    %2\openssl\apps\
copy %loc%\apps\server.pem    %2\openssl\apps\
copy %loc%\apps\openssl.cnf   %2\openssl\apps\

echo .
echo Tests copied
echo Run the test script at the console by typing:
echo     "Perl \openssl\do_tests.pl"
echo .
echo Make sure the Search path includes the OpenSSL subdirectory

goto end

:invalid_dir
echo.
echo Invalid build directory specified: %1
echo.
goto usage

:invalid_drive
echo.
echo Invalid drive: %2
echo.
goto usage

:usage
echo.
echo usage: cpy_tests.bat [test subdirectory] [NetWare drive]
echo     [test subdirectory] - out_nw_clib.dbg, out_nw_libc.dbg, etc. 
echo     [NetWare drive]     - any mapped drive letter
echo.
echo example: cpy_test out_nw_clib.dbg M:
echo  (copy from clib debug build area to M: drive)

:end
