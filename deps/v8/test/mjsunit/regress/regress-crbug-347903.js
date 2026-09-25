// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function f() {
  var a = new Array(84632);
  // Allocation folding will bail out trying to fold the elements alloc of
  // array "b."
  var b = new Array(84632);
  var c = new Array(84632);
  return [a, b, c];
};
%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
for (var i = 0; i < 10; i++) {
  f();
}
