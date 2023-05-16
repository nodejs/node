// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  return x < x;
}
foo(1);
foo(2);

function bar(x) {
  foo(x);
};
%PrepareFunctionForOptimization(bar);
%OptimizeFunctionOnNextCall(bar);

assertThrows(() => bar(Symbol.toPrimitive));
