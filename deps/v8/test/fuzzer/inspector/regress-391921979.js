// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --no-wasm-generic-wrapper --log-code

let bytes =
[
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x02,                    // section kind: Import
  0x07,                    // section length 7
  0x01,                    // imports count 1: import #0
  0x01,                    // module name length:  1
  0x6d,                    // module name: m
  0x01,                    // field name length:  1
  0x66,                    // field name: f
  0x00, 0x00,              // kind: function

];

let module = new WebAssembly.Module(new Uint8Array(bytes));
let instance = new WebAssembly.Instance(module, {m: {f: () => {}}});
