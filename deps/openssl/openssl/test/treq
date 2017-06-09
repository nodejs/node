#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl req -config ../apps/openssl.cnf'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=testreq.pem
fi

if $cmd -in $t -inform p -noout -text 2>&1 | fgrep -i 'Unknown Public Key'; then
  echo "skipping req conversion test for $t"
  exit 0
fi

echo testing req conversions
cp $t req-fff.p

echo "p -> d"
$cmd -in req-fff.p -inform p -outform d >req-f.d
if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in req-fff.p -inform p -outform t >req-f.t
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in req-fff.p -inform p -outform p >req-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -verify -in req-f.d -inform d -outform d >req-ff.d1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> d"
#$cmd -in req-f.t -inform t -outform d >req-ff.d2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -verify -in req-f.p -inform p -outform d >req-ff.d3
if [ $? != 0 ]; then exit 1; fi

#echo "d -> t"
#$cmd -in req-f.d -inform d -outform t >req-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#echo "t -> t"
#$cmd -in req-f.t -inform t -outform t >req-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in req-f.p -inform p -outform t >req-ff.t3
#if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in req-f.d -inform d -outform p >req-ff.p1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> p"
#$cmd -in req-f.t -inform t -outform p >req-ff.p2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in req-f.p -inform p -outform p >req-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp req-fff.p req-f.p
if [ $? != 0 ]; then exit 1; fi
cmp req-fff.p req-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp req-fff.p req-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp req-fff.p req-ff.p3
if [ $? != 0 ]; then exit 1; fi

#cmp req-f.t req-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#cmp req-f.t req-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#cmp req-f.t req-ff.t3
#if [ $? != 0 ]; then exit 1; fi

cmp req-f.p req-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp req-f.p req-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp req-f.p req-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f req-f.* req-ff.* req-fff.*
exit 0
