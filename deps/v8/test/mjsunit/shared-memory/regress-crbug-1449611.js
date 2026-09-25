// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing --harmony-struct

function f(a) { a[4] = 42; }

for (let i = 0; i < 2; i++) {
  f([]);
  f(Atomics.Mutex());
  f(Atomics.Condition());
}
