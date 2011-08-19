#!/bin/sh
#
# print out the issuer
#

for i in $*
do
	n=`openssl x509 -issuer -noout -in $i`
	echo "$i	$n"
done
