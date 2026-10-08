// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

function f4() {
  const sab = new SharedArrayBuffer(8);
  const array = new Int32Array(sab);
  Atomics.notify(array, 0);
  Atomics.waitAsync(array, 0);
}
for (let i = 0; i < 5; i++) {
  f4();
  gc();
}
