// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests that the receiver {length} property conversion works on 32-bit
// systems (i.e. it should not crash).

function ThrowingSort() {
  const __v_3 = new Array(2147549152);
  Object.defineProperty(__v_3, 0, {
    get: () => { throw new Error("Do not actually sort!"); }
  });
  __v_3.sort();
}

assertThrows(() => ThrowingSort());
