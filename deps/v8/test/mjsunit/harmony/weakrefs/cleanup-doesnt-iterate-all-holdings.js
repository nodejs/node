// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup = function(iter) {
  print("in cleanup");
  if (cleanup_call_count == 0) {
    // First call: iterate 2 of the 3 holdings
    let holdings_list = [];
    for (holdings of iter) {
      holdings_list.push(holdings);
      // Don't iterate the rest of the holdings
      if (holdings_list.length == 2) {
        break;
      }
    }
    assertEquals(holdings_list.length, 2);
    assertTrue(holdings_list[0] < 3);
    assertTrue(holdings_list[1] < 3);
    // Update call count only after the asserts; this ensures that the test
    // fails even if the exceptions inside the cleanup function are swallowed.
    cleanup_call_count++;
  } else {
    // Second call: iterate one leftover holdings and one holdings.
    assertEquals(1, cleanup_call_count);
    let holdings_list = [];
    for (holdings of iter) {
      holdings_list.push(holdings);
    }
    assertEquals(holdings_list.length, 2);
    assertTrue((holdings_list[0] < 3 && holdings_list[1] == 100) ||
               (holdings_list[1] < 3 && holdings_list[0] == 100));
    // Update call count only after the asserts; this ensures that the test
    // fails even if the exceptions inside the cleanup function are swallowed.
    cleanup_call_count++;
  }
}

let fg = new FinalizationRegistry(cleanup);
// Create 3 objects and register them in the FinalizationRegistry. The objects need
// to be inside a closure so that we can reliably kill them!

(function() {
  let objects = [];

  for (let i = 0; i < 3; ++i) {
    objects[i] = {a: i};
    fg.register(objects[i], i);
  }

  gc();
  assertEquals(0, cleanup_call_count);

  // Drop the references to the objects.
  objects = [];
})();

// This GC will reclaim the targets.
gc();
assertEquals(0, cleanup_call_count);

let timeout_func_1 = function() {
  assertEquals(1, cleanup_call_count);

  // Assert that the cleanup function won't be called unless new targets appear.
  setTimeout(timeout_func_2, 0);
}

setTimeout(timeout_func_1, 0);

let timeout_func_2 = function() {
  assertEquals(1, cleanup_call_count);

  // Create a new object and register it.
  (function() {
    let obj = {};
    let wc = fg.register(obj, 100);
    obj = null;
  })();

  // This GC will reclaim the targets.
  gc();
  assertEquals(1, cleanup_call_count);

  setTimeout(timeout_func_3, 0);
}

let timeout_func_3 = function() {
  assertEquals(2, cleanup_call_count);
}
