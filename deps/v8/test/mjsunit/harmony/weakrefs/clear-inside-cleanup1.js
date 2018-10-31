// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_weak_cell_count = 0;
let cleanup = function(iter) {
  // Clear the WeakCell before we've iterated through it.
  weak_cell.clear();

  for (wc of iter) {
    ++cleanup_weak_cell_count;
  }
  ++cleanup_call_count;
}

let wf = new WeakFactory(cleanup);
// Create an object and a WeakCell pointing to it. The object needs to be inside
// a closure so that we can reliably kill them!
let weak_cell;

(function() {
  let object = {};
  weak_cell = wf.makeCell(object);

  // object goes out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called, but didn't iterate any weak cells.
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(0, cleanup_weak_cell_count);
}

setTimeout(timeout_func, 0);
