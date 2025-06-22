// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --typed-array-length-loading

const ta = new Uint8Array(128);
function foo(a) {
  return a.length;
}
%PrepareFunctionForOptimization(foo);

foo(ta);

%OptimizeMaglevOnNextCall(foo);
assertEquals(128, foo(ta));
assertTrue(isMaglevved(foo));

ta.buffer.transfer();
assertFalse(isMaglevved(foo));

assertEquals(0, foo(ta));
