// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-throws-1.mjs';

assertEquals(0, globalThis.eval_list.length);

for (let i = 0; i < 100; i++) {
  let err1;
  try {
    ns.foo;
  } catch(e) {
    err1 = e;
  }

  assertArrayEquals(['defer-throws-1'], globalThis.eval_list);
  assertEquals(err1.message, "deferred module eval exception");

  let err2;
  try {
    ns.foo;
  } catch(e) {
    err2 = e;
  }

  assertArrayEquals(['defer-throws-1'], globalThis.eval_list);
  assertEquals(err1, err2);
}
