@echo off
echo start testenc

path=..\ms;%path%
set ssleay=%1%
set input=..\ms\testenc.bat
set tmp1=..\ms\cipher.out
set out1=..\ms\clear.out
set cmp=perl ..\ms\cmp.pl

cd
call tenc.bat enc
if errorlevel 1 goto err

call tenc.bat rc4
if errorlevel 1 goto err

call tenc.bat des-cfb
if errorlevel 1 goto err

call tenc.bat des-ede-cfb
if errorlevel 1 goto err

call tenc.bat des-ede3-cfb
if errorlevel 1 goto err

call tenc.bat des-ofb
if errorlevel 1 goto err

call tenc.bat des-ede-ofb
if errorlevel 1 goto err

call tenc.bat des-ede3-ofb
if errorlevel 1 goto err

call tenc.bat des-ecb
if errorlevel 1 goto err

call tenc.bat des-ede
if errorlevel 1 goto err

call tenc.bat des-ede3
if errorlevel 1 goto err

call tenc.bat des-cbc
if errorlevel 1 goto err

call tenc.bat des-ede-cbc
if errorlevel 1 goto err

call tenc.bat des-ede3-cbc
if errorlevel 1 goto err

call tenc.bat idea-ecb
if errorlevel 1 goto err

call tenc.bat idea-cfb
if errorlevel 1 goto err

call tenc.bat idea-ofb
if errorlevel 1 goto err

call tenc.bat idea-cbc
if errorlevel 1 goto err

call tenc.bat rc2-ecb
if errorlevel 1 goto err

call tenc.bat rc2-cfb
if errorlevel 1 goto err

call tenc.bat rc2-ofb
if errorlevel 1 goto err

call tenc.bat rc2-cbc
if errorlevel 1 goto err

call tenc.bat bf-ecb
if errorlevel 1 goto err

call tenc.bat bf-cfb
if errorlevel 1 goto err

call tenc.bat bf-ofb
if errorlevel 1 goto err

call tenc.bat bf-cbc
if errorlevel 1 goto err

echo OK
del %out1%
del %tmp1%
:err

