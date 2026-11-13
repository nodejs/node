// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

// Negative rejection tests (outer context lookups, super, etc.) live in
// FeedbackVectorTest.CopyFunctionWithFreshFeedback.

var globalVar = 10;
function globalHelper(x) {
  return x + globalVar;
}

// Fresh IC and embedded feedback on repeated copies of the same function.
(function TestFreshFeedback() {
  function addAndLoad(o, a, b) {
    return o.x + (a + b) + globalHelper(1);
  }

  const copy1 = copyFunctionWithFreshFeedback(addAndLoad);
  const copy2 = copyFunctionWithFreshFeedback(addAndLoad);
  assertNotSame(addAndLoad, copy1);
  assertNotSame(copy1, copy2);

  %PrepareFunctionForOptimization(copy1);
  assertEquals(15, copy1({x: 1}, 1, 2));
  assertEquals(15, copy1({x: 1, y: 2}, 1, 2));
  %OptimizeFunctionOnNextCall(copy1);
  assertEquals(15, copy1({x: 1}, 1, 2));

  // `copy2` starts with clean feedback and can specialize monomorphically on a
  // different shape and string addition.
  %PrepareFunctionForOptimization(copy2);
  assertEquals("ab11", copy2({x: "a"}, "b", ""));
  %OptimizeFunctionOnNextCall(copy2);
  assertEquals("ab11", copy2({x: "a"}, "b", ""));
  assertOptimized(copy2);
})();

// Preserves strict mode inherited from an enclosing scope even when the copied
// function body has no `"use strict"` directive of its own.
(function TestPreservesInheritedStrictMode() {
  "use strict";
  function getReceiver() {
    return this;
  }
  const copy = copyFunctionWithFreshFeedback(getReceiver);
  assertEquals(undefined, copy());
})();

// Allows local FunctionContext, nested BlockContexts, CatchContexts, early
// returns out of try/finally, generators, and inner classes with private
// fields, as long as no context outside the copied function is accessed.
(function TestAllowedLocalContexts() {
  let unusedOuterLet = 99;
  assertEquals(99, unusedOuterLet);

  function complexLocalContexts(x) {
    let captured = x * 2;
    const inner = () => captured;
    {
      let blockScoped = 3;
      const readBlock = () => captured + blockScoped;
      try {
        throw 4;
      } catch (e) {
        const readCatch = () => readBlock() + e;
        captured = readCatch();
      }
    }
    for (let i = 0; i < 2; ++i) {
      try {
        let inTry = i + 1;
        const readTry = () => inTry;
        if (i === 1) {
          class InnerClass {
            #secret = captured + readTry();
            get() {
              return this.#secret;
            }
          }
          return new InnerClass().get() + inner();
        }
      } finally {
        captured += 1;
      }
    }
    return -1;
  }

  const copy = copyFunctionWithFreshFeedback(complexLocalContexts);
  assertEquals(complexLocalContexts(5), copy(5));

  function* generatorWithLocalContext(seed) {
    let state = seed;
    const bump = () => ++state;
    yield bump();
    try {
      let step = 10;
      const addStep = () => state + step;
      yield addStep();
      throw 3;
    } catch (e) {
      const getErr = () => e;
      yield getErr();
    }
  }

  const genCopy = copyFunctionWithFreshFeedback(generatorWithLocalContext);
  const it = genCopy(5);
  assertEquals({value: 6, done: false}, it.next());
  assertEquals({value: 16, done: false}, it.next());
  assertEquals({value: 3, done: false}, it.next());
  assertEquals({value: undefined, done: true}, it.next());
})();

// Declared at script scope so inner sloppy eval does not force an enclosing
// FunctionContext on a wrapper (which JS treats as a reachable outer scope).
function sloppyEvalGlobal() {
  eval("");
  return globalVar;
}
(function TestLocalSloppyEvalGlobalLoad() {
  const evalCopy = copyFunctionWithFreshFeedback(sloppyEvalGlobal);
  assertEquals(10, evalCopy());
})();
