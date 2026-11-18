// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// JS_BUNDLE_MODULE:m_term.mjs
export var x = 1;
d8.terminate();
(() => {})();

// JS_BUNDLE_MODULE:outer.mjs
import defer * as mt from './m_term.mjs';
export var y = mt.x;

// JS_BUNDLE_MODULE_ENTRYPOINT
import defer * as mt from './m_term.mjs';
import defer * as outer from './outer.mjs';

// Terminates inside nested synchronous evaluation: outer.mjs -> m_term.mjs.
// Both modules transition to kErrored with a null exception and no capability.
setTimeout(() => {
  outer.y;
  assertUnreachable();
}, 0);

let outer_import_rejected = false;
setTimeout(() => {
  assertThrowsEquals(() => mt.x, null);
  import('./outer.mjs').then(
      () => assertUnreachable(),
      (e) => {
        assertEquals(null, e);
        assertThrowsEquals(() => outer.y, null);
        outer_import_rejected = true;
      });
  Promise.resolve().then(() => {
    setTimeout(() => {
      assertTrue(outer_import_rejected);
    }, 0);
  });
}, 0);
