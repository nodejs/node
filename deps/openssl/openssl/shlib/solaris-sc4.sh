#!/bin/sh

major="1"

slib=libssl
sh_slib=$slib.so.$major

clib=libcrypto
sh_clib=$clib.so.$major

echo collecting all object files for $clib.so
OBJS=
find . -name \*.o -print > allobjs
for obj in `ar t libcrypto.a`
do
	OBJS="$OBJS `grep $obj allobjs`"
done

echo linking $clib.so
cc -G -o $sh_clib -h $sh_clib $OBJS -lnsl -lsocket

rm -f $clib.so
ln -s $sh_clib $clib.so

echo collecting all object files for $slib.so
OBJS=
for obj in `ar t libssl.a`
do
	OBJS="$OBJS `grep $obj allobjs`"
done

echo linking $slib.so
cc -G -o $sh_slib -h $sh_slib $OBJS -L. -lcrypto
	
rm -f $slib.so
ln -s $sh_slib $slib.so

rm -f allobjs

mv libRSAglue.a libRSAglue.a.orig
mv libcrypto.a  libcrypto.a.orig
mv libssl.a     libssl.a.orig 
