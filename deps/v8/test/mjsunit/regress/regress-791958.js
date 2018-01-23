// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

obj = {m: print};
function foo() {
  for (var x = -536870912; x != -536870903; ++x) {
    obj.m(-x >= 1000000 ? x % 1000000 : y);
  }
}
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
