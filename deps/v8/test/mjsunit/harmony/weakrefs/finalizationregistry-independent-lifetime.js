// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

let cleanup_called = false;
function cleanup(holdings) {
  cleanup_called = true;
};
(function() {
  let fg = new FinalizationRegistry(cleanup);
  (function() {
    let x = {};
    fg.register(x, {});
    x = null;
  })();
  // Schedule fg for cleanup.
  gc();
})();

// Collect fg, which should result in cleanup not called.
gc();

setTimeout(function() { assertFalse(cleanup_called); }, 0);
