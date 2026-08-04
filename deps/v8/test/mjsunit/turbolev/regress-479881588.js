// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --maglev-assert-types

function foo() {
  let a = -536870912n;
  (a++).constructor;
}
%PrepareFunctionForOptimization(foo);

foo();
%OptimizeFunctionOnNextCall(foo);
foo();
