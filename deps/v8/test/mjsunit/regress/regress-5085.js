// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

g = async function() {
  await 10;
};
assertEquals(undefined, g.prototype);
g();
assertEquals(undefined, g.prototype);

gen = function*() {
  yield 10;
};
assertTrue(gen.prototype != undefined && gen.prototype != null);
gen();
assertTrue(gen.prototype != undefined && gen.prototype != null);

async_gen = async function*() {
  yield 10;
};
assertTrue(async_gen.prototype != undefined && async_gen.prototype != null);
async_gen();
assertTrue(async_gen.prototype != undefined && async_gen.prototype != null);

function foo(x) {
  return x instanceof Proxy;
};
%PrepareFunctionForOptimization(foo);
function test_for_exception() {
  caught_exception = false;
  try {
    foo({});
  } catch (e) {
    caught_exception = true;
    assertEquals(
        'Function has non-object prototype \'undefined\' in instanceof check',
        e.message);
  } finally {
    assertTrue(caught_exception);
  }
}

test_for_exception();
test_for_exception();
%OptimizeFunctionOnNextCall(foo);
test_for_exception();

Proxy.__proto__.prototype = Function.prototype;
assertTrue((() => {}) instanceof Proxy);

assertEquals(
    new Proxy({}, {
      get(o, s) {
        return s;
      }
    }).test,
    'test');

Proxy.__proto__ = {
  prototype: {b: 2},
  a: 1
};

assertEquals(Proxy.prototype, {b: 2});

(function testProxyCreationContext() {
  let realm = Realm.create();
  let p1 = new Proxy({}, {});
  let p2 = Realm.eval(realm, "new Proxy({}, {})");
  assertEquals(0, Realm.owner(p1));
  assertEquals(1, Realm.owner(p2));
})();
