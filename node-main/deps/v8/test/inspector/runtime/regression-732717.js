// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/732717');

Protocol.Runtime.evaluate({expression: `var v3 = {};
var v6 = {};
Array.prototype.__defineGetter__(0, function() {
  this[0] = 2147483647;
})
Array.prototype.__defineSetter__(0, function() {
console.context(v3);
this[0] = v6;
});
v60 = Array(0x8000).join();`}).then(InspectorTest.completeTest);
