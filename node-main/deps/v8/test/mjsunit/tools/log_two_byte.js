// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --logfile='+' --log --log-code --log-function-events
// Flags: --no-stress-background-compile

let twoByteName = "twoByteName_🍕"
let o = {
  [twoByteName](obj) {
    return obj.a
  }
}
function testFunctionWithFunnyName(o) {
  return o.a;
}

(function testLoopWithFunnyName() {
  let object = {a:1};
  let result = 0;
  for (let i = 0; i < 1000; i++) {
    result += o[twoByteName](object);
  }
})();

var __v_3 = {};
({})['foobar\u2653'] = null;
eval('__v_3 = function foobar() { return foobar };');
__v_3();

const log = d8.log.getAndStop();
// Check that we have a minimally working log file.
assertTrue(log.length > 0);
assertTrue(log.indexOf('v8-version') == 0);
assertTrue(log.indexOf('testFunctionWithFunnyName') >= 10);
assertTrue(log.indexOf("twoByteName") >= 10);
assertTrue(log.indexOf('testLoopWithFunnyName') >= 10);
