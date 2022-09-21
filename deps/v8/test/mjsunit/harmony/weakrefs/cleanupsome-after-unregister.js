// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs-with-cleanup-some --expose-gc --noincremental-marking

let cleanup_count = 0;
let cleanup_holdings = [];
let cleanup = function(holdings) {
  cleanup_holdings.push(holdings);
  ++cleanup_count;
}

let fg = new FinalizationRegistry(cleanup);
let key = {"k": "this is the key"};
(function() {
  let o = {};
  weak_cell = fg.register(o, "holdings", key);

  // cleanupSome won't do anything since there are no reclaimed targets.
  fg.cleanupSome();
  assertEquals(0, cleanup_count);
  return o;
})();

// GC will detect the WeakCell as dirty.
gc();

// Unregister the tracked object just before calling cleanupSome.
fg.unregister(key);

fg.cleanupSome();

assertEquals(0, cleanup_count);
