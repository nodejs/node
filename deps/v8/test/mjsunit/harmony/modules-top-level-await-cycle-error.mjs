// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:--harmony-top-level-await --ignore-unhandled-promises

try {
  await import('./modules-skip-top-level-await-cycle-error.mjs');
  assertUnreachable();
} catch(e) {
  assertEquals(e.message, 'Error in modules-skip-top-level-await-cycle-error-throwing.mjs');
}
