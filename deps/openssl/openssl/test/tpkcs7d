#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl pkcs7'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=pkcs7-1.pem
fi

echo "testing pkcs7 conversions (2)"
cp $t p7d-fff.p

echo "p -> d"
$cmd -in p7d-fff.p -inform p -outform d >p7d-f.d
if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in p7d-fff.p -inform p -outform p >p7d-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -in p7d-f.d -inform d -outform d >p7d-ff.d1
if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -in p7d-f.p -inform p -outform d >p7d-ff.d3
if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in p7d-f.d -inform d -outform p >p7d-ff.p1
if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in p7d-f.p -inform p -outform p >p7d-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp p7d-f.p p7d-ff.p1
if [ $? != 0 ]; then exit 1; fi
cmp p7d-f.p p7d-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f p7d-f.* p7d-ff.* p7d-fff.*
exit 0
