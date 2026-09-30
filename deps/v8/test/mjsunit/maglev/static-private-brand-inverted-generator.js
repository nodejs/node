// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test static private method call when brand check runs before class constructor
// initialization finishes (paused at yield in computed property key).
(function TestStaticPrivateMethodBeforeYield() {
  let gen;
  let cls;
  let exported;
  function advance(resume) {
    if (resume) {
      cls = gen.next().value;
    }
  }
  function* maker() {
    class C {
      static #m() { return 42; }
      [(exported = function access(receiver, resume) {
        try { receiver.#m(); } catch (e) {}
        advance(resume);
        try { return cls.#m(); } catch (e) { return "caught"; }
      }, yield exported, "x")] = 1;
    }
    return C;
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    advance(true);
    assertEquals(42, access(cls, false));
  }
  gen = maker();
  const access = gen.next().value;
  cls = {};
  %PrepareFunctionForOptimization(access);
  for (let i = 0; i < 10; ++i) {
    assertEquals("caught", access({}, false));
  }
  %OptimizeFunctionOnNextCall(access);
  assertEquals("caught", access({}, false));
  assertEquals(42, access({}, true));
})();

// Test static private `#m in obj` brand check before class constructor
// initialization finishes (without throwing an exception).
(function TestStaticPrivateInBeforeYield() {
  let gen;
  let cls;
  let exported;
  function advance(resume) {
    if (resume) {
      cls = gen.next().value;
    }
  }
  function* maker() {
    class C {
      static #m() { return 42; }
      [(exported = function access(receiver, resume) {
        let before = #m in receiver;
        advance(resume);
        return #m in cls;
      }, yield exported, "x")] = 1;
    }
    return C;
  }
  for (let i = 0; i < 10; ++i) {
    gen = maker();
    const access = gen.next().value;
    advance(true);
    assertTrue(access(cls, false));
  }
  gen = maker();
  const access = gen.next().value;
  cls = {};
  %PrepareFunctionForOptimization(access);
  for (let i = 0; i < 10; ++i) {
    assertFalse(access({}, false));
  }
  %OptimizeFunctionOnNextCall(access);
  assertFalse(access({}, false));
  assertTrue(access({}, true));
})();
