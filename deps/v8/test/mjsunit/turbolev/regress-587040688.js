// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function id(x) {
  return x;
}
%PrepareFunctionForOptimization(id);
%DisableEagerInlining(id);

(function TestPostLoopSmiUses() {
  const arr = [1, 2];
  const a = {s: 0.25};
  const b = {t: 3.5};

  function inner(exit_early, cond) {
    let flag = id(exit_early);
    let p = cond ? 5 : a.s;
    let z;
    for (let i = 0;; i++) {
      z = p | 0;
      if (flag) {
        +p;
        break;
      }
      if (i > 1) return -1;
      p = b.t;
    }
    arr[0] = p;
    return p + z;
  }
  %PrepareFunctionForOptimization(inner);

  function outer(cond) {
    return inner(true, cond);
  }
  %PrepareFunctionForOptimization(outer);

  id(true);
  id(false);
  inner(true, true);
  inner(false, false);
  assertEquals(10, outer(true));

  %OptimizeFunctionOnNextCall(outer);
  assertEquals(0.25, outer(false));
  %HeapObjectVerify(arr);
})();

(function TestDeoptFrameUse() {
  const b = {t: 3.5};

  function inner(exit_early, cond) {
    let flag = id(exit_early);
    let p = cond ? 5 : undefined;
    let z;
    let t;
    for (let i = 0;; i++) {
      z = p | 0;
      if (flag) {
        t = typeof p;
        break;
      }
      if (i > 1) return 'bail';
      p = b.t;
    }
    return t + z;
  }
  %PrepareFunctionForOptimization(inner);

  function outer(cond) {
    return inner(true, cond);
  }
  %PrepareFunctionForOptimization(outer);

  id(true);
  id(false);
  inner(true, true);
  inner(false, false);
  assertEquals('number5', outer(true));

  %OptimizeFunctionOnNextCall(outer);
  assertEquals('undefined0', outer(false));
})();

(function TestPostLoopHeapNumberSmiValue() {
  const arr = [1, 2];
  const b = {t: 3.5};

  function makeHeapNumber5(x) {
    return x + 0.5;
  }
  %NeverOptimizeFunction(makeHeapNumber5);

  function inner(exit_early, cond, x) {
    +x;
    let flag = id(exit_early);
    let p = cond ? 5 : x;
    let z;
    for (let i = 0;; i++) {
      z = p + 0.5;
      if (flag) {
        +p;
        break;
      }
      if (i > 1) return -1;
      p = b.t;
    }
    arr[0] = p;
    return (p + 1) + z;
  }
  %PrepareFunctionForOptimization(inner);

  function outer(cond, x) {
    return inner(true, cond, x);
  }
  %PrepareFunctionForOptimization(outer);

  id(true);
  id(false);
  inner(true, true, 1);
  inner(false, false, 1.5);
  assertEquals(11.5, outer(true, 1));

  %OptimizeFunctionOnNextCall(outer);
  assertEquals(11.5, outer(false, makeHeapNumber5(4.5)));
  %HeapObjectVerify(arr);
})();
