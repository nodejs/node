// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestMapConstructorEntrySideEffect(ctor) {
  const k1 = {};
  const k2 = {};
  const k3 = {};
  let firstEntryCallCount = 0;
  let lastEntryCallCount = 0;
  const input = [
    Object.defineProperty([, 1], "0", {
      get() {
        // Verify handling of a non-Smi array length
        input.length = 2 ** 32 - 2;
        firstEntryCallCount++;
        return k1;
      }
    }),
    [k2, 2],
    Object.defineProperty([k3, ], "1", {
      get() {
        input.length = 1;
        lastEntryCallCount++;
        return 3;
      }
    })
  ];
  const col = new ctor(input);

  assertEquals(1, firstEntryCallCount,);
  assertEquals(1, lastEntryCallCount);
  if ('size' in col) assertEquals(3, col.size);
  assertEquals(1, col.get(k1));
  assertEquals(2, col.get(k2));
  assertEquals(3, col.get(k3));
}

%PrepareFunctionForOptimization(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(Map);
TestMapConstructorEntrySideEffect(Map);
TestMapConstructorEntrySideEffect(Map);
%OptimizeFunctionOnNextCall(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(Map);
assertOptimized(TestMapConstructorEntrySideEffect);

// This call would deopt
TestMapConstructorEntrySideEffect(WeakMap);
%PrepareFunctionForOptimization(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(WeakMap);
TestMapConstructorEntrySideEffect(WeakMap);
%OptimizeFunctionOnNextCall(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(WeakMap);
assertOptimized(TestMapConstructorEntrySideEffect);
