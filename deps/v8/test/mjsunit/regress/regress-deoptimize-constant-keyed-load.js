// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {};
o.__defineGetter__('progressChanged', function() {
  %DeoptimizeFunction(f);
  return 10;
});

function g(a, b, c) {
  return a + b + c;
}

function f() {
  var t = 'progressChanged';
  return g(1, o[t], 100);
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
assertEquals(111, f());
