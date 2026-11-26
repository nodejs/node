// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// JS_BUNDLE_MODULE:a.mjs
import defer * as D from './d.mjs';
export function readX() { return D.x; }

// a.mjs is evaluated, but its cycle root (root.mjs) is still evaluating
// asynchronously, so d.mjs is not ready for synchronous execution yet.
setTimeout(() => {
  assertThrows(() => D.x, TypeError);
}, 0);

// JS_BUNDLE_MODULE:d.mjs
import defer * as S from './s.mjs';
import './a.mjs';
export const x = 42;

// JS_BUNDLE_MODULE:s.mjs
import defer * as M from './m.mjs';

// JS_BUNDLE_MODULE:m.mjs
import './root.mjs';
await 0;

// JS_BUNDLE_MODULE_ENTRYPOINT:root.mjs
import { readX } from './a.mjs';
import './s.mjs';
await new Promise(r => setTimeout(r, 0));

// Once root.mjs has finished evaluating, d.mjs can be evaluated synchronously.
setTimeout(() => {
  assertEquals(42, readX());
}, 0);
