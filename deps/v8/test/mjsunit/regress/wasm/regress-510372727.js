// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --single-threaded --experimental-wasm-pgo-to-file

let bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $func0

  0x0a,                    // section kind: Code
  0x04,                    // section length 4
  0x01,                    // functions count 1
                           // function #0 $func0
  0x02,                    // body size 2
  0x00,                    // 0 entries in locals list
  0x0b,                    // end
]);
let module = new WebAssembly.Module(bytes);
let instance = new WebAssembly.Instance(module);
