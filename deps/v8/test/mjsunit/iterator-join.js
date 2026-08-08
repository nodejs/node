// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-iterator-join --allow-natives-syntax

// Basic functionality.
{
  const iter = [1, 2, 3].values();
  assertEquals("1,2,3", iter.join());
}

{
  const iter = [1, 2, 3].values();
  assertEquals("1;2;3", iter.join(";"));
}

{
  const iter = [1, 2, 3].values();
  assertEquals("123", iter.join(""));
}

// Null and undefined values.
{
  const iter = [1, null, 2, undefined, 3].values();
  assertEquals("1,,2,,3", iter.join(","));
}

// Null and undefined separator.
{
  const iter = [1, 2].values();
  assertEquals("1,2", iter.join(undefined));
}

{
  const iter = [1, 2].values();
  assertEquals("1null2", iter.join(null));
}

// Non-object receiver.
{
  assertThrows(() => Iterator.prototype.join.call(null), TypeError);
  assertThrows(() => Iterator.prototype.join.call(undefined), TypeError);
}

// Closing iterator on error (ToString(value)).
{
  let returnCalled = false;
  const iter = {
    next() {
      return {
        value: { toString() { throw new Error("foo"); } },
        done: false
      };
    },
    return() { returnCalled = true; return { done: true }; }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  try {
    iter.join();
    fail("Should have thrown");
  } catch (e) {
    assertEquals("foo", e.message);
  }
  assertTrue(returnCalled);
}

// Closing iterator on error (ToString(separator)).
{
  let returnCalled = false;
  const iter = {
    next() { return { value: 1, done: false }; },
    return() { returnCalled = true; return { done: true }; }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  const badSep = { toString() { throw new Error("bar"); } };

  try {
    iter.join(badSep);
    fail("Should have thrown");
  } catch (e) {
    assertEquals("bar", e.message);
  }
  assertTrue(returnCalled);
}

// Test with generator.
{
  function* gen() {
    yield 1;
    yield 2;
    yield 3;
  }
  assertEquals("1-2-3", gen().join("-"));
}

// Separator evaluation order and frequency.
// Separator should be stringified exactly once, before iteration begins.
{
  let sepToStringCount = 0;
  const sep = {
    toString() {
      sepToStringCount++;
      return "|";
    }
  };
  const iter = [1, 2, 3].values();
  const result = iter.join(sep);
  assertEquals("1|2|3", result);
  assertEquals(1, sepToStringCount);
}

// Separator side effects modifying the iterator.
// Since separator is evaluated before iteration, if it consumes the iterator,
// the join should be empty (or start from where it left off).
{
  const iter = [1, 2, 3].values();
  const sep = {
    toString() {
      iter.next(); // Consumes '1'
      return ",";
    }
  };
  // '1' consumed. Remaining: 2, 3.
  // Join starts. First value is 2.
  assertEquals("2,3", iter.join(sep));
}

// Value toString() side effects modifying the underlying array.
{
  const arr = [1, 2, 3];
  const iter = arr.values();
  const v1 = {
    toString() {
      arr.push(4);
      return "1";
    }
  };
  arr[0] = v1;
  assertEquals("1,2,3,4", iter.join(","));
}

// Value toString() side effects modifying the underlying Set.
{
  const set = new Set();
  const obj = {
    toString() {
      set.add("c");
      return "a";
    }
  };
  set.add(obj);
  set.add("b");

  const iter = set.values();
  // Iteration order: obj, "b", then "c" is added.
  // Sets iterate insertion order. "c" added during iteration of first element.
  // Should visit "c".
  assertEquals("a,b,c", iter.join(","));
}

// Exhausted iterator
{
  const iter = [1, 2].values();
  iter.next();
  iter.next();
  iter.next(); // Done
  assertEquals("", iter.join(","));
}

// Throwing during iteration closes the iterator.
{
  let returnCalled = false;
  const iter = {
    next() {
      return {
        value: { toString() { throw new Error("boom"); } },
        done: false
      };
    },
    return() { returnCalled = true; return { done: true }; }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  assertThrows(() => iter.join(), Error, "boom");
  assertTrue(returnCalled);
}

// Verify 'this' check
{
  assertThrows(() => Iterator.prototype.join.call(123), TypeError);
}

// Large string construction (Stack overflow / limit check).
// Just a basic check that it doesn't crash on reasonably large inputs.
{
  const limit = 1000;
  function* gen() {
    for (let i = 0; i < limit; i++) yield "a";
  }
  const result = gen().join("");
  assertEquals(limit, result.length);
}

// Detaching buffer during iteration.
// This simulates a scenario where an external force invalidates the iterator
// source.
{
  const ta = new Int8Array([1, 2, 3]);
  const originalIter = ta.values();
  const iter = {
    next() {
      const { value, done } = originalIter.next();
      if (done) return { done: true };
      return {
        value: {
          toString() {
            if (value === 1) {
              try { %ArrayBufferDetach(ta.buffer); } catch(e) {}
            }
            return value.toString();
          }
        },
        done: false
      };
    }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  // 1st element: 1 -> toString() -> detaches buffer.
  // 2nd element fetch: ArrayIterator.next() checks buffer -> throws TypeError.
  // The error should propagate out of join.
  assertThrows(() => iter.join(","), TypeError);
}

// Cyclic structure (array containing iterator)
// join() calls ToString(value). If value is [iter],
// ToString([iter]) -> [iter].toString() -> [iter].join() -> iter.toString()
// -> "[object Iterator]" (or similar).
// This should NOT be infinite recursion.
{
  const iter = {
    next() {
      return { value: [this], done: false };
    },
    [Symbol.iterator]() { return this; },
    toString() { return "ITER"; }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  // We need to limit it because the iterator itself is infinite (always returns
  // {value: [this]}). But we want to check that processing *one* value doesn't
  // recurse infinitely. We can make it finite.
  let count = 0;
  iter.next = function() {
    if (count++ > 0) return { done: true };
    return { value: [this], done: false };
  };

  assertEquals("ITER", iter.join(","));
}

// Test a throwing value does close the iterator.
{
  const throwy = {
    get toString() { throw new Error("foo"); }
  };

  const iter = [throwy].values();
  let returnCalled = false;
  iter.return = function () { returnCalled = true; return { done: true }; }

  assertThrows(() => iter.join(), Error, "foo");
  assertTrue(returnCalled);
}

// Test a range error does not close the iterator.
{
  const huge = 'x'.repeat(1024 * 1024);
  let iter = new Array(1000);
  iter.fill(huge);
  let returnCalled = false;
  iter.return = function () { returnCalled = true; return { done: true }; }

  assertThrows(() => iter.join(""), RangeError);
  assertFalse(returnCalled);
}

// Verifies that a throwing .value getter (part of IteratorStepValue)
// does not trigger an implicit iterator closure. This ensures Step 9.a is not
// incorrectly wrapped in the same error handling as Step 9.e.
{
  let returnCalled = false;
  const iter = {
    next() {
      return {
        get value() { throw new Error("foo"); },
        done: false
      };
    },
    return() {
      returnCalled = true;
      return { done: true };
    }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  assertThrows(() => iter.join(), Error, "foo");
  assertFalse(returnCalled, "should not close the iterator");
}

// Verifies that a failure during the initial lookup of the .next
// property (Step 6) does not close the iterator. This confirms the iterator
// record is only active for closing after the lookup succeeds.
{
  let returnCalled = false;
  const iter = {
    get next() { throw new Error("foo"); },
    return() {
      returnCalled = true;
      return { done: true };
    }
  };
  Object.setPrototypeOf(iter, Iterator.prototype);

  assertThrows(() => iter.join(), Error, "foo");
  assertFalse(returnCalled, "should not close the iterator");
}

// Verifies the internal execution order of the loop. Specifically, it
// confirms that IteratorStepValue (calling next() and accessing.value) must be
// fully completed for a given element before the implementation attempts to
// concatenate a separator for that element.
{
  function test(n) {
    const str = 'x'.repeat(n);

    let nextCallCount = 0;
    let returnCalled = false;
    const iter = {
      next() {
        nextCallCount++;
        return {
          get value() {
            if (nextCallCount == 1) return str;
            throw new Error("foo");
          },
          done: false
        };
      },
      return() {
        returnCalled = true;
        return { done: true };
      }
    };
    Object.setPrototypeOf(iter, Iterator.prototype);

    assertThrows(() => iter.join(str), Error, "foo");
    assertEquals(2, nextCallCount);
    assertFalse(returnCalled, "should not close the iterator");
  }

  test(100);
}

// Verifies the boundary between Step 9.d and Step 9.e. It ensures that
// if the separator concatenation fails, the subsequent ToString(value)
// operation is skipped, preventing unnecessary side effects.
{
  function test(n) {
    const str = 'x'.repeat(n);

    let valueAccessCount = 0;
    let valueStringifyCount = 0;
    let nextCallCount = 0;
    const iter = {
      next() {
        if (++nextCallCount > 2) return { done: true };
        return {
          get value() {
            valueAccessCount++;
            return {
              toString() { valueStringifyCount++; return str; }
            };
          },
          done: false
        };
      }
    };
    Object.setPrototypeOf(iter, Iterator.prototype);

    try {
      iter.join(str);
      assertEquals(3, nextCallCount);
      assertEquals(2, valueAccessCount);
      assertEquals(2, valueStringifyCount);
    } catch (e) {
      assertTrue(e instanceof RangeError);
      assertEquals(2, nextCallCount);
      assertEquals(2, valueAccessCount);
      assertEquals(1, valueStringifyCount);
    }
  }

  test(100);
}
