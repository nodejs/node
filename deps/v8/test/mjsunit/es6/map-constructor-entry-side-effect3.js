// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestMapConstructorEntrySideEffect(ctor) {
  const k1 = {};
  const k2 = {};
  const k3 = {};
  const input = [
    Object.defineProperty([, 1], "0", {
      get() {
        // Verify continuation accesses properly accesses subsequent entries
        Object.defineProperty(input, "1", {
          get: () => [k3, 3]
        });
        return k1;
      }
    }),
    [k2, 2]
  ];
  const col = new ctor(input);

  if ('size' in col) assertEquals(2, col.size);
  assertTrue(col.has(k1));
  assertFalse(col.has(k2));
  assertTrue(col.has(k3));
}

TestMapConstructorEntrySideEffect(Map);
TestMapConstructorEntrySideEffect(Map);
TestMapConstructorEntrySideEffect(Map);
%OptimizeFunctionOnNextCall(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(Map);
assertOptimized(TestMapConstructorEntrySideEffect);

TestMapConstructorEntrySideEffect(WeakMap);
TestMapConstructorEntrySideEffect(WeakMap);
TestMapConstructorEntrySideEffect(WeakMap);
%OptimizeFunctionOnNextCall(TestMapConstructorEntrySideEffect);
TestMapConstructorEntrySideEffect(WeakMap);
assertOptimized(TestMapConstructorEntrySideEffect);
