// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-compaction --expose-gc

(async function () {

  // Test that the dirty FinalizationRegistries that are enqueued during GC have
  // their slots correctly recorded by the GC.

  // 1) Create many JSFinalizationRegistry objects.
  let registries = [];
  for (let i = 0; i < 64 * 8; i++) {
    registries.push(new FinalizationRegistry(() => { }));
  }

  // 2) In a function: create a dummy target and register it in all
  // JSFinalizatonRegistry objects.
  (function () {
    let garbage = {};
    registries.forEach((fr) => {
      fr.register(garbage, 42);
    });
    garbage = null;
  })();

  // 3) Outside the function where the target is unreachable: force GC to collect
  // the object.
  await gc({ type: 'minor', execution: 'async' });

  // 4) Force another GC to test that the slot was correctly updated.
  await gc({ type: 'minor', execution: 'async' });

})();
