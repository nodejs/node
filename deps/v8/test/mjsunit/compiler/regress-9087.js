// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function constructor() {}
const obj = Object.create(constructor.prototype);

for (let i = 0; i < 1020; ++i) {
  constructor.prototype["x" + i] = 42;
}

function foo() {
  return obj instanceof constructor;
};
%PrepareFunctionForOptimization(foo);
assertTrue(foo());
assertTrue(foo());
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo());
