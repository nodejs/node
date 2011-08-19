
@echo off
echo Bignum
cd crypto\bn\asm
perl x86.pl %1 > bn%2
perl bn-586.pl %1 > bn%2
perl co-586.pl %1 > co%2
cd ..\..\..

echo DES
cd crypto\des\asm
perl des-586.pl %1 > d%2
cd ..\..\..

echo "crypt(3)"

cd crypto\des\asm
perl crypt586.pl %1 > y%2
cd ..\..\..

echo Blowfish

cd crypto\bf\asm
perl bf-586.pl %1 > b%2
cd ..\..\..

echo CAST5
cd crypto\cast\asm
perl cast-586.pl %1 > c%2
cd ..\..\..

echo RC4
cd crypto\rc4\asm
perl rc4-586.pl %1 > r4%2
cd ..\..\..

echo MD5
cd crypto\md5\asm
perl md5-586.pl %1 > m5%2
cd ..\..\..

echo SHA1
cd crypto\sha\asm
perl sha1-586.pl %1 > s1%2
cd ..\..\..

echo RIPEMD160
cd crypto\ripemd\asm
perl rmd-586.pl %1 > rm%2
cd ..\..\..

echo RC5\32
cd crypto\rc5\asm
perl rc5-586.pl %1 > r5%2
cd ..\..\..

echo CPUID
cd crypto
perl x86cpuid.pl %1 > x86cpuid%2
cd ..\


echo on
