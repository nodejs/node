// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators --allow-natives-syntax

(function TestAutoAccessorOneToManyWithDisabledOptimization() {
  function makeClass() {
    class C {
      accessor x = 1;
    }
    return C;
  }

  const C1 = makeClass();
  const c1 = new C1();
  const descriptor = Object.getOwnPropertyDescriptor(C1.prototype, 'x');
  const getter = descriptor.get;
  const setter = descriptor.set;

  %PrepareFunctionForOptimization(getter);
  %PrepareFunctionForOptimization(setter);
  assertEquals(1, c1.x);
  c1.x = 2;
  assertEquals(2, c1.x);
  %OptimizeFunctionOnNextCall(getter);
  %OptimizeFunctionOnNextCall(setter);
  assertEquals(2, c1.x);
  c1.x = 3;
  assertEquals(3, c1.x);

  %NeverOptimizeFunction(getter);
  %NeverOptimizeFunction(setter);

  const C2 = makeClass();
  const c2 = new C2();
  assertEquals(1, c2.x);
  c2.x = 4;
  assertEquals(4, c2.x);
})();

(function TestClosureOneToManyWithDisabledOptimization() {
  function makeClosure(v) {
    return function() {
      return v;
    };
  }

  const f1 = makeClosure(10);
  %PrepareFunctionForOptimization(f1);
  assertEquals(10, f1());
  %OptimizeFunctionOnNextCall(f1);
  assertEquals(10, f1());

  %NeverOptimizeFunction(f1);

  const f2 = makeClosure(20);
  assertEquals(10, f1());
  assertEquals(20, f2());
})();

(function TestEmptyFeedbackVectorPgoWithDisabledOptimization() {
  function makeOuter(v) {
    return function makeInner() {
      return function emptyFb() {
        return v;
      };
    };
  }

  const inner1 = makeOuter(1)();
  for (let i = 0; i < 5000; i++) {
    assertEquals(1, inner1());
  }
  %NeverOptimizeFunction(inner1);

  const inner2 = makeOuter(2)();
  assertEquals(2, inner2());
})();
