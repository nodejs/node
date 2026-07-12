// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --fuzzing --jit-fuzzing
// Flags: --max-inlined-bytecode-size-small=0 --single-threaded

for (let v0 = 0; v0 < 5; v0++) {
  const v1 = %OptimizeOsr();
  const v4 = Math.sqrt(undefined);
  function f5() {
    return v4;
  }
  f5();
}
