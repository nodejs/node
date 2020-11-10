// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

let seg = new Intl.Segmenter();
let descriptor = Object.getOwnPropertyDescriptor(
    Intl.Segmenter, "supportedLocalesOf");
assertTrue(descriptor.writable);
assertFalse(descriptor.enumerable);
assertTrue(descriptor.configurable);

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

let segmentIterator = seg.segment('text');
let prototype = Object.getPrototypeOf(segmentIterator);
for (let func of ["next", "following", "preceding"]) {
  let descriptor = Object.getOwnPropertyDescriptor(prototype, func);
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

// Test the descriptor is correct for properties.
checkGetterProperty(prototype, 'index');
checkGetterProperty(prototype, 'breakType');

// Test the SegmentIteratorPrototype methods are called with same
// receiver and won't throw.
assertDoesNotThrow(() => prototype.next.call(segmentIterator));
assertDoesNotThrow(() => prototype.following.call(segmentIterator));
assertDoesNotThrow(() => prototype.preceding.call(segmentIterator));

// Test the SegmentIteratorPrototype methods are called with a different
// receiver and correctly throw.
var otherReceivers = [
    1, 123.45, undefined, null, "string", true, false,
    Intl, Intl.Segmenter, Intl.Segmenter.prototype,
    prototype,
    new Intl.Segmenter(),
    new Intl.Collator(),
    new Intl.DateTimeFormat(),
    new Intl.NumberFormat(),
];
for (let rec of otherReceivers) {
   assertThrows(() => prototype.next.call(rec), TypeError);
   assertThrows(() => prototype.following.call(rec), TypeError);
   assertThrows(() => prototype.preceding.call(rec), TypeError);
}

// Check the property of the return object of next()
let nextReturn = segmentIterator.next();

function checkProperty(obj, property) {
  let desc = Object.getOwnPropertyDescriptor(obj, property);
  assertTrue(desc.writable);
  assertTrue(desc.enumerable);
  assertTrue(desc.configurable);
}

checkProperty(nextReturn, 'done');
checkProperty(nextReturn, 'value');
checkProperty(nextReturn.value, 'segment');
checkProperty(nextReturn.value, 'breakType');
checkProperty(nextReturn.value, 'index');
