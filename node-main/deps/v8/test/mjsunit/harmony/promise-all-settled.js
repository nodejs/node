// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignore-unhandled-promises

class MyError extends Error {}

const descriptor = Object.getOwnPropertyDescriptor(Promise, "allSettled");
assertTrue(descriptor.configurable);
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);

// 1. Let C be the this value.
// 2. If Type(C) is not Object, throw a TypeError exception.
assertThrows(() => Promise.allSettled.call(1), TypeError);

{
  // 3. Let promiseCapability be ? NewPromiseCapability(C).
  let called = false;
  class MyPromise extends Promise {
    constructor(...args) {
      called = true;
      super(...args);
    }
  }

  MyPromise.allSettled([]);
  assertTrue(called);
}

{
  // 4. Let iteratorRecord be GetIterator(iterable).
  // 5. IfAbruptRejectPromise(iteratorRecord, promiseCapability).
  let caught = false;
  (async function() {
    class MyError extends Error {}
    let err;

    try {
      await Promise.allSettled({
        [Symbol.iterator]() {
          throw new MyError();
        }
      });
    } catch (e) {
      assertTrue(e instanceof MyError);
      caught = true;
    }
  })();

  %PerformMicrotaskCheckpoint();
  assertTrue(caught);
}

{
  // 6. a. Let next be IteratorStep(iteratorRecord).
  let iteratorStep = false;
  (async function() {
    try {
      await Promise.allSettled({
        [Symbol.iterator]() {
          return {
            next() {
              iteratorStep = true;
              return { done: true }
            }
          };
        }
      });
    } catch (e) {
      %AbortJS(e.stack);
    }
  })();

  %PerformMicrotaskCheckpoint();
  assertTrue(iteratorStep);
}

{
  // 6. a. Let next be IteratorStep(iteratorRecord).
  // 6. b. If next is an abrupt completion, set iteratorRecord.[[Done]] to true.
  // 6. c. ReturnIfAbrupt(next).
  let caught = false;
  (async function() {
    try {
      await Promise.allSettled({
        [Symbol.iterator]() {
          return {
            next() {
              throw new MyError();
            }
          };
        }
      });
    } catch (e) {
      assertTrue(e instanceof MyError);
      caught = true;
    }
  })();

  %PerformMicrotaskCheckpoint();
  assertTrue(caught);
}

{
  // 6. e. Let nextValue be IteratorValue(next).
  let iteratorValue = false;
  (async function() {
    try {
      await Promise.allSettled({
        [Symbol.iterator]() {
          let done = false;

          return {
            next() {
              let result = { value: 1, done };
              iteratorValue = true;
              done = true;
              return result;
            }
          };
        }
      });
    } catch (e) {
      %AbortJS(e.stack);
    }
  })();

  %PerformMicrotaskCheckpoint();
  assertTrue(iteratorValue);
}

{
  // 6. e. Let nextValue be IteratorValue(next).
  // 6. f. If nextValue is an abrupt completion, set iteratorRecord.[[Done]] to true.
  // 6. g. ReturnIfAbrupt(nextValue).
  let caught = false;
  (async function() {
    try {
      await Promise.allSettled({
        [Symbol.iterator]() {
          let done = false;

          return {
            next() {
              return result = {
                get value() {throw new MyError(''); },
                done: false
              };
            }
          };
        }
      });
    } catch (e) {
      assertTrue(e instanceof MyError);
      caught = true;
    }
  })();

  %PerformMicrotaskCheckpoint();
  assertTrue(caught);
}

{
  // TODO(mathias): https://github.com/tc39/proposal-promise-allSettled/pull/40
  // 6. i. Let nextPromise be ? Invoke(constructor, "resolve", « nextValue »).
  let called = false;
  class MyPromise extends Promise {
    static resolve(...args) {
      called = true;
      super.resolve(...args);
    }
  }

  MyPromise.allSettled([1]);
  assertTrue(called);
}

{
  // 6. z. Perform ? Invoke(nextPromise, "then", « resolveElement, rejectElement »).
  let called = false;
  class MyPromise extends Promise {
    then(...args) {
      called = true;
      super.resolve(...args);
    }
  }

  MyPromise.allSettled([1]);
  assertTrue(called);
}

{
  let called = false;
  let result;
  Promise.allSettled([]).then(x => {
    called = true;
    result = x;
  });

  %PerformMicrotaskCheckpoint();
  assertTrue(called);
  assertEquals(result, []);
}

{
  let called = false;
  Promise.allSettled([Promise.resolve("foo")]).then(v => {
    assertEquals(v.length, 1);
    const [x] = v;

    assertSame(Object.getPrototypeOf(x), Object.getPrototypeOf({}));
    const descs = Object.getOwnPropertyDescriptors(x);
    assertEquals(Object.keys(descs).length, 2);

    const { value: desc } = descs;
    assertTrue(desc.writable);
    assertTrue(desc.enumerable);
    assertTrue(desc.configurable);

    assertEquals(x.value, "foo");
    assertEquals(x.status, "fulfilled");
    called = true;
  });

  %PerformMicrotaskCheckpoint();
  assertTrue(called);
}

{
  let called = false;
  Promise.allSettled([Promise.reject("foo")]).then(v => {
    assertEquals(v.length, 1);
    const [x] = v;
    assertEquals(x.reason, "foo");
    assertEquals(x.status, "rejected");
    called = true;
  });

  %PerformMicrotaskCheckpoint();
  assertTrue(called);
}

{
  let called = false;
  Promise.allSettled([Promise.resolve("bar"), Promise.reject("foo")]).then(v => {
    assertEquals(v.length, 2);
    const [x, y] = v;
    assertEquals(x.value, "bar");
    assertEquals(x.status, "fulfilled");
    assertEquals(y.reason, "foo");
    assertEquals(y.status, "rejected");
    called = true;
  });

  %PerformMicrotaskCheckpoint();
  assertTrue(called);
}
