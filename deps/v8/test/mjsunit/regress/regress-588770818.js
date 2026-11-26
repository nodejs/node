// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --print-all-exceptions --stack-size=100

// Top-level recursive function whose prologue overflows before allocating its
// FunctionContext, while the frame still holds NativeContext.
function g(n) {
  let a = 1, b = 2, c = 3, d = 4;
  if (n > 0) g(n - 1);
  return () => a + b + c + d;
}

for (let i = 0; i < 100; i++) {
  g(0);
}
try {
  g(100000);
} catch (e) {}

// Also exercise WithContext unwinding in a function that triggers an exception.
function withContextThrow() {
  let x = 1, y = 2;
  with ({}) {
    try {
      throw new Error('test');
    } catch (e) {}
  }
  return () => x + y;
}
withContextThrow();
