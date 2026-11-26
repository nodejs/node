// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// JS_BUNDLE_MODULE:Thrower.mjs
throw new Error('boom');

// JS_BUNDLE_MODULE:C.mjs
import './A.mjs';
await Promise.resolve();

// W.mjs stays linked while B.mjs is evaluated (it is only deferred-imported),
// and later becomes evaluated-error when imported directly.
// JS_BUNDLE_MODULE:W.mjs
import './Thrower.mjs';
import './C.mjs';

// JS_BUNDLE_MODULE:B.mjs
import defer * as nsW from './W.mjs';

// Cycle root of {A, B, C}.
// JS_BUNDLE_MODULE:A.mjs
import './B.mjs';
await new Promise(r => globalThis.resolveA = r);

// JS_BUNDLE_MODULE:D.mjs
import './B.mjs';
globalThis.dEvaluated = true;

// JS_BUNDLE_MODULE:Middle.mjs
import defer * as nsD from './D.mjs';
globalThis.middleEvaluated = true;
nsD.z;

// JS_BUNDLE_MODULE_ENTRYPOINT:entry.mjs
globalThis.dEvaluated = false;
globalThis.middleEvaluated = false;

// A -> B --(defer)--> W -> C -> A: A, B and C form an asynchronous SCC rooted
// at A, while W stays linked.
import('./A.mjs');

// C's top-level await settles, so B becomes evaluated while its cycle root A
// is still evaluating asynchronously.
await Promise.resolve();
await Promise.resolve();

// W fails synchronously in Thrower.mjs before reaching C.
let wError;
try {
  await import('./W.mjs');
} catch (e) {
  wError = e;
}
assertEquals('boom', wError.message);

// D's asynchronous transitive dependencies include B, which is evaluated but
// whose cycle root A has not settled. Middle.mjs must wait for A instead of
// evaluating D synchronously.
let middleSettled = false;
const middle = import('./Middle.mjs');
middle.then(() => middleSettled = true, () => assertUnreachable());

setTimeout(() => {
  assertFalse(middleSettled);
  assertFalse(globalThis.middleEvaluated);
  assertFalse(globalThis.dEvaluated);
  globalThis.resolveA();
  middle.then(() => {
    assertTrue(globalThis.middleEvaluated);
    assertTrue(globalThis.dEvaluated);
  });
}, 0);
