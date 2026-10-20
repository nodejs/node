// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-concurrent-recompilation
// Flags: --invocation-count-for-turbofan=1000000000
// Flags: --invocation-count-for-feedback-allocation=1
// Flags: --invocation-count-for-maglev=1

function f(x) {
  return x + 1;
}
for (let i = 0; i < 20; i++) {
  assertEquals(i + 1, f(i));
}

function g(o) {
  return o.p;
}
for (let i = 0; i < 10; i++) {
  assertEquals(i, g({p: i, ["a" + i]: i}));
}
