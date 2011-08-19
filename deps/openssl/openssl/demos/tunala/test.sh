#!/bin/sh

HTTP="localhost:8080"
CLIENT_PORT="9020"
SERVER_PORT="9021"

sub_test ()
{
	echo "STARTING - $VER $CIPHER"
	./tunala -listen localhost:$CLIENT_PORT -proxy localhost:$SERVER_PORT \
		-cacert CA.pem -cert A-client.pem -server 0 \
		-dh_special standard -v_peer -v_strict \
		$VER -cipher $CIPHER 1> tc1.txt 2> tc2.txt &
	./tunala -listen localhost:$SERVER_PORT -proxy $HTTP \
		-cacert CA.pem -cert A-server.pem -server 1 \
		-dh_special standard -v_peer -v_strict \
		$VER -cipher $CIPHER 1> ts1.txt 2> ts2.txt &
	# Wait for the servers to be listening before starting the wget test
	DONE="no"
	while [ "$DONE" != "yes" ]; do
		L1=`netstat -a | egrep "LISTEN[\t ]*$" | grep ":$CLIENT_PORT"`
		L2=`netstat -a | egrep "LISTEN[\t ]*$" | grep ":$SERVER_PORT"`
		if [ "x$L1" != "x" ]; then
			DONE="yes"
		elif [ "x$L2" != "x" ]; then
			DONE="yes"
		else
			sleep 1
		fi
	done
	HTML=`wget -O - -T 1 http://localhost:$CLIENT_PORT 2> /dev/null | grep "<HTML>"`
	if [ "x$HTML" != "x" ]; then
		echo "OK - $CIPHER ($VER)"
	else
		echo "FAIL - $CIPHER ($VER)"
		killall tunala
		exit 1
	fi
	killall tunala
	# Wait for the servers to stop before returning - otherwise the next
	# test my fail to start ... (fscking race conditions)
	DONE="yes"
	while [ "$DONE" != "no" ]; do
		L1=`netstat -a | egrep "LISTEN[\t ]*$" | grep ":$CLIENT_PORT"`
		L2=`netstat -a | egrep "LISTEN[\t ]*$" | grep ":$SERVER_PORT"`
		if [ "x$L1" != "x" ]; then
			DONE="yes"
		elif [ "x$L2" != "x" ]; then
			DONE="yes"
		else
			DONE="no"
		fi
	done
	exit 0
}

run_test ()
{
	(sub_test 1> /dev/null) || exit 1
}

run_ssl_test ()
{
killall tunala 1> /dev/null 2> /dev/null
echo ""
echo "Starting all $PRETTY tests"
if [ "$PRETTY" != "SSLv2" ]; then
	if [ "$PRETTY" != "SSLv3" ]; then
		export VER="-no_ssl2 -no_ssl3"
		export OSSL="-tls1"
	else
		export VER="-no_ssl2 -no_tls1"
		export OSSL="-ssl3"
	fi
else
	export VER="-no_ssl3 -no_tls1"
	export OSSL="-ssl2"
fi
LIST="`../../apps/openssl ciphers $OSSL | sed -e 's/:/ /g'`"
#echo "$LIST"
for i in $LIST; do \
	DSS=`echo "$i" | grep "DSS"`
	if [ "x$DSS" != "x" ]; then
		echo "---- skipping $i (no DSA cert/keys) ----"
	else
		export CIPHER=$i
		run_test
		echo "SUCCESS: $i"
	fi
done;
}

# Welcome the user
echo "Tests will assume an http server running at $HTTP"

# TLSv1 test
export PRETTY="TLSv1"
run_ssl_test

# SSLv3 test
export PRETTY="SSLv3"
run_ssl_test

# SSLv2 test
export PRETTY="SSLv2"
run_ssl_test

