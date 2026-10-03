// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-joint-iteration

(function TestNamedShortest() {
  const result = Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2],
  }));
  assertEquals(1, result.length);
  assertEquals(0, result[0].a);
  assertEquals(1, result[0].b);
  assertEquals(null, Object.getPrototypeOf(result[0]));
})();

(function TestNamedEquiv() {
  const result = Array.from(Iterator.zipKeyed({
    a: [0, 1, 2],
    b: [3, 4, 5],
    c: [6, 7, 8],
  }));
  assertEquals(3, result.length);
  for (let i = 0; i < 3; i++) {
    assertEquals(i, result[i].a);
    assertEquals(i + 3, result[i].b);
    assertEquals(i + 6, result[i].c);
    assertEquals(null, Object.getPrototypeOf(result[i]));
  }
})();

(function TestNamedEmpty() {
  assertArrayEquals([], Array.from(Iterator.zipKeyed({})));
})();

(function TestNamedLongest() {
  const result = Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2],
  }, { mode: 'longest' }));
  assertEquals(2, result.length);

  assertEquals(0, result[0].a);
  assertEquals(1, result[0].b);
  assertEquals(null, Object.getPrototypeOf(result[0]));

  assertEquals(undefined, result[1].a);
  assertEquals(2, result[1].b);
  assertEquals(null, Object.getPrototypeOf(result[1]));
})();

(function TestNamedStrict() {
  let result =
    Iterator.zipKeyed({
      a: [0],
      b: [1, 2],
    }, { mode: 'strict' });
  assertThrows(() => {
    Array.from(result);
  }, TypeError);
})();

(function TestNamedPadding() {
  const A_PADDING = {};
  const B_PADDING = {};
  const C_PADDING = {};
  const D_PADDING = {};

  const padding = {
    a: A_PADDING,
    b: B_PADDING,
    c: C_PADDING,
    d: D_PADDING
  };

  const result1 = Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2, 3],
  }, { mode: 'longest', padding }));

  assertEquals(3, result1.length);
  assertEquals(0, result1[0].a);
  assertEquals(1, result1[0].b);
  assertEquals(A_PADDING, result1[1].a);
  assertEquals(2, result1[1].b);
  assertEquals(A_PADDING, result1[2].a);
  assertEquals(3, result1[2].b);
  for (let res of result1) assertEquals(null, Object.getPrototypeOf(res));

  const result2 = Array.from(Iterator.zipKeyed({
    a: [0],
    b: [1, 2, 3],
    c: [4, 5],
    d: [],
  }, { mode: 'longest', padding }));

  assertEquals(3, result2.length);

  assertEquals(0, result2[0].a);
  assertEquals(1, result2[0].b);
  assertEquals(4, result2[0].c);
  assertEquals(D_PADDING, result2[0].d);

  assertEquals(A_PADDING, result2[1].a);
  assertEquals(2, result2[1].b);
  assertEquals(5, result2[1].c);
  assertEquals(D_PADDING, result2[1].d);

  assertEquals(A_PADDING, result2[2].a);
  assertEquals(3, result2[2].b);
  assertEquals(C_PADDING, result2[2].c);
  assertEquals(D_PADDING, result2[2].d);

  for (let res of result2) assertEquals(null, Object.getPrototypeOf(res));
})();

(function TestNextFirstErrorPropagation() {
  let log = [];
  const iter1 = {
    next() { log.push("n1"); throw new Error("error1"); },
    return() { log.push("r1"); }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);
  const iter2 = {
    next() { log.push("n2"); throw new Error("error2"); },
    return() { log.push("r2"); }
  };
  Object.setPrototypeOf(iter2, Iterator.prototype);

  const zipped = Iterator.zipKeyed({ a: iter1, b: iter2 });

  try {
    zipped.next();
    assertUnreachable();
  } catch (e) {
    assertEquals("error1", e.message);
  }

  assertEquals(["n1", "r2"], log);
})();

(function TestReturnFirstErrorPropagation() {
  let log = [];
  const iter1 = {
    next() { return { value: 1, done: false }; },
    return() { log.push("r1"); throw new Error("error1"); }
  };
  Object.setPrototypeOf(iter1, Iterator.prototype);
  const iter2 = {
    next() { return { value: 2, done: false }; },
    return() { log.push("r2"); throw new Error("error2"); }
  };
  Object.setPrototypeOf(iter2, Iterator.prototype);

  const zipped = Iterator.zipKeyed({ a: iter1, b: iter2 });
  const result = zipped.next();
  assertEquals(result.value.a, 1);
  assertEquals(result.value.b, 2);
  assertFalse(result.done);
  assertArrayEquals([], log);

  try {
    zipped.return();
    assertUnreachable();
  } catch (e) {
    assertEquals("error2", e.message);
  }

  assertArrayEquals(["r2", "r1"], log);
})();

(function TestNamedOwnPropertyKeysThrows() {
  const proxy = new Proxy({}, {
    ownKeys() { throw new Error("fail"); }
  });

  assertThrows(() => {
    Iterator.zipKeyed(proxy);
  }, Error);
})();

(function RegressionTestOnOddCapacityGetOwnPropertyThrows() {
  const iterables = {};
  for (let i = 0; i <= 76; i++) {
    iterables['k' + i] = [1];
  }
  // Pushing the 77th iterabla triggers growth to 211 capacity.

  // We use a proxy to make GetOwnProperty throw for 'k76'.
  const proxy = new Proxy(iterables, {
    getOwnPropertyDescriptor(target, key) {
      if (key === 'k76') throw new Error("fail");
      return Object.getOwnPropertyDescriptor(target, key);
    }
  });

  assertThrows(() => {
    Iterator.zipKeyed(proxy);
  }, Error);
})();

(function RegressionTestOnOddCapacityGetThrows() {
  const iterables = {};
  for (let i = 0; i < 76; i++) {
    iterables['k' + i] = [1];
  }
  Object.defineProperty(iterables, 'k76', {
    get() { throw new Error("fail"); },
    enumerable: true,
    configurable: true
  });

  assertThrows(() => {
    Iterator.zipKeyed(iterables);
  }, Error);
})();

(function RegressionTestOnOddCapacityNonIterable() {
  const iterables = {};
  for (let i = 0; i < 76; i++) {
    iterables['k' + i] = [1];
  }
  iterables['k76'] = 1;

  assertThrows(() => {
    Iterator.zipKeyed(iterables);
  }, TypeError);
})();

(function RegressionTestOnOddCapacityPaddingGetThrows() {
  const iterables = {};
  for (let i = 0; i < 77; i++) {
    iterables['k' + i] = [1];
  }

  const options = {
    mode: 'longest',
    padding: {
      get k0() { throw new Error("fail"); }
    }
  };

  assertThrows(() => {
    Iterator.zipKeyed(iterables, options);
  }, Error);
})();
