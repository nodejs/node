// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

(async function() {
  let callback_called = false;
  let fr = new FinalizationRegistry(holdings => {
    callback_called = true;
  });

  (function() {
    let target = {};
    let unregister_token = {};
    fr.register(target, "holdings", unregister_token);

    gc({ type: 'minor' });
    assertFalse(callback_called);

    // Make target and unregister_token unreachable.
    target = null;
    unregister_token = null;
  })();

  setTimeout(() => { assertFalse(callback_called); }, 0);

  await gc({ type: 'minor', execution: 'async' });

  assertFalse(callback_called);
  setTimeout(() => { assertTrue(callback_called); }, 0);
})();
