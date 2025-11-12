// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  var v = x;
  for (i = 0; i < 1; i++) {
    v.apply(this, arguments);
  }
};
%PrepareFunctionForOptimization(f);
function g() {}

f(g);
f(g);
%OptimizeFunctionOnNextCall(f);
assertThrows(function() {
  f('----');
}, TypeError);
