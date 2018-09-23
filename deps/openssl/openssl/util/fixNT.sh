#!/bin/sh
#
# clean up the mess that NT makes of my source tree
#

if [ -f makefile -a ! -f Makefile ]; then
	/bin/mv makefile Makefile
fi
chmod +x Configure util/*
echo cleaning
/bin/rm -f `find . -name '*.$$$' -print` 2>/dev/null >/dev/null
echo 'removing those damn ^M'
perl -pi -e 's/\015//' `find . -type 'f' -print |grep -v '.obj$' |grep -v '.der$' |grep -v '.gz'`
make -f Makefile links
