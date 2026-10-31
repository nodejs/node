// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// JS_BUNDLE_MODULE:c.mjs
await 0;
throw new Error('boom');

// JS_BUNDLE_MODULE:b.mjs
import './a.mjs';
export let x = 1;
await new Promise(() => {});

// JS_BUNDLE_MODULE:a.mjs
import './b.mjs';
import './c.mjs';

// JS_BUNDLE_MODULE:parent.mjs
import './a.mjs';

// JS_BUNDLE_MODULE:defer_b.mjs
import defer * as ns from './b.mjs';
export {ns};

// JS_BUNDLE_MODULE_ENTRYPOINT
let first_error;
try {
  await import('./parent.mjs');
  assertUnreachable();
} catch (e) {
  first_error = e;
}
assertInstanceof(first_error, Error);
assertEquals('boom', first_error.message);

const {ns} = await import('./defer_b.mjs');

setTimeout(() => {
  d8.terminate();
  try {
    ns.x;
  } catch (e) {
  }
  (() => {})();
}, 0);

setTimeout(() => {
  let sync_error;
  try {
    ns.x;
    assertUnreachable();
  } catch (e) {
    sync_error = e;
  }
  assertInstanceof(sync_error, Error);
  assertEquals('boom', sync_error.message);

  let async_error;
  import('./a.mjs').then(
      () => assertUnreachable(),
      (e) => {
        async_error = e;
      });
  Promise.resolve().then(() => {
    setTimeout(() => {
      assertInstanceof(async_error, Error);
      assertEquals('boom', async_error.message);
    }, 0);
  });
}, 0);
