// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --noincremental-marking --no-concurrent-inlining

let cleanup_called = false;
function cleanup(holdings) {
  cleanup_called = true;
};
let cleanup_called_2 = false;
function cleanup2(holdings) {
  cleanup_called_2 = true;
};
let fg = new FinalizationRegistry(cleanup);
(function() {
  let fg2 = new FinalizationRegistry(cleanup2);
  (function() {
    fg.register({}, {});
    fg2.register({}, {});
  })();
  // Schedule fg and fg2 for cleanup.
  gc();
})();

// Collect fg2, but fg is still alive.
gc();

setTimeout(function() {
  assertTrue(cleanup_called);
  assertFalse(cleanup_called_2);
}, 0);
