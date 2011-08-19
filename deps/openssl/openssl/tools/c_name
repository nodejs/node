#!/bin/sh
#
# print the subject
#

for i in $*
do
	n=`openssl x509 -subject -noout -in $i`
	echo "$i	$n"
done
