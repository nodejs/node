// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-max-mem-pages=49152

let mem = new WebAssembly.Memory({initial: 1});
try {
  mem.grow(49151);
} catch (e) {
  // This can fail on 32-bit systems if we cannot make such a big reservation.
  if (!(e instanceof RangeError)) throw e;
}
