// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-mem-pages=65536

try {
  var __v_50189 = new WebAssembly.Memory({
    initial: 65536
  });
} catch (e) {
  // 32-bit builds will throw a RangeError, that's okay.
  assertTrue(e instanceof RangeError);
}
