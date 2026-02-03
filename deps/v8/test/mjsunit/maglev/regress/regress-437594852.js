// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --maglev-truncation

function bar(value) {
  try { Object.getPrototypeOf(value); } catch (e) {}
}

function foo(a, b) {
  bar(a);
  const x = b & 65535;
  const y = x * 65535;
  bar(x);
  const z = y + (x * 65535);
  z >>> z;
  return 65535;
}
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo(4, -2);
%OptimizeMaglevOnNextCall(foo);
foo();
