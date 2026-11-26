// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let shouldThrow = false;
function getArg() {
  if (shouldThrow) throw new Error();
  return 0;
}
%NeverOptimizeFunction(getArg);

// Case 1: Monomorphic CallIC feedback for a different generator (`otherGen`).
// When inlined into `targetGen1`'s default parameter initializer,
// `BuildCheckValueByReference` narrows `<closure>` in `KnownNodeAspects` to
// `otherGen` before `_CreateJSGeneratorObject` is lowered.
(function testGeneratorMismatch() {
  function* otherGen() {}
  otherGen();

  function callWithGeneratorFeedback(fn) {
    return fn(getArg());
  }
  %PrepareFunctionForOptimization(callWithGeneratorFeedback);
  callWithGeneratorFeedback(otherGen);
  callWithGeneratorFeedback(otherGen);

  function makeGenerator() {
    // Default parameter initialization runs in bytecode before
    // `_CreateJSGeneratorObject`. Passing `targetGen1` (`<closure>`) into the
    // inlined helper pins `<closure>` to `otherGen` in the unreachable path
    // after `CheckValue`.
    return function* targetGen1(
        param = callWithGeneratorFeedback(targetGen1)) {
      yield param;
    };
  }

  // Create two closures so the FeedbackCell has ManyClosures and `<closure>` is
  // not a constant prior to the inlined helper call.
  const gen1 = makeGenerator();
  const gen2 = makeGenerator();
  %PrepareFunctionForOptimization(gen1);

  // Inline `targetGen1()` with no arguments so the default parameter branch is
  // taken unconditionally and dominates `_CreateJSGeneratorObject`.
  function runGenerator1(fn) {
    return fn();
  }
  %PrepareFunctionForOptimization(runGenerator1);

  // Throw inside `getArg()` during warmup so `callWithGeneratorFeedback`'s
  // CallIC feedback stays monomorphic on `otherGen`.
  shouldThrow = true;
  assertThrows(() => runGenerator1(gen1), Error);
  assertThrows(() => runGenerator1(gen2), Error);

  %OptimizeFunctionOnNextCall(runGenerator1);
  assertThrows(() => runGenerator1(gen1), Error);
})();

// Case 2: Monomorphic Construct feedback for a non-generator class constructor
// (`NonGenerator`) whose initial map is a smaller JSObject map.
(function testNonGeneratorConstructorMismatch() {
  class NonGenerator {}
  new NonGenerator();

  shouldThrow = false;
  function constructWithClassFeedback(ctor) {
    return new ctor(getArg());
  }
  %PrepareFunctionForOptimization(constructWithClassFeedback);
  constructWithClassFeedback(NonGenerator);
  constructWithClassFeedback(NonGenerator);

  function makeGenerator() {
    return function* targetGen2(
        param = constructWithClassFeedback(targetGen2)) {
      yield param;
    };
  }

  const gen1 = makeGenerator();
  const gen2 = makeGenerator();
  %PrepareFunctionForOptimization(gen1);

  function runGenerator2(fn) {
    return fn();
  }
  %PrepareFunctionForOptimization(runGenerator2);

  shouldThrow = true;
  assertThrows(() => runGenerator2(gen1), Error);
  assertThrows(() => runGenerator2(gen2), Error);

  %OptimizeFunctionOnNextCall(runGenerator2);
  assertThrows(() => runGenerator2(gen1), Error);
})();
