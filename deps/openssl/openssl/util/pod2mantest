#!/bin/sh

# This script is used by test/Makefile to check whether a sane 'pod2man'
# is installed.
# ('make install' should not try to run 'pod2man' if it does not exist or if
# it is a broken 'pod2man' version that is known to cause trouble. if we find
# the system 'pod2man' to be broken, we use our own copy instead)
#
# In any case, output an appropriate command line for running (or not
# running) pod2man.


IFS=:
if test "$OSTYPE" = "msdosdjgpp"; then IFS=";"; fi

try_without_dir=true
# First we try "pod2man", then "$dir/pod2man" for each item in $PATH.
for dir in dummy${IFS}$PATH; do
    if [ "$try_without_dir" = true ]; then
      # first iteration
      pod2man=pod2man
      try_without_dir=false
    else
      # second and later iterations
      pod2man="$dir/pod2man"
      if [ ! -f "$pod2man" ]; then  # '-x' is not available on Ultrix
        pod2man=''
      fi
    fi

    if [ ! "$pod2man" = '' ]; then
        failure=none

	if "$pod2man" --section=1 --center=OpenSSL --release=dev pod2mantest.pod | fgrep OpenSSL >/dev/null; then
	    :
	else
	    failure=BasicTest
	fi

	if [ "$failure" = none ]; then
	    if "$pod2man" --section=1 --center=OpenSSL --release=dev pod2mantest.pod | grep '^MARKER - ' >/dev/null; then
	        failure=MultilineTest
	    fi
	fi


        if [ "$failure" = none ]; then
            echo "$pod2man"
            exit 0
        fi

        echo "$pod2man does not work properly ('$failure' failed).  Looking for another pod2man ..." >&2
    fi
done

echo "No working pod2man found.  Consider installing a new version." >&2
echo "As a workaround, we'll use a bundled old copy of pod2man.pl." >&2
echo "$1 ../../util/pod2man.pl"
