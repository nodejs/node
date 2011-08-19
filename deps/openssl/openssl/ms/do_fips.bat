@echo off

SET ASM=%1

if NOT X%PROCESSOR_ARCHITECTURE% == X goto defined 

echo Processor Architecture Undefined: defaulting to X86

goto X86

:defined

if %PROCESSOR_ARCHITECTURE% == x86 goto X86

if %PROCESSOR_ARCHITECTURE% == IA64 goto IA64

if %PROCESSOR_ARCHITECTURE% == AMD64 goto AMD64

echo Processor Architecture Unrecognized: defaulting to X86

:X86
echo Auto Configuring for X86

SET TARGET=VC-WIN32

if x%ASM% == xno-asm goto compile
echo Generating x86 for NASM assember
SET ASM=nasm
SET ASMOPTS=-DOPENSSL_IA32_SSE2

echo Bignum
cd crypto\bn\asm
perl bn-586.pl win32n %ASMOPTS% > bn_win32.asm
if ERRORLEVEL 1 goto error
perl co-586.pl win32n %ASMOPTS% > co_win32.asm
if ERRORLEVEL 1 goto error
perl mo-586.pl win32n %ASMOPTS% > mt_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo AES
cd crypto\aes\asm
perl aes-586.pl win32n %ASMOPTS% > a_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo DES
cd crypto\des\asm
perl des-586.pl win32n %ASMOPTS% > d_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo "crypt(3)"

cd crypto\des\asm
perl crypt586.pl win32n %ASMOPTS% > y_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo Blowfish

cd crypto\bf\asm
perl bf-586.pl win32n %ASMOPTS% > b_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo CAST5
cd crypto\cast\asm
perl cast-586.pl win32n %ASMOPTS% > c_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo RC4
cd crypto\rc4\asm
perl rc4-586.pl win32n %ASMOPTS% > r4_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo MD5
cd crypto\md5\asm
perl md5-586.pl win32n %ASMOPTS% > m5_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo SHA1
cd crypto\sha\asm
perl sha1-586.pl win32n %ASMOPTS% > s1_win32.asm
if ERRORLEVEL 1 goto error
perl sha512-sse2.pl win32n %ASMOPTS% > sha512-sse2.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo RIPEMD160
cd crypto\ripemd\asm
perl rmd-586.pl win32n %ASMOPTS% > rm_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo RC5\32
cd crypto\rc5\asm
perl rc5-586.pl win32n %ASMOPTS% > r5_win32.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo CPU-ID
cd crypto
perl x86cpuid.pl win32n %ASMOPTS% > cpu_win32.asm
if ERRORLEVEL 1 goto error
cd ..

goto compile

:IA64

echo Auto Configuring for IA64
SET TARGET=VC-WIN64I
perl ms\uplink.pl win64i > ms\uptable.asm
if ERRORLEVEL 1 goto error
ias -o ms\uptable.obj ms\uptable.asm
if ERRORLEVEL 1 goto error

goto compile

:AMD64

echo Auto Configuring for AMD64
SET TARGET=VC-WIN64A
perl ms\uplink.pl win64a > ms\uptable.asm
if ERRORLEVEL 1 goto error
ml64 -c -Foms\uptable.obj ms\uptable.asm
if ERRORLEVEL 1 goto error

if x%ASM% == xno-asm goto compile
echo Generating x86_64 for ML64 assember
SET ASM=ml64

echo Bignum
cd crypto\bn\asm
perl x86_64-mont.pl x86_64-mont.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo AES
cd crypto\aes\asm
perl aes-x86_64.pl aes-x86_64.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo SHA
cd crypto\sha\asm
perl sha1-x86_64.pl sha1-x86_64.asm
if ERRORLEVEL 1 goto error
perl sha512-x86_64.pl sha256-x86_64.asm
if ERRORLEVEL 1 goto error
perl sha512-x86_64.pl sha512-x86_64.asm
if ERRORLEVEL 1 goto error
cd ..\..\..

echo CPU-ID
cd crypto
perl x86_64cpuid.pl cpuid-x86_64.asm
if ERRORLEVEL 1 goto error
cd ..

:compile

perl Configure %TARGET% fipscanisterbuild
pause

echo on

perl util\mkfiles.pl >MINFO
@if ERRORLEVEL 1 goto error
perl util\mk1mf.pl dll %ASM% %TARGET% >ms\ntdll.mak
@if ERRORLEVEL 1 goto error

perl util\mkdef.pl 32 libeay > ms\libeay32.def
@if ERRORLEVEL 1 goto error
perl util\mkdef.pl 32 ssleay > ms\ssleay32.def
@if ERRORLEVEL 1 goto error

nmake -f ms\ntdll.mak clean
nmake -f ms\ntdll.mak
@if ERRORLEVEL 1 goto error

@echo.
@echo.
@echo.
@echo ***************************
@echo ****FIPS BUILD SUCCESS*****
@echo ***************************

@goto end

:error

@echo.
@echo.
@echo.
@echo ***************************
@echo ****FIPS BUILD FAILURE*****
@echo ***************************

:end
