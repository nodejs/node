// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let seg = new Intl.Segmenter();
let descriptor = Object.getOwnPropertyDescriptor(
    Intl.Segmenter, "supportedLocalesOf");
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

let segmenterPrototype = Object.getPrototypeOf(seg);
assertEquals("Intl.Segmenter", segmenterPrototype[Symbol.toStringTag]);

// ecma402 #sec-Intl.Segmenter.prototype
// Intl.Segmenter.prototype
// The value of Intl.Segmenter.prototype is %SegmenterPrototype%.
// This property has the attributes
// { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: false }.
descriptor = Object.getOwnPropertyDescriptor(Intl.Segmenter, "prototype");
assertFalse(descriptor.writable);
assertFalse(descriptor.enumerable);
assertFalse(descriptor.configurable);

for (let func of ["segment", "resolvedOptions"]) {
  let descriptor = Object.getOwnPropertyDescriptor(
      Intl.Segmenter.prototype, func);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);
}


let segments = seg.segment('text');
let segmentsPrototype = Object.getPrototypeOf(segments);
for (let func of ["containing", Symbol.iterator]) {
  let descriptor = Object.getOwnPropertyDescriptor(segmentsPrototype, func);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);
}

function checkGetterProperty(prototype, property) {
  let desc = Object.getOwnPropertyDescriptor(prototype, property);
  assertEquals(`get ${property}`, desc.get.name);
  assertEquals('function', typeof desc.get)
  assertEquals(undefined, desc.set);
  assertFalse(desc.enumerable);
  assertTrue(desc.configurable);
}

// Test the SegmentsPrototype methods are called with same
// receiver and won't throw.
assertDoesNotThrow(() => segmentsPrototype.containing.call(segments));
assertDoesNotThrow(() => segmentsPrototype[Symbol.iterator].call(segments));

// Test the SegmentIteratorPrototype methods are called with a different
// receiver and correctly throw.
var otherReceivers = [
    1, 123.45, undefined, null, "string", true, false,
    Intl, Intl.Segmenter, Intl.Segmenter.prototype,
    segmentsPrototype,
    new Intl.Segmenter(),
    new Intl.Collator(),
    new Intl.DateTimeFormat(),
    new Intl.NumberFormat(),
];
for (let rec of otherReceivers) {
   assertThrows(() => segmentsPrototype.containing.call(rec), TypeError);
}

let segmentIterator = segments[Symbol.iterator]();
let segmentIteratorPrototype = Object.getPrototypeOf(segmentIterator);
for (let func of ["next"]) {
  let descriptor = Object.getOwnPropertyDescriptor(segmentIteratorPrototype,
      func);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);
}

assertEquals("Segmenter String Iterator",
    segmentIteratorPrototype[Symbol.toStringTag]);
let desc = Object.getOwnPropertyDescriptor(
    segmentIteratorPrototype, Symbol.toStringTag);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertTrue(desc.configurable);

let nextReturn = segmentIterator.next();

function checkProperty(obj, property) {
  let desc = Object.getOwnPropertyDescriptor(obj, property);
  assertTrue(desc.writable);
  assertTrue(desc.enumerable);
  assertTrue(desc.configurable);
}
