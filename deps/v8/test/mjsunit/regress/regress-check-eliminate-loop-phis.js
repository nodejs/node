// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var o = {x:1};
  var y = {y:2.5, x:0};
  var result;
  for (var i = 0; i < 2; i++) {
    result = o.x + 3;
    o = y;
  }
  return result;
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
assertEquals(3, f());
