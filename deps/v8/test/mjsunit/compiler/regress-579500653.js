// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function testDirectGeneratorNext() {
  function* myGen() {
    throw new Error("boom");
  }

  function run(gen) {
    return gen.next();
  }

  %PrepareFunctionForOptimization(run);
  assertThrows(() => run(myGen()), Error, "boom");
  assertThrows(() => run(myGen()), Error, "boom");
  %OptimizeFunctionOnNextCall(run);

  try {
    run(myGen());
    assertUnreachable();
  } catch (e) {
    assertTrue(e.stack.includes("myGen.next"));
  }
})();

(function testDelegatingGeneratorNext() {
  function* innerGen() {
    throw new Error("boom");
  }

  function* outerGen() {
    return yield* innerGen();
  }

  function run(gen) {
    let r = gen.next();
    while (!r.done) r = gen.next(r.value);
    return r.value;
  }

  %PrepareFunctionForOptimization(run);
  assertThrows(() => run(outerGen()), Error, "boom");
  assertThrows(() => run(outerGen()), Error, "boom");
  %OptimizeFunctionOnNextCall(run);

  try {
    run(outerGen());
    assertUnreachable();
  } catch (e) {
    assertTrue(e.stack.includes("innerGen.next"));
    assertTrue(e.stack.includes("outerGen.next"));
  }
})();
