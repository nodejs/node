// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-enable-debug-features

// Ensure we could reduce reference equal with boolean input.
// When the inputs are all boolean with true constant, we could
// reduce to the input. And when the inputs are all boolean with
// false constant, we could reduce to the input with BooleanNot.
function foo(x, y) {
  const v = (x === y);
  %TurbofanStaticAssert(((v === true) === v));
  %TurbofanStaticAssert((!(v === false) === v));
};
%PrepareFunctionForOptimization(foo);
foo(1, 2);
foo(2, 3);
%OptimizeFunctionOnNextCall(foo);
foo(3, 4);
