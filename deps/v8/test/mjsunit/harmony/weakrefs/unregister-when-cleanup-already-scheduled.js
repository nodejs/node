// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup = function(iter) {
  ++cleanup_call_count;
}

let key = {"k": "this is my key"};
let fg = new FinalizationGroup(cleanup);
// Create an object and register it in the FinalizationGroup. The object needs to be inside
// a closure so that we can reliably kill them!

(function() {
  let object = {};
  fg.register(object, {}, key);

  // object goes out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Unregister the object from the FinalizationGroup before cleanup has ran.
fg.unregister(key);

// Assert that the cleanup function won't be called.
let timeout_func = function() {
  assertEquals(0, cleanup_call_count);
}

setTimeout(timeout_func, 0);
