// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --sparkplug

// Tests that yielding undefined does not confuse the skip-allocation fast path
// in inlined Generator.prototype.next.

(function yieldUndefinedOnFastPath() {
  function* gen() {
    yield undefined;
    yield undefined;
  }

  function callNext(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(callNext);

  const g = gen();
  let res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);

  %OptimizeMaglevOnNextCall(callNext);
  res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);
})();

// Same as above, with the generator body in Sparkplug: covers the baseline ack.
(function yieldUndefinedOnFastPathBaselineBody() {
  function* gen() {
    yield undefined;
    yield undefined;
  }
  %CompileBaseline(gen);

  function callNext(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(callNext);

  const g = gen();
  let res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);

  %OptimizeMaglevOnNextCall(callNext);
  res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);
  assertTrue(%ActiveTierIsSparkplug(gen));
})();

// Same as above, with the generator body in Maglev: covers the Maglev ack.
(function yieldUndefinedOnFastPathMaglevBody() {
  function* gen() {
    yield undefined;
    yield undefined;
  }
  %PrepareFunctionForOptimization(gen);
  for (const _ of gen());
  %OptimizeMaglevOnNextCall(gen);

  function callNext(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(callNext);

  const g = gen();  // Maglev-compiles the body.
  let res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);

  %OptimizeMaglevOnNextCall(callNext);
  res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);
  assertOptimized(gen);
})();

// Skip path + lazy deopt: the continuation must wrap the returned value, which
// is the bare yielded value rather than a result object.
(function lazyDeoptOnFastPath() {
  function* gen() {
    yield 1;
    %DeoptimizeFunction(callNext);
    yield 42;
    yield undefined;
  }

  function callNext(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(callNext);

  const g = gen();
  callNext(g);

  %OptimizeMaglevOnNextCall(callNext);
  let res = callNext(g);
  assertUnoptimized(callNext);
  assertEquals(42, res.value);
  assertFalse(res.done);

  res = g.next();
  assertEquals(undefined, res.value);
  assertFalse(res.done);
})();

// A try-catch around the call disables the handshake, so the body allocates
// the result object itself. Yielding `undefined` must still work, including
// when the call lazy-deopts.
(function lazyDeoptWithoutHandshake() {
  function* gen() {
    yield undefined;
    %DeoptimizeFunction(callNext);
    yield undefined;
  }

  function callNext(g) {
    try {
      return g.next();
    } catch (e) {
      return e;
    }
  }
  %PrepareFunctionForOptimization(callNext);

  const g = gen();
  let res = callNext(g);
  assertEquals(undefined, res.value);
  assertFalse(res.done);

  %OptimizeMaglevOnNextCall(callNext);
  res = callNext(g);
  assertUnoptimized(callNext);
  assertEquals(undefined, res.value);
  assertFalse(res.done);

  res = g.next();
  assertEquals(undefined, res.value);
  assertTrue(res.done);
})();

// The generator returning `undefined` closes it and must be reported as done,
// never as a skipped allocation.
(function returnUndefined() {
  function* gen() {
    yield 1;
    return undefined;
  }

  function callNext(g) {
    return g.next();
  }
  %PrepareFunctionForOptimization(callNext);

  let g = gen();
  callNext(g);
  callNext(g);

  %OptimizeMaglevOnNextCall(callNext);
  g = gen();
  let res = callNext(g);
  assertEquals(1, res.value);
  assertFalse(res.done);

  res = callNext(g);
  assertEquals(undefined, res.value);
  assertTrue(res.done);

  res = callNext(g);
  assertEquals(undefined, res.value);
  assertTrue(res.done);
})();
