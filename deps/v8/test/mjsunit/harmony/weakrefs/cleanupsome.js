// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking --allow-natives-syntax

let cleanup_count = 0;
let cleanup_holdings = [];
let cleanup = function(iter) {
  %AbortJS("shouldn't be called");
}

let cleanup2 = function(iter) {
  for (holdings of iter) {
    cleanup_holdings.push(holdings);
  }
  ++cleanup_count;
}

let fg = new FinalizationGroup(cleanup);
(function() {
  let o = {};
  fg.register(o, "holdings");

  // cleanupSome won't do anything since there are no reclaimed targets.
  fg.cleanupSome(cleanup2);
  assertEquals(0, cleanup_count);
})();

// GC will detect o as dead.
gc();

fg.cleanupSome(cleanup2);
assertEquals(1, cleanup_count);
assertEquals(1, cleanup_holdings.length);
assertEquals("holdings", cleanup_holdings[0]);
