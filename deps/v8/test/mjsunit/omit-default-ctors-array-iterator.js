// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors

// This behavior is not spec compliant, see crbug.com/v8/13249.
(function ArrayIteratorMonkeyPatched() {
  let iterationCount = 0;
  const oldIterator = Array.prototype[Symbol.iterator];
  Array.prototype[Symbol.iterator] =
      function () { ++iterationCount; return oldIterator.call(this); };

  class A {}
  class B extends A {}
  class C extends B {}

  assertEquals(0, iterationCount);

  new C();

  // C default ctor doing "...args" and B default ctor doing "...args".
  assertEquals(2, iterationCount);

  Array.prototype[Symbol.iterator] = oldIterator;
})();
