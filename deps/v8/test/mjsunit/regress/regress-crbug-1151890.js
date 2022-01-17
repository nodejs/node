// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

for (let i = 0, j = 0; i < 10; ++i) {
  let x = (-0xffffffffffffffff_ffffffffffffffffn >> 0x40n);
  assertEquals(-0x10000000000000000n, x);
  %SimulateNewspaceFull();
}
