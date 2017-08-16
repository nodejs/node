// // Copyright 2016 the V8 project authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

// Flags: --harmony-object-rest-spread
var { ...x } = { a: 1 };
assertEquals({ a: 1 }, x);

var { ...x } = { a: 1, b: 1 };
assertEquals({ a: 1, b: 1 }, x);

var { x, ...x } = { a: 1, b: 1 };
assertEquals({ a: 1, b: 1 }, x);

var { x = {}, ...x } = { a: 1, b: 1 };
assertEquals({ a: 1, b: 1 }, x);

var { y, ...x } = { y: 1, a: 1 };
assertEquals({ a: 1 }, x);
assertEquals(1, y);

var { z, y, ...x } = { z:1, y: 1, a: 1, b: 1 };
assertEquals({ a: 1, b:1 }, x);
assertEquals(1, y);
assertEquals(1, z);

({ a, ...b } = { a: 1, b: 2 });
assertEquals(1, a);
assertEquals({ b: 2 }, b);

var { ...x } = {};
assertEquals({}, x);

var key = "b";
var { [key]: y, ...x } = { b: 1, a: 1 };
assertEquals({ a: 1 }, x);
assertEquals(1, y);

var key = 1;
var { [key++]: y, ...x } = { 1: 1, a: 1 };
assertEquals({ a: 1 }, x);
assertEquals(key, 2);
assertEquals(1, y);

var key = '1';
var {[key]: y, ...x} = {1: 1, a: 1};
assertEquals({a: 1}, x);
assertEquals(1, y);

function example({a, ...rest}, { b = rest }) {
    assertEquals(1, a);
    assertEquals({ b: 2, c: 3}, rest);
    assertEquals({ b: 2, c: 3}, b);
};
example({ a: 1, b: 2, c: 3}, { b: undefined });

var x = { a: 3 };
var y = {
  set a(val) { assertUnreachable(); },
  ...x,
};
assertEquals(y.a, 3);

var {...y} = {
  get a() {
    return 1
  }
};
assertEquals({a: 1}, y);

var x = {
  get a() { throw new Error(); },
};
assertThrows(() => { var { ...y } = x });

var p = new Proxy({}, {
  ownKeys() { throw new Error(); }
});
assertThrows(() => { var { ...y } = p });

var p = new Proxy({}, {
  ownKeys() { [1]; },
  get() { throw new Error(); }
});
assertThrows(() => { var { ...y } = p });

var p = new Proxy({}, {
  ownKeys() { [1]; },
  getOwnPropertyDescriptor() { throw new Error(); }
});
assertThrows(() => { var { ...y } = p });

var z = { b: 1}
var p = new Proxy(z, {
  ownKeys() { return Object.keys(z); },
  get(_, prop) { return z[prop]; },
  getOwnPropertyDescriptor(_, prop) {
    return Object.getOwnPropertyDescriptor(z, prop);
  },
});
var { ...y } = p ;
assertEquals(z, y);

var z = { b: 1}
var { ...y } =  { ...z} ;
assertEquals(z, y);

var count = 0;
class Foo {
  constructor(x) { this.x = x; }
  toString() { count++; return this.x.toString(); }
}
var f = new Foo(1);
var { [f] : x, ...y } = { 1: 1, 2: 2}
assertEquals(1, count);
assertEquals({2: 2}, y);

var { 1: x, 2: y, ...z } = { 1: 1, 2: 2, 3:3 };
assertEquals(1, x);
assertEquals(2, y);
assertEquals({ 3: 3 }, z);

var { 1.5: x, 2: y, ...z } = { 1.5: 1, 2: 2, 3:3 };
assertEquals(1, x);
assertEquals(2, y);
assertEquals({ 3: 3 }, z);

(({x, ...z}) => { assertEquals({y: 1}, z); })({ x: 1, y: 1});

var [...{...z}] = [{ x: 1}];
assertEquals({ 0: { x: 1} }, z);

var {...{x}} = { x: 1};
assertEquals(1, x);

var {4294967297: y, ...x} = {4294967297: 1, x: 1};
assertEquals(1, y);
assertEquals({x: 1}, x);

var obj = {
  [Symbol.toPrimitive]() {
    return 1;
  }
};
var {[obj]: y, ...x} = {1: 1, x: 1};
assertEquals(1, y);
assertEquals({x: 1}, x);

var {[null]: y, ...x} = {null: 1, x: 1};
assertEquals(1, y);
assertEquals({x: 1}, x);

var {[true]: y, ...x} = {true: 1, x: 1};
assertEquals(1, y);
assertEquals({x: 1}, x);

var {[false]: y, ...x} = {false: 1, x: 1};
assertEquals(1, y);
assertEquals({x: 1}, x);
