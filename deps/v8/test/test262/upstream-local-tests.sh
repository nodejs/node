# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: test/test262/upstream-local-tests.sh
# This script takes the files which were modified in the test262 local-test
# directory (test/test262/local-tests) in the top patch of the v8 tree and
# creates a new patch in the local test262 checkout (test/test262/data).
# This patch could then hopefully be used for upstreaming tests.
# The script should be run from the top level directory of the V8 checkout.

git show $1 | grep '+++ b/test/test262/local-tests' | while read test; do
  path=${test:6}
  datapath=${path/local-tests/data}
  echo cp $path $datapath
  cp $path $datapath
  cd test/test262/data
  git add ${datapath:18} || exit 1
  cd ../../../
done
cd test/test262/data
git commit || exit 1
