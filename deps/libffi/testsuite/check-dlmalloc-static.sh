#!/bin/sh
# Check that closures.c keeps the bundled dlmalloc private.
#
# closures.c declares each dl* function static before it includes
# dlmalloc.c.  A dl* function missing from that list gets dlmalloc's
# default extern linkage.  The version script hides it in libffi.so.
# libffi.a has no version script, so a program that links libffi.a
# exports it.
#
# Usage: check-dlmalloc-static.sh NM LIBTOOL-OBJECT

nm_cmd=$1
lo=$2

case $nm_cmd in
  *dumpbin*)
    echo "SKIP: $nm_cmd does not print nm-style output"
    exit 0
    ;;
esac

pic_object=none
non_pic_object=none
. "./$lo"
dir=`dirname "$lo"`

status=0
checked=0
for obj in "$pic_object" "$non_pic_object"; do
  test "$obj" = none && continue
  checked=1
  leaked=`$nm_cmd "$dir/$obj" | awk 'NF >= 3 && $2 ~ /^[A-TV-Z]$/ { print $3 }' | grep '^_*dl'`
  if test -n "$leaked"; then
    echo "FAIL: $dir/$obj defines these dlmalloc functions as external symbols:"
    echo "$leaked" | sed 's/^/  /'
    echo "Declare them static in src/closures.c, before it includes dlmalloc.c."
    status=1
  fi
done

if test $checked = 0; then
  echo "FAIL: $lo names no object to check"
  exit 1
fi
test $status = 0 && echo "PASS: no dlmalloc function in $lo is external"
exit $status
