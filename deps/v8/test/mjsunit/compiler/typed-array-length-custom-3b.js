// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

const ta = new Uint8Array(100);

function foo() {
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(100, foo());

%OptimizeFunctionOnNextCall(foo);
const val1 = foo();
assertEquals(100, val1);
assertOptimized(foo);

ta.__proto__ = { get length() { return 3; }};

assertUnoptimized(foo);

const val2 = foo();
assertEquals(3, val2);
