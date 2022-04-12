// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-inline-array-builtins --opt
// Flags: --no-always-opt

// normal case
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, 0);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  testArrayIncludes();
  assertOptimized(testArrayIncludes);
})();

// from_index is not smi will lead to bailout
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, {
      valueOf: () => {
        return 0;
      }
    });
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Length change detected during get from_index, will bailout
(() => {
  let called_values;
  function testArrayIncludes(deopt) {
    const a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    return a.includes(9, {
      valueOf: () => {
        if (deopt) {
          a.length = 3;
        }
        return 0;
      }
    });
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  assertEquals(true, testArrayIncludes());
  testArrayIncludes();
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  assertEquals(true, testArrayIncludes());
  assertEquals(false, testArrayIncludes(true));
  assertFalse(isOptimized(testArrayIncludes));
})();

// Input array change during get from_index, will bailout
(() => {
  const a = [1, 2, 3, 4, 5];
  function testArrayIncludes() {
    return a.includes(9, {
      valueOf: () => {
        a[0] = 9;
        return 0;
      }
    });
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  assertEquals(true, testArrayIncludes());
  testArrayIncludes();
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  assertEquals(true, testArrayIncludes());
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is undefined, will bail out
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, undefined);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is null, will bail out
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, undefined);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is float, will bail out
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, 0.5);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is symbol, will throw
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, Symbol.for('123'));
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  assertThrows(() => testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  assertThrows(() => testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is string, will bailout
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, '0');
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes()
  assertEquals(true, testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle from_index is object which cannot convert to smi, will throw
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20, {
      valueOf: () => {
        return Symbol.for('123')
      }
    });
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  assertThrows(() => testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  assertThrows(() => testArrayIncludes());
  assertFalse(isOptimized(testArrayIncludes));
})();

// Handle input array is smi packed elements and search_element is number
// , will be inlined
(() => {
  const a = [
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  ];
  function testArrayIncludes() {
    return a.includes(20);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);
})();

// Handle input array is double packed elements, will be inlined
(() => {
  const a = [
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, 25.5
  ];
  function testArrayIncludes() {
    return a.includes(20.5);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);
})();

// Handle input array is double packed elements and has NaN, will be inlined
(() => {
  const a = [
    NaN,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, 25.5
  ];
  function testArrayIncludes() {
    return a.includes(NaN);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);

})();

// Handle input array is packed elements, will reach slow path
(() => {
  const a = [
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, {}
  ];
  function testArrayIncludes() {
    return a.includes(20.5);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);

})();


// Handle input array is packed elements, will be inlined
(() => {
  const obj = {}
  const a = [
    1.5,  2.5,  3.5,  4.5,  5.5,  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, obj
  ];
  function testArrayIncludes() {
    return a.includes(obj);
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);

})();


// Handle input array is packed elements and search_element is symbol
(() => {
  const a = [
    1.5,  2.5,  Symbol.for("123"),  "4.5",  BigInt(123),  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, {}
  ];
  function testArrayIncludes() {
    return a.includes(Symbol.for("123"));
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);

})();

// Handle input array is packed elements and search_element is BigInt
(() => {
  const a = [
    1.5,  2.5,  Symbol.for("123"),  "4.5",  BigInt(123),  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, {}
  ];
  function testArrayIncludes() {
    return a.includes(BigInt(123));
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);

})();

// Handle input array is packed elements and search_element is string
(() => {
  const a = [
    1.5,  2.5,  Symbol.for("123"),  "4.5",  BigInt(123),  6.5,  7.5,  8.5,  9.5,
    10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5,
    19.5, 20.5, 21.5, 22.5, 23.5, 24.5, {}
  ];
  function testArrayIncludes() {
    return a.includes("4.5");
  }
  %PrepareFunctionForOptimization(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  %OptimizeFunctionOnNextCall(testArrayIncludes);
  testArrayIncludes();
  assertEquals(true, testArrayIncludes());
  assertOptimized(testArrayIncludes);
})();
