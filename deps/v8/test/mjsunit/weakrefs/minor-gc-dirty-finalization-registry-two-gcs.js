// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc --allow-natives-syntax

(async function() {
  let callback_called = false;
  let fr = new FinalizationRegistry(holdings => {
    callback_called = true;
  });

  (function() {
    let target = {};
    fr.register(target, "holdings");
    target = null;
  })();

  // `fr` is marked dirty and scheduled for cleanup.
  assertTrue(%InYoungGeneration(fr));
  await gc({ type: 'minor', execution: 'async' });
  assertFalse(callback_called);

  // `fr` is moved. Unless the dirty finalization registry list is updated, the
  // cleanup task will crash.
  assertTrue(%InYoungGeneration(fr));
  gc({ type: 'minor' });

  assertFalse(callback_called);
  // Give the cleanup task a chance to run.
  await new Promise(resolve=>setTimeout(resolve, 0));
  assertTrue(callback_called);
})();
