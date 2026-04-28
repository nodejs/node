// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(b) {
  var x = -0;
  var y = -0x80000000;

  if (b) {
    x = -1;
    y = 1;
  }

  return (x - y) == -0x80000000;
}

%PrepareFunctionForOptimization(foo);
assertFalse(foo(true));
%OptimizeFunctionOnNextCall(foo);
assertFalse(foo(false));
