// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";
function f1(d) {
  return 1 + f2(1, f3(d), d);
};
%PrepareFunctionForOptimization(f1);
function f2(v0, v1, v2) {
  return v1;
}

function f3(d) {
  if (d) %DeoptimizeFunction(f1);
  return 2;
}

%NeverOptimizeFunction(f3);

f1(false);
f1(false);
%OptimizeFunctionOnNextCall(f1);
assertEquals(3, f1(true));
