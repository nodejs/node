// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let ja = 42;
export {ja as yo};
export const bla = "blaa";
export {foo as foo_again};
// See further below for the actual star import that declares "foo".

// The object itself.
assertEquals("object", typeof foo);
assertThrows(() => foo = 666, TypeError);
assertFalse(Reflect.isExtensible(foo));
assertTrue(Reflect.preventExtensions(foo));
assertThrows(() => Reflect.apply(foo, {}, []));
assertThrows(() => Reflect.construct(foo, {}, []));
assertSame(null, Reflect.getPrototypeOf(foo));
assertTrue(Reflect.setPrototypeOf(foo, null));
assertFalse(Reflect.setPrototypeOf(foo, {}));
assertSame(null, Reflect.getPrototypeOf(foo));
assertEquals(
    ["bla", "foo_again", "yo", Symbol.toStringTag], Reflect.ownKeys(foo));

// Its "yo" property.
assertEquals(
    {value: 42, enumerable: true, configurable: false, writable: true},
    Reflect.getOwnPropertyDescriptor(foo, "yo"));
assertFalse(Reflect.deleteProperty(foo, "yo"));
assertTrue(Reflect.has(foo, "yo"));
assertFalse(Reflect.set(foo, "yo", true));
// TODO(neis): The next two should be False.
assertTrue(Reflect.defineProperty(foo, "yo",
    Reflect.getOwnPropertyDescriptor(foo, "yo")));
assertTrue(Reflect.defineProperty(foo, "yo", {}));
assertFalse(Reflect.defineProperty(foo, "yo", {get() {return 1}}));
assertEquals(42, Reflect.get(foo, "yo"));
assertEquals(43, (ja++, foo.yo));

// Its "foo_again" property.
assertSame(foo, foo.foo_again);

// Its @@toStringTag property.
assertTrue(Reflect.has(foo, Symbol.toStringTag));
assertEquals("string", typeof Reflect.get(foo, Symbol.toStringTag));
assertEquals(
    {value: "Module", configurable: false, writable: false, enumerable: false},
    Reflect.getOwnPropertyDescriptor(foo, Symbol.toStringTag));
assertFalse(Reflect.deleteProperty(foo, Symbol.toStringTag));
assertEquals(
    {value: "Module", configurable: false, writable: false, enumerable: false},
    Reflect.getOwnPropertyDescriptor(foo, Symbol.toStringTag));

// Nonexistent properties.
let nonexistent = ["gaga", 123, Symbol('')];
for (let key of nonexistent) {
  assertSame(undefined, Reflect.getOwnPropertyDescriptor(foo, key));
  assertTrue(Reflect.deleteProperty(foo, key));
  assertFalse(Reflect.set(foo, key, true));
  assertSame(undefined, Reflect.get(foo, key));
  assertFalse(Reflect.defineProperty(foo, key, {get() {return 1}}));
  assertFalse(Reflect.has(foo, key));
}

// The actual star import that we are testing. Namespace imports are
// initialized before evaluation.
import * as foo from "modules-namespace1.mjs";

// There can be only one namespace object.
import * as bar from "modules-namespace1.mjs";
assertSame(foo, bar);
