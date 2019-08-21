// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup_holdings_count = 0;
let cleanup = function(iter) {
  // Unregister before we've iterated through the holdings.
  let success = fg.unregister(key);
  assertTrue(success);

  for (wc of iter) {
    ++cleanup_holdings_count;
  }
  ++cleanup_call_count;
}

let fg = new FinalizationGroup(cleanup);
let key = {"k": "the key"};
// Create an object and register it in the FinalizationGroup. The object needs
// to be inside a closure so that we can reliably kill them!

(function() {
  let object = {};
  fg.register(object, "holdings", key);

  // object goes out of scope.
})();

// This GC will discover unretained targets and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called, but didn't iterate any holdings.
let timeout_func = function() {
  assertEquals(1, cleanup_call_count);
  assertEquals(0, cleanup_holdings_count);
}

setTimeout(timeout_func, 0);
