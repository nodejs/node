// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

async function f(v) {
  return await Promise.resolve(v);
}
%PrepareFunctionForOptimization(f);
assertPromiseResult(f(1), r1 => {
  assertEquals(1, r1);
  %OptimizeFunctionOnNextCall(f);
  assertPromiseResult(f(2), r2 => {
    assertEquals(2, r2);
    assertOptimized(f);

    Object.defineProperty(Promise, Symbol.species, {value: Promise});
    assertUnoptimized(f);

    assertPromiseResult(f(3), r3 => {
      assertEquals(3, r3);
      %PrepareFunctionForOptimization(f);
      assertPromiseResult(f(4), r4 => {
        assertEquals(4, r4);
        %OptimizeFunctionOnNextCall(f);
        assertPromiseResult(f(5), r5 => {
          assertEquals(5, r5);
          assertOptimized(f);
        });
      });
    });
  });
});
