// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-1.mjs';

assertEquals(0, globalThis.eval_list.length);

const probe = { a: 1, foo: 2 };

for (const k in probe) {
  delete probe.a;
  delete probe.foo;
  Object.setPrototypeOf(probe, ns);
}

// HasEnumerableProperty triggers evaluation of the deferred module
// because it calls [[GetOwnProperty]] while iterating on prototype.
assertArrayEquals(['defer-1'], globalThis.eval_list);
