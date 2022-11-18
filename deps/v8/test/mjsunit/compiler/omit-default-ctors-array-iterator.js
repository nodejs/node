// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors --allow-natives-syntax --turbofan
// Flags: --no-always-turbofan

// This behavior is not spec compliant, see crbug.com/v8/13249.
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
  %OptimizeFunctionOnNextCall(C);

  // C default ctor doing "...args" and B default ctor doing "...args".
  assertEquals(2, iterationCount);

  new C();

  // C default ctor doing "...args" and B default ctor doing "...args".
  assertEquals(4, iterationCount);
  assertTrue(isTurboFanned(C));  // No deopt.

  Array.prototype[Symbol.iterator] = oldIterator;
})();
