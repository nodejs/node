
@echo off
echo Generating x86 assember

echo Bignum
cd crypto\bn\asm
perl x86.pl win32n > bn-win32.asm
cd ..\..\..

echo DES
cd crypto\des\asm
perl des-586.pl win32n > d-win32.asm
cd ..\..\..

echo "crypt(3)"

cd crypto\des\asm
perl crypt586.pl win32n > y-win32.asm
cd ..\..\..

echo Blowfish

cd crypto\bf\asm
perl bf-586.pl win32n > b-win32.asm
cd ..\..\..

echo CAST5
cd crypto\cast\asm
perl cast-586.pl win32n > c-win32.asm
cd ..\..\..

echo RC4
cd crypto\rc4\asm
perl rc4-586.pl win32n > r4-win32.asm
cd ..\..\..

echo MD5
cd crypto\md5\asm
perl md5-586.pl win32n > m5-win32.asm
cd ..\..\..

echo SHA1
cd crypto\sha\asm
perl sha1-586.pl win32n > s1-win32.asm
cd ..\..\..

echo RIPEMD160
cd crypto\ripemd\asm
perl rmd-586.pl win32n > rm-win32.asm
cd ..\..\..

echo RC5\32
cd crypto\rc5\asm
perl rc5-586.pl win32n > r5-win32.asm
cd ..\..\..

echo on
