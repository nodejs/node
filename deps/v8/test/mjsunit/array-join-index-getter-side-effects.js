// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function Throws() {
  function TestError() {}

  let callCount = 0;
  const a = [0, 1];
  Object.defineProperty(a, '0', {
    configurable: true,
    get() {
      callCount++;
      throw new TestError();
    }
  });
  assertTrue(%HasDictionaryElements(a));
  assertThrows(() => a.join(), TestError);
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after thrown error.
  Object.defineProperty(a, '0', {
    configurable: true,
    get() {
      callCount++;
      return 777;
    }
  });
  assertSame('777,1', a.join());
  assertSame(2, callCount);
})();

(function ArrayLengthIncreased() {
  let callCount = 0;
  const a = [1];
  Object.defineProperty(a, '0', {
    configurable: true,
    get() {
      callCount++;
      a.push(2);
      return 9;
    }
  });
  assertSame('9', a.join());
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after continuation.
  assertSame('9,2', a.join());
  assertSame(2, callCount);
})();

(function ArrayLengthIncreasedWithHole() {
  let callCount = 0;
  const a = [1, , 2];
  Object.defineProperty(a, '1', {
    configurable: true,
    get() {
      callCount++;
      a.push(3);
    }
  });
  assertSame('1,,2', a.join());
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after continuation.
  assertSame('1,,2,3', a.join());
  assertSame(2, callCount);
})();

(function ArrayLengthDecreased() {
  let callCount = 0;
  const a = [0, 1];
  Object.defineProperty(a, '0', {
    configurable: true,
    get() {
      callCount++;
      a.length = 1;
      return 9;
    }
  });
  assertSame('9,', a.join());
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after continuation.
  assertSame('9', a.join());
  assertSame(2, callCount);
})();

(function ElementsKindChangedToHoley() {
  let callCount = 0;
  const a = [0, 1];
  Object.defineProperty(a, '0', {
    configurable: true,
    get() {
      callCount++;
      a.length = 3;
      return 9;
    }
  });
  assertSame('9,1', a.join());
  assertSame(1, callCount);

  // Verifies cycle detection still works properly after continuation.
  assertSame('9,1,', a.join());
  assertSame(2, callCount);
})();
