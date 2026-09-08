// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --array-destructure-bytecode

(function TestHoleCheckElisionInDefault() {
  function f1() {
    let [x = z, y = z, z] = [, , 0];
    return x + y + z;
  }

  assertThrows(() => f1(), ReferenceError);

  function f2() {
    let [x = z, y = z, z] = [1, , 0];
    return x + y + z;
  }

  assertThrows(() => f2(), ReferenceError);
})();
