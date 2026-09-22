// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --js-decorators

// Test 1: Private getter accessed in try/catch before class literal
// finishes evaluating (paused at yield in computed property key).
(function TestPrivateGetterBeforeYield() {
  let gen;
  let object;
  let exported;
  function advance(resume) {
    if (resume) {
      const result = gen.next();
      object = new result.value();
    }
  }
  function* maker() {
    class C {
      get #m() { return 42; }
      [(exported = function access(receiver, resume) {
        try { receiver.#m; } catch (e) {}
        advance(resume);
        try { return object.#m; } catch (e) { return "caught"; }
      }, yield exported, "x")] = 1;
    }
    return C;
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    const C = gen.next().value;
    object = new C();
    assertEquals(42, access(object, false));
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    advance(true);
    assertEquals(42, access(object, false));
  }
  gen = maker();
  const access = gen.next().value;
  object = {};
  %PrepareFunctionForOptimization(access);
  for (let i = 0; i < 10; ++i) {
    assertEquals("caught", access({}, false));
  }
  %OptimizeFunctionOnNextCall(access);
  assertEquals("caught", access({}, false));
  assertEquals(42, access({}, true));
})();

// Test 2: Private setter accessed in try/catch before class literal
// finishes evaluating.
(function TestPrivateSetterBeforeYield() {
  let gen;
  let object;
  let exported;
  let stored = 0;
  function advance(resume) {
    if (resume) {
      const result = gen.next();
      object = new result.value();
    }
  }
  function* maker() {
    class C {
      set #m(v) { stored = v; }
      [(exported = function access(receiver, resume) {
        try { receiver.#m = 1; } catch (e) {}
        advance(resume);
        try { object.#m = 99; return stored; } catch (e) { return "caught"; }
      }, yield exported, "x")] = 1;
    }
    return C;
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    advance(true);
    assertEquals(99, access(object, false));
  }
  gen = maker();
  const access = gen.next().value;
  object = {};
  %PrepareFunctionForOptimization(access);
  for (let i = 0; i < 10; ++i) {
    assertEquals("caught", access({}, false));
  }
  %OptimizeFunctionOnNextCall(access);
  assertEquals("caught", access({}, false));
  assertEquals(99, access({}, true));
})();

// Test 3: Private auto-accessor accessed in try/catch before yield.
(function TestPrivateAutoAccessorBeforeYield() {
  let gen;
  let object;
  let exported;
  function advance(resume) {
    if (resume) {
      const result = gen.next();
      object = new result.value();
    }
  }
  function* maker() {
    class C {
      accessor #m = 10;
      [(exported = function access(receiver, resume) {
        try { receiver.#m; } catch (e) {}
        advance(resume);
        try { return object.#m; } catch (e) { return "caught"; }
      }, yield exported, "x")] = 1;
    }
    return C;
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    advance(true);
    assertEquals(10, access(object, false));
  }
  gen = maker();
  const access = gen.next().value;
  object = {};
  %PrepareFunctionForOptimization(access);
  for (let i = 0; i < 10; ++i) {
    assertEquals("caught", access({}, false));
  }
  %OptimizeFunctionOnNextCall(access);
  assertEquals("caught", access({}, false));
  assertEquals(10, access({}, true));
})();
