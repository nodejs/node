// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let calledAlready = false;

function callNonRecursive(v) {
  if (calledAlready) return;
  calledAlready = true;
  v();
  calledAlready = false;
  return Number.NaN;
}

let v8 = new WebAssembly.Instance(new WebAssembly.Module(new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x06,                    // section length 6
  0x01, 0x60,              // types count 1:  kind: func
  0x01, 0x6f,              // param count 1:  externref
  0x01, 0x7d,              // return count 1:  f32

  0x02,                    // section kind: Import
  0x17,                    // section length 23
  0x01,                    // imports count 1: import #0
  0x07,                    // module name length:  7
  0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x73,  // module name: imports
  0x0b,                    // field name length:  11
  0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x5f, 0x30,
  0x5f, 0x76, 0x35,        // field name: import_0_v5
  0x00, 0x00,              // kind: function (param externref) (result f32)

  0x07,                    // section kind: Export
  0x07,                    // section length 7
  0x01,                    // exports count 1: export # 0
  0x03,                    // field name length:  3
  0x69, 0x77, 0x30,        // field name: iw0
  0x00, 0x00,              // kind: function index:  0
])),
{ imports: {
    import_0_v5: callNonRecursive,
} });

const v10 = v8.exports.iw0;
function f11() {
    v10(f11);
    try { [].reduceRight(f11); } catch (e) { }
}
%PrepareFunctionForOptimization(f11);
f11();
%OptimizeFunctionOnNextCall(f11);
f11();
