// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-throws-1.mjs';

assertEquals(0, globalThis.eval_list.length);

const probe = { a: 1, foo: 2 };

let err;
try {
  for (const k in probe) {
    delete probe.a;
    delete probe.foo;
    Object.setPrototypeOf(probe, ns);
  }
} catch(e) {
  err = e;
}

// HasEnumerableProperty triggers evaluation of the deferred module
// because it performs a [[GetOwnProperty]] while iterating on prototype.
// Since the module throws during evaluation, the for-in loop throws.
assertArrayEquals(['defer-throws-1'], globalThis.eval_list);
assertEquals(err.message, "deferred module eval exception");
