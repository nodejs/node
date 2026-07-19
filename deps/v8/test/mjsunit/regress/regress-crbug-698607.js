// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertSame(expected, found) {
  if (found === expected) {
  } else if (expected !== expected && found !== found) {
  }
}

function foo() {
  var x = {var : 0.5};
  assertSame(x, x.val);
  return () => x;
};
%PrepareFunctionForOptimization(foo);
foo(1);
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo(1);
