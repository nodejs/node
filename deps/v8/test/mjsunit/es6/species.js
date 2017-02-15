// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the ES2015 @@species feature

'use strict';

let TypedArray = Uint8Array.__proto__;

// The @@species property exists on the right objects and has the right values

let classesWithSpecies = [RegExp, Array, TypedArray, ArrayBuffer, Map, Set, Promise];
let classesWithoutSpecies = [Object, Function, String, Number, Symbol, WeakMap, WeakSet];

for (let constructor of classesWithSpecies) {
  assertEquals(constructor, constructor[Symbol.species]);
  assertThrows(function() { constructor[Symbol.species] = undefined }, TypeError);
  let descriptor = Object.getOwnPropertyDescriptor(constructor, Symbol.species);
  assertTrue(descriptor.configurable);
  assertFalse(descriptor.enumerable);
  assertEquals(undefined, descriptor.writable);
  assertEquals(undefined, descriptor.set);
  assertEquals('function', typeof descriptor.get);
}

// @@species is defined with distinct getters
assertEquals(classesWithSpecies.length,
             new Set(classesWithSpecies.map(constructor =>
                                            Object.getOwnPropertyDescriptor(
                                              constructor, Symbol.species).get)
                ).size);

for (let constructor of classesWithoutSpecies)
  assertEquals(undefined, constructor[Symbol.species]);
