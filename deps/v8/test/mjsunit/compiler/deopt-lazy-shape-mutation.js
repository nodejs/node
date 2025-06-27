// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

var b = false;

function change_o() {
  if (b) o = { y : 1, x : 0};
}
%NeverOptimizeFunction(change_o);

var o = { x : 1 };
function f() {
  change_o();
  return o.x;
}

%PrepareFunctionForOptimization(f);
f(); f();
%OptimizeFunctionOnNextCall(f);
b = true;
f();
