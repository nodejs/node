// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-compaction --expose-gc

(async function () {

  // Test that the dirty FinalizationRegistries that are enqueued during GC have
  // their slots correctly recorded by the GC.

  // 1) Create many JSFinalizationRegistry objects so that they span several pages
  // (page size is 256kb).
  let registries = [];
  for (let i = 0; i < 1024 * 8; i++) {
    registries.push(new FinalizationRegistry(() => { }));
  }

  // 2) Force two GCs to ensure that JSFinalizatonRegistry objects are tenured.
  // Here and below, we need to invoke GC asynchronously and wait for it to
  // finish, so that it doesn't need to scan the stack. Otherwise, the objects
  // may not be reclaimed because of conservative stack scanning and the test
  // may not work as intended.
  await gc({ type: 'major', execution: 'async' });
  await gc({ type: 'major', execution: 'async' });

  // 3) In a function: create a dummy target and register it in all
  // JSFinalizatonRegistry objects.
  (function () {
    let garbage = {};
    registries.forEach((fr) => {
      fr.register(garbage, 42);
    });
    garbage = null;
  })();

  // 4) Outside the function where the target is unreachable: force GC to collect
  // the object.
  await gc({ type: 'major', execution: 'async' });

  // 5) Force another GC to test that the slot was correctly updated.
  await gc({ type: 'major', execution: 'async' });

})();
