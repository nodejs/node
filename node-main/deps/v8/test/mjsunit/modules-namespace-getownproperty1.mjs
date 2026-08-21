// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from "./modules-namespace-getownproperty1.mjs";


////////////////////////////////////////////////////////////////////////////////
// There are three exports, a and b and c (all let-declared).  Variable b is
// declared AFTER the first set of tests ran (see below).
export let a = 1;
export let c = 3;
////////////////////////////////////////////////////////////////////////////////


// for-in
assertThrows(() => { for (let p in ns) {} }, ReferenceError);

// Object.prototype.propertyIsEnumerable
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'a'));
assertThrows(() => Object.prototype.propertyIsEnumerable.call(ns, 'b'),
    ReferenceError);
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'c'));

// Object.prototype.hasOwnProperty
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'a'));
assertThrows(() => Object.prototype.hasOwnProperty.call(ns, 'b'),
    ReferenceError);
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'c'));

// Object.keys
assertThrows(() => Object.keys(ns), ReferenceError);

// Object.entries
assertThrows(() => Object.entries(ns), ReferenceError);

// Object.values
assertThrows(() => Object.values(ns), ReferenceError);

// Object.getOwnPropertyNames
assertEquals(['a', 'b', 'c'], Object.getOwnPropertyNames(ns));

// Object.getOwnPropertySymbols
assertEquals([Symbol.toStringTag], Object.getOwnPropertySymbols(ns));

// Reflect.ownKeys
assertEquals(['a', 'b', 'c', Symbol.toStringTag], Reflect.ownKeys(ns));

// Object.assign
var copy = {};
assertThrows(() => Object.assign(copy, ns), ReferenceError);
assertEquals({a: 1}, copy);

// Object.isFrozen
assertFalse(Object.isFrozen(ns));

// Object.isSealed
assertThrows(() => Object.isSealed(ns), ReferenceError);

// Object.freeze
assertThrows(() => Object.freeze(ns), TypeError);

// Object.seal
assertThrows(() => Object.seal(ns), ReferenceError);

// JSON.stringify
assertThrows(() => JSON.stringify(ns), ReferenceError);

// PropertyDefinition
assertThrows(() => ({...copy} = ns), ReferenceError);

// delete
assertThrows(() => delete ns.b, TypeError);
assertFalse(Reflect.deleteProperty(ns, 'b'));


////////////////////////////////////////////////////////////////////////////////
// Variable b is declared here.
export let b = 2;
////////////////////////////////////////////////////////////////////////////////


// for-in
var i = 1;
for (let p in ns) {
  assertEquals(i, ns[p]);
  i++
}
assertEquals(i, 4);

// Object.prototype.propertyIsEnumerable
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'a'));
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'b'));
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'c'));

// Object.prototype.hasOwnProperty
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'a'));
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'b'));
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'c'));

// Object.keys
assertEquals(['a', 'b', 'c'], Object.keys(ns));

// Object.entries
assertEquals([['a', 1], ['b', 2], ['c', 3]], Object.entries(ns));

// Object.values
assertEquals([1, 2, 3], Object.values(ns));

// Object.getOwnPropertyNames
assertEquals(['a', 'b', 'c'], Object.getOwnPropertyNames(ns));

// Object.getOwnPropertySymbols
assertEquals([Symbol.toStringTag], Object.getOwnPropertySymbols(ns));

// Reflect.ownKeys
assertEquals(['a', 'b', 'c', Symbol.toStringTag], Reflect.ownKeys(ns));

// Object.assign
copy = {};
Object.assign(copy, ns);
assertEquals({a: 1, b:2, c:3}, copy);

// Object.isFrozen
assertFalse(Object.isFrozen(ns));

// Object.isSealed
assertTrue(Object.isSealed(ns));

// Object.freeze
assertThrows(() => Object.freeze(ns), TypeError);

// Object.seal
assertDoesNotThrow(() => Object.seal(ns));

// JSON.stringify
assertEquals('{"a":1,"b":2,"c":3}', JSON.stringify(ns));

// PropertyDefinition
copy = {};
({...copy} = ns);
assertEquals({a: 1, b:2, c:3}, copy);

// delete
assertThrows(() => delete ns.b, TypeError);
assertFalse(Reflect.deleteProperty(ns, 'b'));
