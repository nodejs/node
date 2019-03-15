// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_call_count = 0;
let cleanup = function(iter) {
  ++cleanup_call_count;
}

let fg = new FinalizationGroup(cleanup);
let key = {"k": "this is the key"};
// Create an object and register it in the FinalizationGroup. The object needs
// to be inside a closure so that we can reliably kill them!

(function() {
  let object = {};
  fg.register(object, "holdings", key);

  // Unregister before the GC has a chance to discover the object.
  fg.unregister(key);

  // Call unregister again (just to assert we handle this gracefully).
  fg.unregister(key);

  // object goes out of scope.
})();

// This GC will reclaim the target object.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function won't be called, since the weak reference
// was unregistered.
let timeout_func = function() {
  assertEquals(0, cleanup_call_count);
}

setTimeout(timeout_func, 0);
