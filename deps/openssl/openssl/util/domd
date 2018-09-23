#!/bin/sh
# Do a makedepend, only leave out the standard headers
# Written by Ben Laurie <ben@algroup.co.uk> 19 Jan 1999

TOP=$1
shift
if [ "$1" = "-MD" ]; then
    shift
    MAKEDEPEND=$1
    shift
fi
if [ "$MAKEDEPEND" = "" ]; then MAKEDEPEND=makedepend; fi

cp Makefile Makefile.save
# fake the presence of Kerberos
touch $TOP/krb5.h
if expr "$MAKEDEPEND" : '.*gcc$' > /dev/null; then
    args=""
    while [ $# -gt 0 ]; do
	if [ "$1" != "--" ]; then args="$args $1"; fi
	shift
    done
    sed -e '/^# DO NOT DELETE.*/,$d' < Makefile > Makefile.tmp
    echo '# DO NOT DELETE THIS LINE -- make depend depends on it.' >> Makefile.tmp
    ${MAKEDEPEND} -Werror -D OPENSSL_DOING_MAKEDEPEND -M $args >> Makefile.tmp || exit 1
    ${PERL} $TOP/util/clean-depend.pl < Makefile.tmp > Makefile.new
    RC=$?
    rm -f Makefile.tmp
else
    ${MAKEDEPEND} -D OPENSSL_DOING_MAKEDEPEND $@ && \
    ${PERL} $TOP/util/clean-depend.pl < Makefile > Makefile.new
    RC=$?
fi
mv Makefile.new Makefile
# unfake the presence of Kerberos
rm $TOP/krb5.h

exit $RC
