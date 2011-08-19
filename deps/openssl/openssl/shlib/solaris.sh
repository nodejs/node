#!/bin/sh

echo "#define DATE      \"`date`\"" >crypto/date.h

major="0"
minor="8.0"
slib=libssl
clib=libcrypto
CC=gcc
CPP='gcc -E'
AS=as
#FLAGS='-DTERMIO -O3 -DL_ENDIAN -fomit-frame-pointer -mv8 -Wall'
FLAGS='-DTERMIO -g2 -ggdb -DL_ENDIAN -Wall -DREF_CHECK -DCRYPTO_MDEBUG'
INCLUDE='-Iinclude -Icrypto -Issl'
SHFLAGS='-DPIC -fpic'

CFLAGS="$FLAGS $INCLUDE $SHFLAGS"
ASM_OBJ="";

echo compiling bignum assember
$AS -o bn_asm.o crypto/bn/asm/sparc.s
CFLAGS="$CFLAGS -DBN_ASM"
ASM_OBJ="$ASM_OBJ bn_asm.o"

echo compiling $clib
$CC -c $CFLAGS -DCFLAGS="\"$FLAGS\"" -o crypto.o crypto/crypto.c

echo linking $clib.so
gcc $CFLAGS -shared -o $clib.so.$major.$minor crypto.o $ASM_OBJ -lnsl -lsocket

echo compiling $slib.so
$CC -c $CFLAGS -o ssl.o ssl/ssl.c

echo building $slib.so
gcc $CFLAGS -shared -o $slib.so ssl.o -L. -lcrypto

