#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl pkcs7'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=testp7.pem
fi

echo testing pkcs7 conversions
cp $t p7-fff.p

echo "p -> d"
$cmd -in p7-fff.p -inform p -outform d >p7-f.d
if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in p7-fff.p -inform p -outform p >p7-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -in p7-f.d -inform d -outform d >p7-ff.d1
if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -in p7-f.p -inform p -outform d >p7-ff.d3
if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in p7-f.d -inform d -outform p >p7-ff.p1
if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in p7-f.p -inform p -outform p >p7-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp p7-fff.p p7-f.p
if [ $? != 0 ]; then exit 1; fi
cmp p7-fff.p p7-ff.p1
if [ $? != 0 ]; then exit 1; fi
cmp p7-fff.p p7-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp p7-f.p p7-ff.p1
if [ $? != 0 ]; then exit 1; fi
cmp p7-f.p p7-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f p7-f.* p7-ff.* p7-fff.*
exit 0
