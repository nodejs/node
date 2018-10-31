// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

// Test that WeakCell.prototype.clear() also clears the WeakFactory pointer of
// WeakCell. The only way to observe this is to assert that the WeakCell no
// longer keeps its WeakFactory alive after clear() has been called.

let weak_cell;
let weak_cell_pointing_to_factory;

let cleanup1_call_count = 0;
let cleanup2_call_count = 0;

let cleanup1 = function() {
  ++cleanup1_call_count;
}

let cleanup2 = function() {
  ++cleanup2_call_count;
}

let wf1 = new WeakFactory(cleanup1);

(function(){
  let wf2 = new WeakFactory(cleanup2);

  (function() {
    let object = {};
    weak_cell = wf2.makeCell(object);
    // object goes out of scope.
  })();

  weak_cell_pointing_to_factory = wf1.makeCell(wf2);
  // wf goes out of scope
})();

weak_cell.clear();
gc();

// Assert that weak_cell_pointing_to_factory now got cleared.
let timeout_func = function() {
  assertEquals(1, cleanup1_call_count);
  assertEquals(0, cleanup2_call_count);
}

setTimeout(timeout_func, 0);
