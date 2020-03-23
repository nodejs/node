// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  return arguments[1][0] === arguments[0];
}

%PrepareFunctionForOptimization(foo);
assertFalse(foo(0, 0));
assertFalse(foo(0, 0));
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo, TypeError);
