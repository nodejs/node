// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testRestrictedPropertiesStrict() {
  function* generator() { "use strict"; }
  assertFalse(generator.hasOwnProperty("arguments"));
  assertThrows(function() { return generator.arguments; }, TypeError);
  assertThrows(function() { return generator.arguments = {}; }, TypeError);

  assertFalse(generator.hasOwnProperty("caller"));
  assertThrows(function() { return generator.caller; }, TypeError);
  assertThrows(function() { return generator.caller = {}; }, TypeError);
})();


(function testRestrictedPropertiesSloppy() {
  function* generator() {}
  assertFalse(generator.hasOwnProperty("arguments"));
  assertThrows(function() { return generator.arguments; }, TypeError);
  assertThrows(function() { return generator.arguments = {}; }, TypeError);

  assertFalse(generator.hasOwnProperty("caller"));
  assertThrows(function() { return generator.caller; }, TypeError);
  assertThrows(function() { return generator.caller = {}; }, TypeError);
})();

function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}


(function testIteratorResultStrict() {
  function* generator() { "use strict"; }
  assertIteratorResult(undefined, true, generator().next());
})();


(function testIteratorResultSloppy() {
  function* generator() {}
  assertIteratorResult(undefined, true, generator().next());
})();
