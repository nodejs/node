// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  let smi = 0;
  // x cannot be represented as a Smi, but it can be represented as an Int32,
  // so it is a HeapNumber object.
  var x = 0x4e000000;
  for (let i = 0; i < 200; ++i) {
    if (i == 100) {
      smi = x;
    }
    smi + 2;
  }
  return smi;
}

assertEquals(0x4e000000, foo());
