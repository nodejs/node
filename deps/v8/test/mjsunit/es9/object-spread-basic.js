// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var x = {a: 1};
var y = { ...x};
assertEquals(x, y);

assertEquals({}, y = { ...{} } );
assertEquals({}, y = { ...undefined });
assertEquals({}, y = { ...null });

assertEquals({}, y = { ...1 });
assertEquals({}, y = { ...1n });
assertEquals({}, y = { ...NaN });
assertEquals({}, y = { ...false });
assertEquals({}, y = { ...true });
assertEquals({}, y = { ...Symbol() });
assertEquals({0: 'f', 1: 'o', 2: 'o'}, y = { ...'foo' });
assertEquals({0: 0, 1: 1}, y = { ...[0, 1] });
assertEquals({}, { ...new Proxy({}, {}) });

assertEquals({a: 2}, y = { ...x, a: 2 });
assertEquals({a: 1, b: 1}, y = { ...x, b: 1 });
assertEquals({a: 1}, y = { a: 2, ...x });
assertEquals({a: 1, b: 1}, y = { a:2, ...x, b: 1 });
assertEquals({a: 3}, y = { a: 2, ...x, a: 3 });

var z = { b: 1}
assertEquals({a: 1, b: 1}, y = { ...x, ...z });
assertEquals({a: 1, b: 1}, y = { a: 2, ...x, ...z });
assertEquals({a: 1, b: 1}, y = { b: 2, ...z, ...x });
assertEquals({a: 1, b: 1}, y = { a: 1, ...x, b: 2, ...z });
assertEquals({a: 1, b: 2}, y = { a: 1, ...x, ...z, b: 2 });
assertEquals({a: 2, b: 2}, y = { ...x, ...z, a:2, b: 2 });

var x = {}
Object.defineProperty(x, 'a', {
  enumerable: false,
  configurable: false,
  writable: false,
  value: 1
});
assertEquals({}, { ...x });

var x = {}
Object.defineProperty(x, 'a', {
  enumerable: true,
  configurable: false,
  writable: false,
  value: 1
});
var y = { ...x };
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 1);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);

var x = { __proto__: z }
assertEquals({}, { ...x });

var x = {
  get a() { return 1; },
  set a(_) { assertUnreachable("setter called"); },
};
assertEquals({ a: 1 }, y = { ...x });

var x = {
  method() { return 1; },
};
assertEquals(x, y = { ...x });

var x = {
  *gen() { return {value: 1, done: true} ; },
};
assertEquals(x, y = { ...x });

var x = {
  get a() { throw new Error(); },
};
assertThrows(() => { y = { ...x } });

var p = new Proxy({}, {
  ownKeys() { throw new Error(); }
});
assertThrows(() => { y = { ...p } });

var p = new Proxy({}, {
  ownKeys() { [1]; },
  get() { throw new Error(); }
});
assertThrows(() => { y = { ...p } });

var p = new Proxy({}, {
  ownKeys() { [1]; },
  getOwnPropertyDescriptor() { throw new Error(); }
});
assertThrows(() => { y = { ...p } });

var p = new Proxy(z, {
  ownKeys() { return Object.keys(z); },
  get(_, prop) { return z[prop]; },
  getOwnPropertyDescriptor(_, prop) {
    return Object.getOwnPropertyDescriptor(z, prop);
  },
});
assertEquals(z, y = { ...p });

var x = { a:1 };
assertEquals(x, y = { set a(_) { throw new Error(); }, ...x });
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 1);
assertFalse("set" in prop);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);

var x = { a:2 };
assertEquals(x, y = { get a() { throw new Error(); }, ...x });
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 2);
assertFalse("get" in prop);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);

var x = { a:3 };
assertEquals(x, y = {
  get a() {
    throw new Error();
  },
  set a(_) {
    throw new Error();
  },
  ...x
});
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 3);
assertFalse("get" in prop);
assertFalse("set" in prop);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);

var x = Object.seal({ a:4 });
assertEquals(x, y = { ...x });
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 4);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);

var x = Object.freeze({ a:5 });
assertEquals(x, y = { ...x });
var prop = Object.getOwnPropertyDescriptor(y, 'a');
assertEquals(prop.value, 5);
assertTrue(prop.enumerable);
assertTrue(prop.configurable);
assertTrue(prop.writable);
