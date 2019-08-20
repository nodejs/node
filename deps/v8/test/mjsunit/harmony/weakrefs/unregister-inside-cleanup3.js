// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_holdings_count = 0;
let cleanup = function(iter) {
  for (holdings of iter) {
    assertEquals(holdings, "holdings");
    ++cleanup_holdings_count;
  }
  // Unregister an already iterated over weak reference.
  let success = fg.unregister(key);
  assertFalse(success);
  ++cleanup_call_count;
}

let fg = new FinalizationGroup(cleanup);
let key = {"k": "this is the key"};

// Create an object and register it in the FinalizationGroup. The object needs to be inside
// a closure so that we can reliably kill them!

(function() {
  let object = {};
  fg.register(object, "holdings", key);

  // object goes out of scope.
})();

// This GC will reclaim the target object and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called and iterated the holdings.
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(1, cleanup_holdings_count);
}

setTimeout(timeout_func, 0);
