// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle --expose-gc --verify-heap

// JS_BUNDLE_MODULE:child.mjs
await new Promise(r => setTimeout(r, 0));

// JS_BUNDLE_MODULE:m1.mjs
import './child.mjs';
export var x = 1;
d8.terminate();
(() => {})();
await 0;

// JS_BUNDLE_MODULE:other.mjs
import './m1.mjs';
export var y = 2;

// JS_BUNDLE_MODULE:setup.mjs
import defer * as ns_m1 from './m1.mjs';

// Like regress-564880275-7.js, but m1.mjs has a top-level await, so it is
// started via ExecuteAsyncModule (not ExecuteModule) from the exec list of
// AsyncModuleExecutionFulfilled once child.mjs's timer resolves, and
// terminates synchronously before reaching its first await. Both m1.mjs and
// the entrypoint must be transitioned to kErrored (with a null exception and
// finished async ordinal).
let other_rejected = false;
setTimeout(() => {
  gc();
  assertThrowsEquals(() => ns_m1.x, null);
  import('./other.mjs').then(
      () => assertUnreachable(),
      (e) => {
        assertEquals(null, e);
        other_rejected = true;
      });
  Promise.resolve().then(() => {
    setTimeout(() => {
      assertTrue(other_rejected);
    }, 0);
  });
}, 0);

// JS_BUNDLE_MODULE_ENTRYPOINT
import './m1.mjs';
import './setup.mjs';
