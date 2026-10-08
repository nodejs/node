// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-concurrent-recompilation --maglev-untagged-phis

function foo() {
  let x = 0;
  var y = 0x4e000000;
  for (let i = 0; i < 250; i++) {
    if (i == 100) {
      x = y;
    }
    delete ArrayBuffer[x];
    x | 0;
  }

  return x;
}

assertEquals(1308622848, foo());
