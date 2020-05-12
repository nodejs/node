// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_holdings_count = 0;
let cleanup = function(iter) {
  for (holdings of iter) {
    ++cleanup_holdings_count;
  }
  ++cleanup_call_count;
}

let fg1 = new FinalizationRegistry(cleanup);
let fg2 = new FinalizationRegistry(cleanup);

// Create two objects and register them in FinalizationRegistries. The objects need
// to be inside a closure so that we can reliably kill them!

(function() {
  let object1 = {};
  fg1.register(object1, "holdings1");

  let object2 = {};
  fg2.register(object2, "holdings2");

  // object1 and object2 go out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called and iterated the holdings.
let timeout_func = function() {
  assertEquals(2, cleanup_call_count);
  assertEquals(2, cleanup_holdings_count);
}

setTimeout(timeout_func, 0);
