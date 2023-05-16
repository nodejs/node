// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let object = { foo: false };
assertTrue((Object.hasOwn(object, "foo")));

let object2 = Object.create({ foo: true });
assertFalse(Object.hasOwn(object2, "foo"));

let object3 = Object.create(null);
assertFalse(Object.hasOwn(object3, "foo"));

let O = function() {
  this.foo = 42;
};
let object8 = new O;
assertTrue((Object.hasOwn(object8, "foo")));

// Object is undefined or null
assertThrows(() => Object.hasOwn(undefined). TypeError);
assertThrows(() => Object.hasOwn(null). TypeError);

// Using defineProperty
let object4 = {};
Object.defineProperty(object4, "foo", {
  value: 42
});
assertTrue((Object.hasOwn(object4, "foo")));

// Object.defineProperty with base
let base = {};
Object.defineProperty(base, "foo", {
  value: 42,
  configurable: true
});
let object5 = Object.create(base);
assertTrue((Object.hasOwn(base, "foo")));
assertFalse((Object.hasOwn(object5, "foo")));

// Prototype with getter and/or setter
let object6 = {
  get foo() {}
};
let object7 = {
  set foo(x) {}
};
assertTrue((Object.hasOwn(object6, "foo")));
assertTrue((Object.hasOwn(object7, "foo")));
