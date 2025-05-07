// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --typed-array-length-loading

let scriptContextSlot = 0;

function foo(size) {
  let a = new Uint8Array(size);
  scriptContextSlot = a.length;
}
%PrepareFunctionForOptimization(foo);

if (%Is64Bit()) {
  foo(100);
  const largeLength = 8589934592;
  foo(largeLength);

  %OptimizeMaglevOnNextCall(foo);
  foo(100);
  assertEquals(100, scriptContextSlot);
  assertTrue(isMaglevved(foo));

  foo(largeLength);
  assertEquals(largeLength, scriptContextSlot);
  assertTrue(isMaglevved(foo));
}
