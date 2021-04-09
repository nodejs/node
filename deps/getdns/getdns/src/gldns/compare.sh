#!/bin/sh

# Meant to be run from this directory
rm -fr gldns
mkdir gldns
svn co https://github.com/NLnetLabs/unbound/trunk/sldns/
mv gbuffer.h sbuffer.h
mv gbuffer.c sbuffer.c
for f in sldns/*.[ch]
do
#sed -e 's/sldns_/gldns_/g' -e 's/LDNS_/GLDNS_/g' -e 's/include "ldns/include "gldns/g' -e 's/<ldns\/rrdef\.h>/"gldns\/rrdef.h"/g' -e 's/sbuffer\.h/gbuffer.h/g' ${f#sldns/} > gldns/${f#sldns/}
	sed -e 's/gldns_/sldns_/g' \
	    -e 's/GLDNS_/LDNS_/g' \
	    -e 's/<gldns\/rrdef\.h>/<sldns\/rrdef.h>/g' \
	    -e 's/include "gldns/include "sldns/g' \
	    -e 's/gbuffer\.h/sbuffer.h/g' \
	    ${f#sldns/} > gldns/${f#sldns/}
done
mv sbuffer.h gbuffer.h
mv sbuffer.c gbuffer.c

