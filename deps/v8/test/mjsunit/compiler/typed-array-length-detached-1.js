// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

const ta = new Uint8Array(128);
function foo(a) {
  return a.length;
}
%PrepareFunctionForOptimization(foo);

foo(ta);

%OptimizeFunctionOnNextCall(foo);
assertEquals(128, foo(ta));
assertOptimized(foo);

ta.buffer.transfer();
assertUnoptimized(foo);

assertEquals(0, foo(ta));
