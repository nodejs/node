#!/bin/sh
# print out the hash values 
#

for i in $*
do
	h=`openssl x509 -hash -noout -in $i`
	echo "$h.0 => $i"
done
