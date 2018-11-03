// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup = function(iter) {
  ++cleanup_call_count;
}

let wf = new WeakFactory(cleanup);
// Create an object and a WeakCell pointing to it. The object needs to be inside
// a closure so that we can reliably kill them!
let weak_cell;

(function() {
  let object = {};
  weak_cell = wf.makeCell(object);

  // Clear the WeakCell before the GC has a chance to discover it.
  weak_cell.clear();

  // Call clear again (just to assert we handle this gracefully).
  weak_cell.clear();

  // object goes out of scope.
})();

// This GC will discover dirty WeakCells.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function won't be called, since the WeakCell was cleared.
let timeout_func = function() {
  assertEquals(0, cleanup_call_count);
}

setTimeout(timeout_func, 0);
