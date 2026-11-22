// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x05,                    // section length 5
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $grow (result i32)

  0x05,                    // section kind: Memory
  0x03,                    // section length 3
  0x01, 0x00,              // memory count 1:  no maximum
  0x01,                    // initial size 1

  0x07,                    // section kind: Export
  0x11,                    // section length 17
  0x02,                    // exports count 2
                           // export # 0
  0x04,                    // field name length:  4
  0x67, 0x72, 0x6f, 0x77,  // field name: grow
  0x00, 0x00,              // kind: function index:  0
                           // export # 1
  0x06,                    // field name length:  6
  0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79,  // field name: memory
  0x02, 0x00,              // kind: memory memory index

  0x0a,                    // section kind: Code
  0x08,                    // section length 8
  0x01,                    // functions count 1
                           // function #0 $grow
  0x06,                    // body size 6
  0x00,                    // 0 entries in locals list
  0x41, 0x01,              // i32.const 1
  0x40, 0x00,              // memory.grow
  0x0b,                    // end
]);

assertPromiseResult(
    WebAssembly.compile(bytes),
    module => {
      print('promise resolved: ' + module);
      new WebAssembly.Instance(module).exports.grow();
    });
