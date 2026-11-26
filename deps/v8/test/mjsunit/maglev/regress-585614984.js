// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(flag, a, val, skip) {
  let r = "abcdefghijklmn" + (0 | flag);
  if (flag) {
    (0 | flag) + (a + r).length;
  }
  try {
    with (val) {
      return 2;
    }
  } catch {}
  if (skip) return 3;
  intentionallyMissing;
  return (null?.foo().bar)();
}

%PrepareFunctionForOptimization(foo);
foo(true, "x", null, true);
foo(false, {}, 1, true);
%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo(false, 0x18800000, null, false), ReferenceError);
