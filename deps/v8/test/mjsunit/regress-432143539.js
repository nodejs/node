// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

// Test fails when run with `d8 --isolate` which is used by
// `run-tests.py --isolates`.

async function f0() {
  await gc({ execution: "async" });
  d8.terminate();
}
f0();
f0();
f0();
