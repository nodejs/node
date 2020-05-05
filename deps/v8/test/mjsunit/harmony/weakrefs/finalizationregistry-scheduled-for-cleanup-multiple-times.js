// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking
// Flags: --no-stress-flush-bytecode

let cleanup0_call_count = 0;
let cleanup0_holdings_count = 0;

let cleanup1_call_count = 0;
let cleanup1_holdings_count = 0;

let cleanup0 = function(iter) {
  for (holdings of iter) {
    ++cleanup0_holdings_count;
  }
  ++cleanup0_call_count;
}

let cleanup1 = function(iter) {
  for (holdings of iter) {
    ++cleanup1_holdings_count;
  }
  ++cleanup1_call_count;
}

let fg0 = new FinalizationRegistry(cleanup0);
let fg1 = new FinalizationRegistry(cleanup1);

// Register 1 weak reference for each FinalizationRegistry and kill the objects they point to.
(function() {
  // The objects need to be inside a closure so that we can reliably kill them.
  let objects = [];
  objects[0] = {};
  objects[1] = {};

  fg0.register(objects[0], "holdings0-0");
  fg1.register(objects[1], "holdings1-0");

  // Drop the references to the objects.
  objects = [];
})();

// Will schedule both fg0 and fg1 for cleanup.
gc();

// Before the cleanup task has a chance to run, do the same thing again, so both
// FinalizationRegistries are (again) scheduled for cleanup. This has to be a IIFE function
// (so that we can reliably kill the objects) so we cannot use the same function
// as before.
(function() {
  let objects = [];
  objects[0] = {};
  objects[1] = {};
  fg0.register(objects[0], "holdings0-1");
  fg1.register(objects[1], "holdings1-1");
  objects = [];
})();

gc();

let timeout_func = function() {
  assertEquals(1, cleanup0_call_count);
  assertEquals(2, cleanup0_holdings_count);
  assertEquals(1, cleanup1_call_count);
  assertEquals(2, cleanup1_holdings_count);
}

// Give the cleanup task a chance to run. All holdings will be iterated during
// the same invocation of the cleanup function.
setTimeout(timeout_func, 0);
