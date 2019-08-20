// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestMapConstructorEntrySideEffect(ctor) {
  const originalPrototypeSet = ctor.prototype.set;
  const k1 = {};
  const k2 = {};
  let callCount = 0;
  const input = [
    Object.defineProperty([, 1], "0", {
      get() {
        // Verify continuation retains original set function
        ctor.prototype.set = () => {
          callCount++;
        };
        return k1;
      }
    }),
    [k2, 2]
  ];
  const col = new ctor(input);

  assertEquals(0, callCount);
  if ('size' in col) assertEquals(2, col.size);
  assertTrue(col.has(k1));
  assertTrue(col.has(k2));

  const col2 = new ctor(input);

  assertEquals(2, callCount);
  if ('size' in col) assertEquals(0, col2.size);
  assertFalse(col2.has(k1));
  assertFalse(col2.has(k2));

  ctor.prototype.set = originalPrototypeSet;
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
