// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --handle-weak-ref-weakly-in-minor-gc

const cleanup = function (holdings) { globalThis.FRRan = true; };
const FR = new FinalizationRegistry(cleanup);

(function () {
  let obj = {};
  // obj is its own unregister token and becomes unreachable after this
  // block. If the unregister token is held strongly this test will not
  // terminate.
  FR.register(obj, 42, obj);
})();

function tryAgain() {
  (async function () {
    // We need to invoke GC asynchronously and wait for it to finish, so that
    // it doesn't need to scan the stack. Otherwise, the objects may not be
    // reclaimed because of conservative stack scanning and the test may not
    // work as intended.
    await gc({ type: 'minor', execution: 'async' });

    if (globalThis.FRRan) {
      return;
    }

    setTimeout(tryAgain, 0);
  })();
}
tryAgain();
