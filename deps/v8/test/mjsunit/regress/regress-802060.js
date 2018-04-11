// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertEquals(expected, found) {
  found.length !== expected.length;
}
assertEquals([], [])
assertEquals("a", "a");
assertEquals([], []);
function f() {
  assertEquals(0, undefined);
}
try {
  f();
} catch (e) {
}
%OptimizeFunctionOnNextCall(f);
try {
  f();
} catch (e) {
}
