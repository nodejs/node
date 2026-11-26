// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

Object.prototype.customAt = Array.prototype.at;

// BigInt receiver mixed with fast JSArray receiver. TurboFan must bail out of
// reducing Array.prototype.at when a receiver map does not support fast array
// iteration, rather than emitting an unguarded JSObject elements load whose
// non-object BigInt digit at offset 8 could be spilled as a tagged pointer
// across a GC-triggering operation such as String.prototype.indexOf.
(() => {
  function test(receiver, str) {
    let idx = 0;
    const val = receiver.customAt(0, idx = str.indexOf("needle"));
    return [val, idx];
  }

  %PrepareFunctionForOptimization(test);
  assertEquals([42, -1], test([42], "abc"));
  assertEquals([undefined, -1], test(0x11223344n, "abc"));

  %OptimizeFunctionOnNextCall(test);
  assertEquals([42, -1], test([42], "abc"));

  const half = "a".repeat(30000);
  for (let i = 0; i < 200; i++) {
    assertEquals([undefined, -1], test(0xabcd1235n, half + half));
  }
})();

// Non-Smi index on non-fast-array receiver should stay optimized and return the
// expected value instead of failing a premature CheckSmi.
(() => {
  const obj = {0: 99, length: 1};

  function test(receiver, idx) {
    return receiver.customAt(idx);
  }

  %PrepareFunctionForOptimization(test);
  assertEquals(42, test([42], 0));
  assertEquals(99, test(obj, 0));

  %OptimizeFunctionOnNextCall(test);
  assertEquals(42, test([42], 0));
  assertEquals(99, test(obj, "0"));
  assertOptimized(test);
})();
