// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

function f(v) {
  %DeoptimizeFunction(test);
  return 153;
}

function test() {
  var o = {};
  var q = "q";
  o.__defineSetter__(q, f);
  assertEquals(1, o[q] = 1);
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
