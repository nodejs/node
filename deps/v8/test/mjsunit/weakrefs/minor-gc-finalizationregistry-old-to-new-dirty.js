// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --no-incremental-marking --no-single-generation --no-gc-global --no-minor-ms --handle-weak-ref-weakly-in-minor-gc --no-scavenger-chaos-mode --no-concurrent-inlining --allow-natives-syntax --scavenger-chaos-mode --scavenger-chaos-mode-threshold=0

(async function () {

  let fg1 = new FinalizationRegistry(_ => {});
  gc();
  assertFalse(%InYoungGeneration(fg1));

  (function () {
    fg1.register({}, "holdings1");
  })();
  // Schedule fg1 for cleanup.
  gc({ type: 'minor' });
  // Dirty finalization registry now contain one old finalization registrty.

  let fg2 = new FinalizationRegistry(_ => {});
  assertTrue(%InYoungGeneration(fg2));

  (function () {
    fg2.register({}, "holdings2");
  })();

  // Schedule fg2 for cleanup.
  gc({ type: 'minor' });
  // Dirty finalization registry now contains two finalziation registries, with a old-to-new reference.
  // Make sure GC doesn't crash when updating the dirty finalization registry list.
  gc({ type: 'minor' });
})();
