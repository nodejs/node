#!/bin/sh

if ../util/shlib_wrap.sh ../apps/openssl no-rsa; then
  echo skipping rsa conversion test
  exit 0
fi

cmd='../util/shlib_wrap.sh ../apps/openssl rsa'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=testrsa.pem
fi

echo testing rsa conversions
cp $t rsa-fff.p

echo "p -> d"
$cmd -in rsa-fff.p -inform p -outform d >rsa-f.d
if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in rsa-fff.p -inform p -outform t >rsa-f.t
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in rsa-fff.p -inform p -outform p >rsa-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -in rsa-f.d -inform d -outform d >rsa-ff.d1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> d"
#$cmd -in rsa-f.t -inform t -outform d >rsa-ff.d2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -in rsa-f.p -inform p -outform d >rsa-ff.d3
if [ $? != 0 ]; then exit 1; fi

#echo "d -> t"
#$cmd -in rsa-f.d -inform d -outform t >rsa-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#echo "t -> t"
#$cmd -in rsa-f.t -inform t -outform t >rsa-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in rsa-f.p -inform p -outform t >rsa-ff.t3
#if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in rsa-f.d -inform d -outform p >rsa-ff.p1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> p"
#$cmd -in rsa-f.t -inform t -outform p >rsa-ff.p2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in rsa-f.p -inform p -outform p >rsa-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp rsa-fff.p rsa-f.p
if [ $? != 0 ]; then exit 1; fi
cmp rsa-fff.p rsa-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp rsa-fff.p rsa-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp rsa-fff.p rsa-ff.p3
if [ $? != 0 ]; then exit 1; fi

#cmp rsa-f.t rsa-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#cmp rsa-f.t rsa-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#cmp rsa-f.t rsa-ff.t3
#if [ $? != 0 ]; then exit 1; fi

cmp rsa-f.p rsa-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp rsa-f.p rsa-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp rsa-f.p rsa-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f rsa-f.* rsa-ff.* rsa-fff.*
exit 0
