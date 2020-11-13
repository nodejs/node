// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --logfile='+' --log --log-code --no-stress-opt

function testFunctionWithFunnyName(o) {
  return o.a;
}

(function testLoopWithFunnyName() {
  const o = {a:1};
  let result = 0;
  for (let i = 0; i < 1000; i++) {
    result += testFunctionWithFunnyName(o);
  }
})();

const log = d8.log.getAndStop();

// Check that we have a minimally working log file.
assertTrue(log.length > 0);
assertTrue(log.indexOf('v8-version') == 0);
assertTrue(log.indexOf('testFunctionWithFunnyName') >= 10);
assertTrue(log.indexOf('testLoopWithFunnyName') >= 10);
