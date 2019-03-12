// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup = function(iter) {
  if (cleanup_call_count == 0) {
    // First call: iterate 2 of the 3 cells
    let cells = [];
    for (wc of iter) {
      cells.push(wc);
      // Don't iterate the rest of the cells
      if (cells.length == 2) {
        break;
      }
    }
    assertEquals(cells.length, 2);
    assertTrue(cells[0].holdings < 3);
    assertTrue(cells[1].holdings < 3);
    // Update call count only after the asserts; this ensures that the test
    // fails even if the exceptions inside the cleanup function are swallowed.
    cleanup_call_count++;
  } else {
    // Second call: iterate one leftover cell and one new cell.
    assertEquals(1, cleanup_call_count);
    let cells = [];
    for (wc of iter) {
      cells.push(wc);
    }
    assertEquals(cells.length, 2);
    assertTrue((cells[0].holdings < 3 && cells[1].holdings == 100) ||
               (cells[1].holdings < 3 && cells[0].holdings == 100));
    // Update call count only after the asserts; this ensures that the test
    // fails even if the exceptions inside the cleanup function are swallowed.
    cleanup_call_count++;
  }
}

let wf = new WeakFactory(cleanup);
// Create 3 objects and WeakCells pointing to them. The objects need to be
// inside a closure so that we can reliably kill them!
let weak_cells = [];

(function() {
  let objects = [];

  for (let i = 0; i < 3; ++i) {
    objects[i] = {a: i};
    weak_cells[i] = wf.makeCell(objects[i], i);
  }

  gc();
  assertEquals(0, cleanup_call_count);

  // Drop the references to the objects.
  objects = [];
})();

// This GC will discover dirty WeakCells.
gc();
assertEquals(0, cleanup_call_count);

let timeout_func_1 = function() {
  assertEquals(1, cleanup_call_count);

  // Assert that the cleanup function won't be called unless new WeakCells appear.
  setTimeout(timeout_func_2, 0);
}

setTimeout(timeout_func_1, 0);

let timeout_func_2 = function() {
  assertEquals(1, cleanup_call_count);

  // Create a new WeakCells to be cleaned up.
  let obj = {};
  let wc = wf.makeCell(obj, 100);
  obj = null;

  gc();

  setTimeout(timeout_func_3, 0);
}

let timeout_func_3 = function() {
  assertEquals(2, cleanup_call_count);
}
