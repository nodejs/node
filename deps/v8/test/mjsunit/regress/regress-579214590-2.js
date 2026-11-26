// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code

// Regression test for ContextCell / PreparseData parity when bytecode is flushed
// and recompiled while a closure capturing a non-simple `arguments` parameter
// is still alive.
(function testDefaultParamFlush() {
  function f(arguments = 7) {
    function dummy() {}
    return () => arguments;
  }
  const g1 = f();
  for (let i = 0; i < 4; i++) gc();
  const g2 = f(42);
  assertEquals(7, g1());
  assertEquals(42, g2());
})();

(function testDestructuredParamFlush() {
  function f([arguments]) {
    function dummy() {}
    return () => arguments;
  }
  const g1 = f([5]);
  for (let i = 0; i < 4; i++) gc();
  const g2 = f([9]);
  assertEquals(5, g1());
  assertEquals(9, g2());
})();

(function testRestParamFlush() {
  function f(...arguments) {
    function dummy() {}
    return () => arguments;
  }
  const g1 = f(1, 2);
  for (let i = 0; i < 4; i++) gc();
  const g2 = f(3, 4);
  assertEquals([1, 2], g1());
  assertEquals([3, 4], g2());
})();

(function testSimpleParamFlush() {
  function f(arguments) {
    function dummy() {}
    return () => arguments;
  }
  const g1 = f(11);
  for (let i = 0; i < 4; i++) gc();
  const g2 = f(22);
  assertEquals(11, g1());
  assertEquals(22, g2());
})();
