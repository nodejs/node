// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --typed-array-length-loading
// Flags: --mock-arraybuffer-allocator

// Using --mock-arraybuffer-allocator makes the big array buffer allocation
// always succeed. This test doesn't access the memory, so it's fine.

function maybeThrow(maybe) {
  if (maybe) { throw 'lol'; }
}
%NeverOptimizeFunction(maybeThrow);

function foo(size, throwEarly) {
  let a = new Uint8Array(size);
  let b = a.length;
  try {
    maybeThrow(throwEarly);
    b = 42;
    maybeThrow(!throwEarly);
  } catch(e) {
    return b;
  }
  assertUnreachable();
}
%PrepareFunctionForOptimization(foo);

foo(100, true);
foo(100, false);

%OptimizeFunctionOnNextCall(foo);
assertEquals(100, foo(100, true));
assertEquals(42, foo(100, false));
assertOptimized(foo);

// Also large JSTypedArray lengths are supported.
if (%Is64Bit()) {
  const largeLength = 8589934592;
  assertEquals(largeLength, foo(largeLength, true));
  assertEquals(42, foo(largeLength, false));
  assertOptimized(foo);
}
