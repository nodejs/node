// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test_func_1(n, b, o, s) {
  return test_func_2(n + 1, !b, o, s);
}

function test_func_2(n, b, o, s) {
  return (function(n, b, o, s) {
    return test_func_3(n, b, o, s);
  })(n + 1, !b, { inner: o }, s + "!");
}

function test_func_3(n, b, o, s) {
  throw new Error("v8dbg bridge test: " + s);
}

test_func_1(42, true, { key: "value" }, "hello");
