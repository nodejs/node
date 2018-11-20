# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: test/test262/prune-local-tests.sh
# This script removes redundant tests present in the local-tests directory
# when they are identical to upstreamed tests. It should be run as part of
# the test262 roll process.

find test/test262/local-tests -type f | while read localpath; do
  datapath=${localpath/local-tests/data}
  if [ -e $datapath ] ; then
    if diff $localpath $datapath >/dev/null ; then
      git rm $localpath || exit 1
    fi
  fi
done
