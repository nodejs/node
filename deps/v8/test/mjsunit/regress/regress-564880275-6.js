// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --expose-gc --verify-heap

// JS_BUNDLE_MODULE:tla.mjs
export let x = 42;
await new Promise(r => {
  globalThis.resume_tla = r;
});

// JS_BUNDLE_MODULE:parent.mjs
import './tla.mjs';
export let ok = true;

// JS_BUNDLE_MODULE_ENTRYPOINT
let parent_resolved = false;
import('./parent.mjs').then((ns) => {
  assertTrue(ns.ok);
  parent_resolved = true;
});
await 0;

setTimeout(() => {
  // tla.mjs is kEvaluatingAsync (via parent.mjs) with no top_level_capability
  // yet. Evaluating it directly while a termination interrupt is pending must
  // not transition it to kErrored.
  Promise.resolve().then(() => d8.terminate());
  import('./tla.mjs');
}, 0);

// Absorbs the d8.terminate() interrupt inside dynamic import's nested
// EmptyMessageQueues.
setTimeout(() => {}, 0);

let tla_resolved = false;
setTimeout(() => {
  gc();
  globalThis.resume_tla();
  import('./tla.mjs').then((ns) => {
    assertEquals(42, ns.x);
    tla_resolved = true;
  });
  Promise.resolve().then(() => {
    setTimeout(() => {
      assertTrue(parent_resolved);
      assertTrue(tla_resolved);
    }, 0);
  });
}, 0);
