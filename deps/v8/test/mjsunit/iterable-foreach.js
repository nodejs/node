// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertIterated(expected, iterable) {
  let result = [];
  %IterableForEach(iterable, (x) => result.push(x));
  assertEquals(expected, result);
}

// Basic Iteration
// Smi Array
assertIterated([1, 2, 3], [1, 2, 3]);

// Double Array
assertIterated([1.1, 2.2, 3.3], [1.1, 2.2, 3.3]);

// Generic Array
assertIterated(['a', 'b', 'c'], ['a', 'b', 'c']);
assertIterated([{}, {}, {}], [{}, {}, {}]); // Objects are distinct

// Set
assertIterated([1, 2, 3], new Set([1, 2, 3]));

// Custom Iterable
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
assertIterated([0, 1, 2], customIterable);

// Exception handling and 'return' method
function testReturnCalled(iterableFactory) {
  let returnCalled = false;
  let iterable = iterableFactory();
  let iterator = iterable[Symbol.iterator]();
  let originalIterator = iterator;

  // Wrap the iterator to intercept 'return'
  iterable[Symbol.iterator] = () => {
      originalIterator.return = () => {
          returnCalled = true;
          return { done: true };
      };
      return originalIterator;
  }

  try {
    %IterableForEach(iterable, (x) => {
      throw new Error("Stop iteration");
    });
  } catch (e) {
    assertEquals("Stop iteration", e.message);
  }
  assertTrue(returnCalled, "Iterator.return() should be called");
}

// Test with custom iterable
testReturnCalled(() => ({
    [Symbol.iterator]() {
        let i = 0;
        return {
            next() { return { value: i++, done: false }; }
        };
    }
}));

// Exception in 'return' vs Exception in callback
function testExceptionPriority() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() { return { value: 1, done: false }; },
        return() {
            throw new Error("Exception in return");
        }
      };
    }
  };

  try {
    %IterableForEach(iterable, (x) => {
      throw new Error("Exception in callback");
    });
  } catch (e) {
    // The spec says if completion is a throw completion, return ? completion.
    // So "Exception in callback" should be rethrown, suppressing "Exception in return".
    assertEquals("Exception in callback", e.message);
  }
}
testExceptionPriority();

// Stack overflow
function testStackOverflow() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() { return { value: 1, done: false }; },
        return() { return { done: true }; }
      };
    }
  };

  let infiniteIterable = {
      [Symbol.iterator]() {
          return {
              next() { return { value: 1, done: false }; },
              return() { return { done: true }; } // Should be called
          }
      }
  };

  let returnCalled = false;
  let recursiveIterable = {
      [Symbol.iterator]() {
          return {
              next() { return { value: 1, done: false }; },
              return() {
                  returnCalled = true;
                  return { done: true };
              }
          }
      }
  };

  try {
      function recurse(x) {
          recurse(x);
      }
      %IterableForEach(recursiveIterable, recurse);
  } catch(e) {
      assertInstanceof(e, RangeError);
  }
  assertTrue(returnCalled, "Iterator.return() should be called on stack overflow");
}
testStackOverflow();

// Next method throws -> Exception propagates, iterator NOT closed
(function TestNextThrows() {
  let closed = false;
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          throw new Error("Next error");
        },
        return() {
          closed = true;
          return {};
        }
      };
    }
  };

  try {
    %IterableForEach(iterable, () => {});
    // Should have thrown
    throw new Error("Should have thrown Next error");
  } catch (e) {
    if (e.message !== "Next error") throw e;
  }
  // Iterator should NOT be closed because the loop body wasn't entered / Step failed.
  if (closed) throw new Error("Iterator should NOT be closed when next() throws");
})();

// Value getter throws -> Exception propagates, iterator NOT closed
(function TestValueGetterThrows() {
  let closed = false;
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return {
            done: false,
            get value() {
              throw new Error("Value error");
            }
          };
        },
        return() {
          closed = true;
          return {};
        }
      };
    }
  };

  try {
    %IterableForEach(iterable, () => {});
    throw new Error("Should have thrown Value error");
  } catch (e) {
    if (e.message !== "Value error") throw e;
  }
  // Iterator should NOT be closed.
  if (closed) throw new Error("Iterator should NOT be closed when value getter throws");
})();

// Next returns non-object -> TypeError, iterator NOT closed
(function TestNextReturnsNonObject() {
  let closed = false;
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() {
          return 42; // Not an object
        },
        return() {
          closed = true;
          return {};
        }
      };
    }
  };

  try {
    %IterableForEach(iterable, () => {});
    throw new Error("Should have thrown TypeError");
  } catch (e) {
    if (!(e instanceof TypeError)) throw e;
  }
  // Iterator should NOT be closed.
  if (closed) throw new Error("Iterator should NOT be closed when next() returns non-object");
})();

// Array holes are visited as undefined
(function TestArrayHoles() {
  let result = [];
  let arr = [1, , 3];
  %IterableForEach(arr, (val) => result.push(val));
  // holes should be undefined
  if (result.length !== 3) throw new Error("Wrong length");
  if (result[0] !== 1) throw new Error("Wrong index 0");
  if (result[1] !== undefined) throw new Error("Wrong index 1");
  if (result[2] !== 3) throw new Error("Wrong index 2");
})();

// Modified Array Iterator -> Slow path
(function TestModifiedArrayIterator() {
  let result = [];
  let arr = [1, 2];

  // Save original
  const originalIter = Array.prototype[Symbol.iterator];

  // Modify
  Array.prototype[Symbol.iterator] = function() {
    let i = 0;
    return {
      next() {
        if (i < 1) {
            i++;
            return { value: 99, done: false }; // Inject 99
        }
        return { done: true };
      }
    };
  };

  try {
    // Should use the modified iterator, not fast path
    // Wait, modifying Array.prototype[Symbol.iterator] invalidates the protector.
    // So logic should fall back to slow path.
    // The mock iterator yields 99 once, then finishes.
    %IterableForEach(arr, (val) => result.push(val));
  } finally {
    // Restore
    Array.prototype[Symbol.iterator] = originalIter;
  }

  if (result.length !== 1 || result[0] !== 99) {
    throw new Error(`Expected [99], got [${result}]`);
  }
})();

// JSArrayIterator with patched return method
(function TestArrayIteratorReturnCalled() {
  let arr = [1, 2, 3];
  let it = arr.values();
  let returnCalled = false;
  it.return = function() {
    returnCalled = true;
    return { done: true };
  };

  try {
    %IterableForEach(it, (x) => {
      if (x === 2) throw new Error("Stop");
    });
  } catch (e) {
    assertEquals("Stop", e.message);
  }
  assertTrue(returnCalled, "Iterator.return() should be called when callback throws");
})();

// JSArrayIterator with holey array and patched return method
(function TestHoleyArrayIteratorReturnCalled() {
  let arr = [1, , 3];
  let it = arr.values();
  let returnCalled = false;
  it.return = function() {
    returnCalled = true;
    return { done: true };
  };

  try {
    %IterableForEach(it, (x) => {
      if (x === 1) throw new Error("Stop");
    });
  } catch (e) {
    assertEquals("Stop", e.message);
  }
  assertTrue(returnCalled, "Iterator.return() should be called when callback throws on holey array");
})();

// JSArrayIterator with holey array, throw on hole
(function TestHoleyArrayIteratorReturnCalledOnHole() {
  let arr = [1, , 3];
  let it = arr.values();
  let returnCalled = false;
  it.return = function() {
    returnCalled = true;
    return { done: true };
  };

  try {
    %IterableForEach(it, (x) => {
      if (x === undefined) throw new Error("Stop");
    });
  } catch (e) {
    assertEquals("Stop", e.message);
  }
  assertTrue(returnCalled, "Iterator.return() should be called when callback throws on hole");
})();

// JSSetIterator with patched return method
(function TestSetIteratorReturnCalled() {
  let set = new Set([1, 2, 3]);
  let it = set.values();
  let returnCalled = false;
  it.return = function() {
    returnCalled = true;
    return { done: true };
  };

  try {
    %IterableForEach(it, (x) => {
      if (x === 2) throw new Error("Stop");
    });
  } catch (e) {
    assertEquals("Stop", e.message);
  }
  assertTrue(returnCalled, "Iterator.return() should be called when callback throws on Set iterator");
})();

// Test iterator.return() returning non-object
(function TestReturnReturnsNonObject() {
  let iterable = {
    [Symbol.iterator]() {
      return {
        next() { return { value: 1, done: false }; },
        return() { return 42; } // Invalid, but ignored on exception
      };
    }
  };
  try {
    // Break iteration to trigger return()
    %IterableForEach(iterable, () => { throw new Error('break'); });
  } catch (e) {
    // The original error 'break' takes precedence over the invalid return value
    // because IteratorClose returns the completion if it's a throw completion (Step 5).
    assertEquals('break', e.message);
  }
})();

// TypedArray Iteration
// Uint8Array
assertIterated([1, 2, 3], new Uint8Array([1, 2, 3]));
// Float32Array
assertIterated([1.5, 2.5, 3.5], new Float32Array([1.5, 2.5, 3.5]));
// BigInt64Array
if (typeof BigInt64Array !== 'undefined') {
  assertIterated([1n, 2n, 3n], new BigInt64Array([1n, 2n, 3n]));
}
// Float16Array
if (typeof Float16Array !== 'undefined') {
   assertIterated([1.5, 2.5], new Float16Array([1.5, 2.5]));
}

// Detached TypedArray
(function TestDetachedTypedArray() {
  let ta = new Uint8Array([1, 2, 3]);
  try {
    %IterableForEach(ta, (x) => {
      if (x === 2) {
        %ArrayBufferDetach(ta.buffer);
      }
    });
    throw new Error("Should have thrown TypeError");
  } catch (e) {
    // V8 error message for detached TypedArray
    if (!e.message.includes("detached")) throw e;
  }
})();

// Modified TypedArray Iterator
(function TestModifiedTypedArrayIterator() {
  let ta = new Uint8Array([1, 2]);
  let result = [];
  const originalIter = Uint8Array.prototype[Symbol.iterator];
  Uint8Array.prototype[Symbol.iterator] = function() {
    let i = 0;
    return {
      next() {
        if (i < 1) {
            i++;
            return { value: 99, done: false };
        }
        return { done: true };
      }
    };
  };

  try {
    // Should fall back to slow path and use modified iterator
    %IterableForEach(ta, (val) => result.push(val));
  } finally {
    Uint8Array.prototype[Symbol.iterator] = originalIter;
  }
  if (result.length !== 1 || result[0] !== 99) {
    throw new Error(`Expected [99], got [${result}]`);
  }
})();

// TypedArray with offset
(function TestOffsetTypedArray() {
  const buffer = new ArrayBuffer(16);
  const ta = new Uint8Array(buffer, 8, 4); // Offset 8, length 4
  for (let i = 0; i < 4; i++) {
    ta[i] = i + 1;
  }
  assertIterated([1, 2, 3, 4], ta);
})();
