// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors --allow-natives-syntax --maglev

(function ArrayIteratorMonkeyPatched() {
  let iterationCount = 0;
  const oldIterator = Array.prototype[Symbol.iterator];
  Array.prototype[Symbol.iterator] =
      function () { ++iterationCount; return oldIterator.call(this); };

  class A {}
  class B extends A {}
  class C extends B {}

  %PrepareFunctionForOptimization(C);
  new C();
  %OptimizeMaglevOnNextCall(C);

  // C default ctor doing "...args" and B default ctor doing "...args"
  // do *not* use the iterator.
  assertEquals(0, iterationCount);

  new C();

  // C default ctor doing "...args" and B default ctor doing "...args"
  // do *not* use the iterator.
  assertEquals(0, iterationCount);
  assertTrue(isMaglevved(C));  // No deopt.

  Array.prototype[Symbol.iterator] = oldIterator;
})();
