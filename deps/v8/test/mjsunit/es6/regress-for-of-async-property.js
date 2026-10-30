// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Only the bare identifier `async` is disallowed as the left-hand side of a
// for-of loop (`for (async of ...)`). Property accesses like `x.async` must
// be allowed, and non-identifier expressions ending in `async` must report
// standard LHS errors rather than "The left-hand side of a for-of loop may
// not be 'async'".

let obj = {};
for (obj.async of [42]) {}
assertEquals(42, obj.async);

{
  let async = 0;
  for ((async) of [42]) {}
  assertEquals(42, async);
}

assertThrows(
    () => eval("for (async of [1]) {}"),
    SyntaxError,
    "The left-hand side of a for-of loop may not be 'async'.");
assertThrows(
    () => eval("for (obj?.async of [1]) {}"),
    SyntaxError,
    "Invalid left-hand side in for-loop");
assertThrows(
    () => eval("for (+async of [1]) {}"),
    SyntaxError,
    "Invalid left-hand side in for-loop");
assertThrows(
    () => eval("for (a, async of [1]) {}"),
    SyntaxError,
    "Invalid left-hand side in for-loop");
