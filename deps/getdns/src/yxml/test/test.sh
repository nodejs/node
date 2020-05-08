#!/bin/sh

for i in *.xml; do
  b=`basename $i .xml`
  o=${b}.out
  t=${b}.test
  ./test <$i >$t
  if [ -n "`diff -q $o $t`" ]; then
    echo "Test failed for $i:"
    diff -u $o $t
    exit 1
  fi
done

echo "All tests completed successfully."
