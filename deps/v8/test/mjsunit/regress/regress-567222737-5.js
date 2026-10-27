// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A call to `async(...)` with spread arguments inside an async arrow function's
// parameter initializer should not record rest-parameter errors on the outer
// async arrow head.
{
  let async = (...args) => args;

  let f1 = async (a = async(...[1, 2], 3)) => a;
  f1().then(v => assertEquals([1, 2, 3], v));

  let b;
  let f2 = async (a = async(...b = [4, 5])) => a;
  f2().then(v => {
    assertEquals([4, 5], v);
    assertEquals([4, 5], b);
  });

  // Top-level/eagerly parsed versions:
  let f3 = (async (a = async(...[6, 7], 8)) => a);
  f3().then(v => assertEquals([6, 7, 8], v));
}

// Actual invalid rest parameters in async arrow heads must still be rejected.
assertThrows("async (...a, b) => {}", SyntaxError);
assertThrows("async (...a = [1]) => {}", SyntaxError);
assertThrows("async (x = async (...a, b) => {}) => {}", SyntaxError);
assertThrows("async (x = async (...a = [1]) => {}) => {}", SyntaxError);
