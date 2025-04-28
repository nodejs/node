// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --typed-array-length-loading

function foo(size) {
  let a = new Uint8Array(size);
  return a.length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(100, foo(100));

%OptimizeMaglevOnNextCall(foo);
const val1 = foo(100);
assertEquals(100, val1);
assertTrue(isMaglevved(foo));

Uint8Array.prototype.__proto__ = { get length() { return 3; }};

assertFalse(isMaglevved(foo));

const val2 = foo(100);
assertEquals(3, val2);
