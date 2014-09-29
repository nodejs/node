// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-generators

function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}

function test(f) {
  var cdesc = Object.getOwnPropertyDescriptor(f, "caller");
  var adesc = Object.getOwnPropertyDescriptor(f, "arguments");

  assertFalse(cdesc.enumerable);
  assertFalse(cdesc.configurable);

  assertFalse(adesc.enumerable);
  assertFalse(adesc.configurable);

  assertSame(cdesc.get, cdesc.set);
  assertSame(cdesc.get, adesc.get);
  assertSame(cdesc.get, adesc.set);

  assertTrue(cdesc.get instanceof Function);
  assertEquals(0, cdesc.get.length);
  assertThrows(cdesc.get, TypeError);

  assertThrows(function() { return f.caller; }, TypeError);
  assertThrows(function() { f.caller = 42; }, TypeError);
  assertThrows(function() { return f.arguments; }, TypeError);
  assertThrows(function() { f.arguments = 42; }, TypeError);
}

function *sloppy() { test(sloppy); }
function *strict() { "use strict"; test(strict); }

test(sloppy);
test(strict);

assertIteratorResult(undefined, true, sloppy().next());
assertIteratorResult(undefined, true, strict().next());
