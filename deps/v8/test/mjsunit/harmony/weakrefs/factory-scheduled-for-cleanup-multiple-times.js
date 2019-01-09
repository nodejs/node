// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking
// Flags: --no-stress-flush-bytecode

let cleanup0_call_count = 0;
let cleanup0_weak_cell_count = 0;

let cleanup1_call_count = 0;
let cleanup1_weak_cell_count = 0;

let cleanup0 = function(iter) {
  for (wc of iter) {
    ++cleanup0_weak_cell_count;
  }
  ++cleanup0_call_count;
}

let cleanup1 = function(iter) {
  for (wc of iter) {
    ++cleanup1_weak_cell_count;
  }
  ++cleanup1_call_count;
}

let wf0 = new WeakFactory(cleanup0);
let wf1 = new WeakFactory(cleanup1);

// Create 1 WeakCell for each WeakFactory and kill the objects they point to.
(function() {
  // The objects need to be inside a closure so that we can reliably kill them.
  let objects = [];
  objects[0] = {};
  objects[1] = {};

  wf0.makeCell(objects[0]);
  wf1.makeCell(objects[1]);

  // Drop the references to the objects.
  objects = [];

  // Will schedule both wf0 and wf1 for cleanup.
  gc();
})();

// Before the cleanup task has a chance to run, do the same thing again, so both
// factories are (again) scheduled for cleanup. This has to be a IIFE function
// (so that we can reliably kill the objects) so we cannot use the same function
// as before.
(function() {
  let objects = [];
  objects[0] = {};
  objects[1] = {};
  wf0.makeCell(objects[0]);
  wf1.makeCell(objects[1]);
  objects = [];
  gc();
})();

let timeout_func = function() {
  assertEquals(1, cleanup0_call_count);
  assertEquals(2, cleanup0_weak_cell_count);
  assertEquals(1, cleanup1_call_count);
  assertEquals(2, cleanup1_weak_cell_count);
}

// Give the cleanup task a chance to run. All WeakCells to cleanup will be
// available during the same invocation of the cleanup function.
setTimeout(timeout_func, 0);
