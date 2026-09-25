// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ns from "./modules-namespace-getownproperty2.mjs";

// This tests the same as modules-namespace-getownproperty1.mjs except that here
// variable a doesn't exist. This means that the late-declared variable b is the
// (alphabetically) first property of the namespace object, which makes a
// difference for some operations.


////////////////////////////////////////////////////////////////////////////////
// There are two exports, b and c (both let-declared).  Variable b is
// declared AFTER the first set of tests ran (see below).
export let c = 3;
////////////////////////////////////////////////////////////////////////////////


// for-in
assertThrows(() => { for (let p in ns) {} }, ReferenceError);

// Object.prototype.propertyIsEnumerable
assertThrows(() => Object.prototype.propertyIsEnumerable.call(ns, 'b'),
    ReferenceError);
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'c'));

// Object.prototype.hasOwnProperty
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
assertEquals(['b', 'c'], Object.getOwnPropertyNames(ns));

// Object.getOwnPropertySymbols
assertEquals([Symbol.toStringTag], Object.getOwnPropertySymbols(ns));

// Reflect.ownKeys
assertEquals(['b', 'c', Symbol.toStringTag], Reflect.ownKeys(ns));

// Object.assign
var copy = {};
assertThrows(() => Object.assign(copy, ns), ReferenceError);
assertEquals({}, copy);

// Object.isFrozen
assertThrows(() => Object.isFrozen(ns), ReferenceError);

// Object.isSealed
assertThrows(() => Object.isSealed(ns), ReferenceError);

// Object.freeze
assertThrows(() => Object.freeze(ns), ReferenceError);

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
var i = 2;
for (let p in ns) {
  assertEquals(i, ns[p]);
  i++
}
assertEquals(i, 4);

// Object.prototype.propertyIsEnumerable
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'b'));
assertTrue(Object.prototype.propertyIsEnumerable.call(ns, 'c'));

// Object.prototype.hasOwnProperty
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'b'));
assertTrue(Object.prototype.hasOwnProperty.call(ns, 'c'));

// Object.keys
assertEquals(['b', 'c'], Object.keys(ns));

// Object.entries
assertEquals([['b', 2], ['c', 3]], Object.entries(ns));

// Object.values
assertEquals([2, 3], Object.values(ns));

// Object.getOwnPropertyNames
assertEquals(['b', 'c'], Object.getOwnPropertyNames(ns));

// Object.getOwnPropertySymbols
assertEquals([Symbol.toStringTag], Object.getOwnPropertySymbols(ns));

// Reflect.ownKeys
assertEquals(['b', 'c', Symbol.toStringTag], Reflect.ownKeys(ns));

// Object.assign
copy = {};
Object.assign(copy, ns);
assertEquals({b:2, c:3}, copy);

// Object.isFrozen
assertFalse(Object.isFrozen(ns));

// Object.isSealed
assertTrue(Object.isSealed(ns));

// Object.freeze
assertThrows(() => Object.freeze(ns), TypeError);

// Object.seal
assertDoesNotThrow(() => Object.seal(ns));

// JSON.stringify
assertEquals('{"b":2,"c":3}', JSON.stringify(ns));

// PropertyDefinition
copy = {};
({...copy} = ns);
assertEquals({b:2, c:3}, copy);

// delete
assertThrows(() => delete ns.b, TypeError);
assertFalse(Reflect.deleteProperty(ns, 'b'));
