// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-bulk-memory

// Make sure DCHECK doesn't fire when passive data segment is at the end of the
// module.
let bytes = new Uint8Array([
  0,   97,  115, 109, 1,   0,   0,   0,   1,   132, 128, 128, 128, 0,   1,
  96,  0,   0,   3,   133, 128, 128, 128, 0,   4,   0,   0,   0,   0,   5,
  131, 128, 128, 128, 0,   1,   0,   1,   7,   187, 128, 128, 128, 0,   4,
  12,  100, 114, 111, 112, 95,  112, 97,  115, 115, 105, 118, 101, 0,   0,
  12,  105, 110, 105, 116, 95,  112, 97,  115, 115, 105, 118, 101, 0,   1,
  11,  100, 114, 111, 112, 95,  97,  99,  116, 105, 118, 101, 0,   2,   11,
  105, 110, 105, 116, 95,  97,  99,  116, 105, 118, 101, 0,   3,   12,  129,
  128, 128, 128, 0,   2,   10,  183, 128, 128, 128, 0,   4,   133, 128, 128,
  128, 0,   0,   252, 9,   0,   11,  140, 128, 128, 128, 0,   0,   65,  0,
  65,  0,   65,  0,   252, 8,   0,   0,   11,  133, 128, 128, 128, 0,   0,
  252, 9,   1,   11,  140, 128, 128, 128, 0,   0,   65,  0,   65,  0,   65,
  0,   252, 8,   1,   0,   11,  11,  136, 128, 128, 128, 0,   2,   1,   0,
  0,   65,  0,   11,  0
]);
new WebAssembly.Instance(new WebAssembly.Module(bytes));
