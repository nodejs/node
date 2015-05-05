#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl crl'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=testcrl.pem
fi

echo testing crl conversions
cp $t crl-fff.p

echo "p -> d"
$cmd -in crl-fff.p -inform p -outform d >crl-f.d
if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in crl-fff.p -inform p -outform t >crl-f.t
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in crl-fff.p -inform p -outform p >crl-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -in crl-f.d -inform d -outform d >crl-ff.d1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> d"
#$cmd -in crl-f.t -inform t -outform d >crl-ff.d2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -in crl-f.p -inform p -outform d >crl-ff.d3
if [ $? != 0 ]; then exit 1; fi

#echo "d -> t"
#$cmd -in crl-f.d -inform d -outform t >crl-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#echo "t -> t"
#$cmd -in crl-f.t -inform t -outform t >crl-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in crl-f.p -inform p -outform t >crl-ff.t3
#if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in crl-f.d -inform d -outform p >crl-ff.p1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> p"
#$cmd -in crl-f.t -inform t -outform p >crl-ff.p2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in crl-f.p -inform p -outform p >crl-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp crl-fff.p crl-f.p
if [ $? != 0 ]; then exit 1; fi
cmp crl-fff.p crl-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp crl-fff.p crl-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp crl-fff.p crl-ff.p3
if [ $? != 0 ]; then exit 1; fi

#cmp crl-f.t crl-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#cmp crl-f.t crl-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#cmp crl-f.t crl-ff.t3
#if [ $? != 0 ]; then exit 1; fi

cmp crl-f.p crl-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp crl-f.p crl-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp crl-f.p crl-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f crl-f.* crl-ff.* crl-fff.*
exit 0
