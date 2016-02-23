#!/bin/sh

if [ -f /etc/faketimerc ] ; then
	echo "Running the test program with your system-wide default in /etc/faketimerc"
	echo "\$ LD_PRELOAD=../src/libfaketime.so.1 ./timetest"
	LD_PRELOAD=../src/libfaketime.so.1 ./timetest
	echo
else
	echo "Running the test program with no faked time specified"
	echo "\$ LD_PRELOAD=../src/libfaketime.so.1 ./timetest"
	LD_PRELOAD=../src/libfaketime.so.1 ./timetest
	echo
fi

echo "============================================================================="
echo

echo "Running the test program with absolute date 2003-01-01 10:00:05 specified"
echo "\$ LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"2003-01-01 10:00:05\" ./timetest"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="2003-01-01 10:00:05" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with START date @2005-03-29 14:14:14 specified"
echo "\$ LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"@2005-03-29 14:14:14\" ./timetest"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="@2005-03-29 14:14:14" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified"
echo "LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"-10d\" ./timetest"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="-10d" ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days negative offset specified, and FAKE_STAT disabled"
echo "\$ LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"-10d\" NO_FAKE_STAT=1 ./timetest"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="-10d" NO_FAKE_STAT=1 ./timetest
echo

echo "============================================================================="
echo

echo "Running the test program with 10 days postive offset specified, and sped up 2 times"
echo "\$ LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"+10d x2\" ./timetest"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="+10d x2" NO_FAKE_STAT=1 ./timetest
echo

echo "============================================================================="
echo

echo "Running the 'date' command with 15 days negative offset specified"
echo "\$ LD_PRELOAD=../src/libfaketime.so.1 FAKETIME=\"-15d\" date"
LD_PRELOAD=../src/libfaketime.so.1 FAKETIME="-15d" date
echo

echo "============================================================================="
echo "Testing finished."

exit 0
