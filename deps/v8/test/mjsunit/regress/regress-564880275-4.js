// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --expose-gc --verify-heap

// JS_BUNDLE_MODULE:cycle_c.mjs
throw new Error('sync error');

// JS_BUNDLE_MODULE:cycle_b.mjs
import './cycle_a.mjs';
await new Promise(() => {});

// JS_BUNDLE_MODULE:cycle_a.mjs
import './cycle_b.mjs';
import './cycle_c.mjs';

// JS_BUNDLE_MODULE:sync_term_tla.mjs
d8.terminate();
(() => {})();
await 0;

// JS_BUNDLE_MODULE_ENTRYPOINT:entry1.mjs
import './entry2.mjs';

// Scenario 1: cycle_b starts async evaluation (setting its ordinal) as part of
// an SCC whose cycle root cycle_a then fails synchronously in cycle_c.
let cycle_error;
try {
  await import('./cycle_a.mjs');
  assertUnreachable();
} catch (e) {
  cycle_error = e;
}
assertInstanceof(cycle_error, Error);
assertEquals('sync error', cycle_error.message);
gc();

// Scenario 2: sync_term_tla has top-level await (so it sets its ordinal) but
// terminates synchronously before reaching the await.
setTimeout(() => {
  import('./sync_term_tla.mjs');
}, 0);

// Absorbs the re-requested TerminateExecution interrupt from d8's dynamic
// import handler during entry2.mjs's EmptyMessageQueues below.
setTimeout(() => {}, 0);

let reimport_rejected = false;
setTimeout(() => {
  gc();
  import('./sync_term_tla.mjs').then(
      () => assertUnreachable(),
      (e) => {
        assertEquals(null, e);
        reimport_rejected = true;
      });
  Promise.resolve().then(() => {
    setTimeout(() => {
      assertTrue(reimport_rejected);
    }, 0);
  });
}, 0);

// JS_BUNDLE_MODULE_ENTRYPOINT:entry2.mjs
