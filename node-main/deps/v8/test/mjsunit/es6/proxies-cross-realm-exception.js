// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Do not read out the prototype from a cross-realm object.
var realm = Realm.create();

__proto__ = {};
assertEquals(null,
  Realm.eval(realm, "3; Reflect.getPrototypeOf(Realm.global(0))"));
assertFalse(Realm.eval(realm, "3; Realm.global(0) instanceof Object"));

__proto__ = new Proxy({}, { getPrototypeOf() { assertUnreachable() } });
assertEquals(null,
    Realm.eval(realm, "1; Reflect.getPrototypeOf(Realm.global(0))"));
assertFalse(Realm.eval(realm, "1; Realm.global(0) instanceof Object"));

// Test that the instannceof check works in optimized code.
var test = Realm.eval(realm,
    "()=>{1.1; return Realm.global(0) instanceof Object; }");
%PrepareFunctionForOptimization(test);
assertFalse(test());
test();
test();
%OptimizeFunctionOnNextCall(test);
assertFalse(test());

__proto__ = {};
__proto__ = new Proxy({}, { get(t, p, r) { assertUnreachable() } });
assertEquals(null,
    Realm.eval(realm, "2; Reflect.getPrototypeOf(Realm.global(0))"));
assertFalse(Realm.eval(realm, "2; Realm.global(0) instanceof Object"));


__proto__ = {};
__proto__.__proto__ = new Proxy({}, {
  getPrototypeOf() { assertUnreachable() }
});
assertEquals(null,
  Realm.eval(realm, "4; Reflect.getPrototypeOf(Realm.global(0))"));
assertFalse(Realm.eval(realm, "4; Realm.global(0) instanceof Object"));

// 2-level proxy indirection
__proto__ = {};
__proto__ = new Proxy({},
  new Proxy({}, {
    get() { assertUnreachable() }
  })
);
assertEquals(null,
  Realm.eval(realm, "5; Reflect.getPrototypeOf(Realm.global(0))"));
assertFalse(Realm.eval(realm, "5; Realm.global(0) instanceof Object"));
