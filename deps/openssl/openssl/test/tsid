#!/bin/sh

cmd='../util/shlib_wrap.sh ../apps/openssl sess_id'

if [ "$1"x != "x" ]; then
	t=$1
else
	t=testsid.pem
fi

echo testing session-id conversions
cp $t sid-fff.p

echo "p -> d"
$cmd -in sid-fff.p -inform p -outform d >sid-f.d
if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in sid-fff.p -inform p -outform t >sid-f.t
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in sid-fff.p -inform p -outform p >sid-f.p
if [ $? != 0 ]; then exit 1; fi

echo "d -> d"
$cmd -in sid-f.d -inform d -outform d >sid-ff.d1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> d"
#$cmd -in sid-f.t -inform t -outform d >sid-ff.d2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> d"
$cmd -in sid-f.p -inform p -outform d >sid-ff.d3
if [ $? != 0 ]; then exit 1; fi

#echo "d -> t"
#$cmd -in sid-f.d -inform d -outform t >sid-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#echo "t -> t"
#$cmd -in sid-f.t -inform t -outform t >sid-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#echo "p -> t"
#$cmd -in sid-f.p -inform p -outform t >sid-ff.t3
#if [ $? != 0 ]; then exit 1; fi

echo "d -> p"
$cmd -in sid-f.d -inform d -outform p >sid-ff.p1
if [ $? != 0 ]; then exit 1; fi
#echo "t -> p"
#$cmd -in sid-f.t -inform t -outform p >sid-ff.p2
#if [ $? != 0 ]; then exit 1; fi
echo "p -> p"
$cmd -in sid-f.p -inform p -outform p >sid-ff.p3
if [ $? != 0 ]; then exit 1; fi

cmp sid-fff.p sid-f.p
if [ $? != 0 ]; then exit 1; fi
cmp sid-fff.p sid-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp sid-fff.p sid-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp sid-fff.p sid-ff.p3
if [ $? != 0 ]; then exit 1; fi

#cmp sid-f.t sid-ff.t1
#if [ $? != 0 ]; then exit 1; fi
#cmp sid-f.t sid-ff.t2
#if [ $? != 0 ]; then exit 1; fi
#cmp sid-f.t sid-ff.t3
#if [ $? != 0 ]; then exit 1; fi

cmp sid-f.p sid-ff.p1
if [ $? != 0 ]; then exit 1; fi
#cmp sid-f.p sid-ff.p2
#if [ $? != 0 ]; then exit 1; fi
cmp sid-f.p sid-ff.p3
if [ $? != 0 ]; then exit 1; fi

/bin/rm -f sid-f.* sid-ff.* sid-fff.*
exit 0
