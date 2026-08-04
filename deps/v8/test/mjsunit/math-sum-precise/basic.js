// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-sum-precise --allow-natives-syntax

function assertSum(expected, iterable) {
  let res = Math.sumPrecise(iterable);
  assertEquals(expected, res);
}

assertEquals(-0, Math.sumPrecise([]));
assertEquals(6, Math.sumPrecise([1, 2, 3]));
assertEquals(-0, Math.sumPrecise([-0, -0]));
assertSum(6.6, [1.1, 2.2, 3.3]);

// Abusing an existing iterator (Array iterator on plain object)
(function TestAbusedArrayIterator() {
  var x = {};
  Object.defineProperty(x, "0", {value: 42});
  x.length = 1;
  x[Symbol.iterator] = [].constructor.prototype[Symbol.iterator];
  assertSum(42, x);
})();

// Species / Subclassing check
(function TestTypedArraySpecies() {
  class MyUint8Array extends Uint8Array {
    static get [Symbol.species]() {
      return Array;
    }
  }
  const typed = new MyUint8Array([10, 20, 30]);
  assertSum(60, typed);
})();

// Holey Arrays (Undefined -> NaN)
(function TestHoleyArray() {
  // [1, , 3] -> hole is undefined -> not a number -> TypeError
  assertThrows(() => Math.sumPrecise([1, , 3]), TypeError);
})();

// Strings
(function TestStrings() {
  assertThrows(() => Math.sumPrecise("123"), TypeError);
})();

// Set and Map
(function TestSetMap() {
  assertSum(6, new Set([1, 2, 3]));
  assertThrows(() => Math.sumPrecise(new Map([[1, 1], [2, 2]])), TypeError);
  assertSum(3, new Map([['a', 1], ['b', 2]]).values());
})();

// TypedArrays (various types)
(function TestTypedArrays() {
  assertSum(6, new Uint8Array([1, 2, 3]));
  assertSum(6, new Float32Array([1, 2, 3]));
  if (typeof BigInt64Array !== 'undefined') {
    assertThrows(() => Math.sumPrecise(new BigInt64Array([1n])), TypeError);
  }
})();

// Detached TypedArray
(function TestDetachedTypedArray() {
  let ta = new Uint8Array([1, 2, 3]);
  %ArrayBufferDetach(ta.buffer);
  assertThrows(() => Math.sumPrecise(ta), TypeError);
})();

// Iterator that throws
(function TestThrowingIterator() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() { throw new Error("Boom"); }
      };
    }
  };
  assertThrows(() => Math.sumPrecise(iterable), Error, "Boom");
})();

// Iterator returning non-object
(function TestBadIterator() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() { return 42; } // Non-object result
      };
    }
  };
  assertThrows(() => Math.sumPrecise(iterable), TypeError);
})();

// Modifying TypedArray during iteration
(function TestModificationDuringIteration() {
  // If we iterate a generic object that mutates...
  let arr = [0, 0, 3];
  let proxy = new Proxy(arr, {
    get(target, prop, receiver) {
      if (prop === "length") return 3;
      if (prop === "0") {
        arr[1] = 2;
        return 1;
      }
      return Reflect.get(target, prop, receiver);
    }
  });
  assertSum(6, proxy);
})();

(function TestModificationDuringIteration2() {
  // If we iterate a generic object that mutates...
  let arr = [0, 0, 3];
  Object.defineProperty(arr, "0", { get: function() {
    arr[1] = 2;
    return 1;
  }});
  assertSum(6, arr);
})();

// Very large values (precision check)
(function TestPrecision() {
  // 1e100 + -1e100 + 1 = 1
  assertSum(1, [1e100, -1e100, 1]);
})();

// Resizable ArrayBuffer out of bounds
(function TestResizableArrayBufferOutOfBounds() {
  let rab = new ArrayBuffer(1024, { maxByteLength: 2048 });
  let ta = new Uint8Array(rab, 512, 128);
  rab.resize(256);
  assertThrows(() => Math.sumPrecise(ta), TypeError);
})();

// Custom Iterable
(function TestCustomIterable() {
  let customIterable = {
    [Symbol.iterator]() {
      let i = 0;
      return {
        next() {
          if (i < 3) return { value: i++, done: false };
          return { value: undefined, done: true };
        }
      };
    }
  };
  assertSum(3, customIterable);
})();

// TypedArray Iteration
// Float16Array
if (typeof Float16Array !== 'undefined') {
   assertSum(4, new Float16Array([1.5, 2.5]));
}

// TypedArray with offset
(function TestOffsetTypedArrayFromIterableForeach() {
  const buffer = new ArrayBuffer(16);
  const ta = new Uint8Array(buffer, 8, 4); // Offset 8, length 4
  for (let i = 0; i < 4; i++) {
    ta[i] = i + 1;
  }
  assertSum(10, ta);
})();

// Empty Iterables
(function TestEmptyIterables() {
  assertSum(-0, []);
  assertSum(-0, new Set());
  assertSum(-0, new Map().values());
  assertSum(-0, (function*(){})());
  assertSum(-0, new Uint8Array(0));
})();

// Generator Function
(function TestGenerator() {
  function* gen() {
    yield 10;
    yield 20;
    yield 30;
  }
  assertSum(60, gen());
})();

// Infinity and NaN
(function TestInfinityAndNaN() {
  // Infinity + -Infinity is NaN
  assertSum(NaN, [Infinity, -Infinity]);
  assertSum(Infinity, [Infinity, Infinity]);
  assertSum(-Infinity, [-Infinity, -Infinity]);
  assertSum(Infinity, [1, Infinity]);
  // NaN propagates
  assertSum(NaN, [1, NaN, 2]);
})();

// Iterator Closing on Error
(function TestIteratorClosing() {
  let returnCalled = false;
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return { value: "not a number", done: false };
        },
        return() {
          returnCalled = true;
          return { done: true };
        }
      };
    }
  };

  assertThrows(() => Math.sumPrecise(iterable), TypeError);
  assertTrue(returnCalled, "Iterator should have been closed");
})();

// Signed Zero
(function TestSignedZero() {
  assertSum(-0, [-0]);
  assertSum(-0, [-0, -0]);
  assertSum(0, [0, -0]);
  assertSum(0, [-0, 0]);
})();

// Iterator Closing Throws
(function TestIteratorClosingThrows() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return { value: "not a number", done: false };
        },
        return() {
          throw new Error("Error in return");
        }
      };
    }
  };

  // Should throw TypeError (from "not a number"), checking that it suppresses "Error in return"
  assertThrows(() => Math.sumPrecise(iterable), TypeError);
})();
