// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

const ta = new Uint8Array();
ta.__proto__ = { get length() { return 3; }};

function foo() {
  return ta.length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(3, foo());

%OptimizeFunctionOnNextCall(foo);
const val = foo();
assertEquals(3, val);
assertOptimized(foo);
