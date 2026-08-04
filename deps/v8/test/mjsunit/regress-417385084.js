// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing

function wrap(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  let u1 = wrap();
  for (let i = 0; i < 25; i++) {
    let u2 = wrap();
    const sum = wrap(() => x + 2147483647);
    try {
      x = sum | foo;
    } catch (e) {}
    try {
      u2++;
    } catch (e) {}
    u1 = (u1 & sum && u2) | 4294967295;
  }
}

foo();
foo();
