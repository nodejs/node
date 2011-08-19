#!/bin/sh
#
# print the subject
#

for i in $*
do
	n=`openssl x509 -subject -issuer -enddate -noout -in $i`
	echo "$i"
	echo "$n"
	echo "--------"
done
