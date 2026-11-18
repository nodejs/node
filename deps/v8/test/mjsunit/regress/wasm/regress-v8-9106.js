// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure DCHECK doesn't fire when passive data segment is at the end of the
// module.
let bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x84, 0x80, 0x80, 0x80, 0x00,  // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x85, 0x80, 0x80, 0x80, 0x00,  // section length 5
  0x04,                    // functions count 4
  0x00,                    // 0 $drop_passive
  0x00,                    // 1 $init_passive
  0x00,                    // 2 $drop_active
  0x00,                    // 3 $init_active

  0x05,                    // section kind: Memory
  0x83, 0x80, 0x80, 0x80, 0x00,  // section length 3
  0x01, 0x00,              // memory count 1:  no maximum
  0x01,                    // initial size 1

  0x07,                    // section kind: Export
  0xbb, 0x80, 0x80, 0x80, 0x00,  // section length 59
  0x04,                    // exports count 4
                           // export # 0
  0x0c,                    // field name length:  12
  0x64, 0x72, 0x6f, 0x70, 0x5f, 0x70, 0x61, 0x73,
  0x73, 0x69, 0x76, 0x65,  // field name: drop_passive
  0x00, 0x00,              // kind: function index:  0
                           // export # 1
  0x0c,                    // field name length:  12
  0x69, 0x6e, 0x69, 0x74, 0x5f, 0x70, 0x61, 0x73,
  0x73, 0x69, 0x76, 0x65,  // field name: init_passive
  0x00, 0x01,              // kind: function index:  1
                           // export # 2
  0x0b,                    // field name length:  11
  0x64, 0x72, 0x6f, 0x70, 0x5f, 0x61, 0x63, 0x74,
  0x69, 0x76, 0x65,        // field name: drop_active
  0x00, 0x02,              // kind: function index:  2
                           // export # 3
  0x0b,                    // field name length:  11
  0x69, 0x6e, 0x69, 0x74, 0x5f, 0x61, 0x63, 0x74,
  0x69, 0x76, 0x65,        // field name: init_active
  0x00, 0x03,              // kind: function index:  3

  0x0c,                    // section kind: DataCount
  0x81, 0x80, 0x80, 0x80, 0x00,  // section length 1
  0x02,                    // data segments count 2

  0x0a,                    // section kind: Code
  0xb7, 0x80, 0x80, 0x80, 0x00,  // section length 55
  0x04,                    // functions count 4
                           // function #0 $drop_passive
  0x85, 0x80, 0x80, 0x80, 0x00,  // body size 5
  0x00,                    // 0 entries in locals list
  0xfc, 0x09, 0x00,        // data.drop 0
  0x0b,                    // end
                           // function #1 $init_passive
  0x8c, 0x80, 0x80, 0x80, 0x00,  // body size 12
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0xfc, 0x08, 0x00, 0x00,  // memory.init 0
  0x0b,                    // end
                           // function #2 $drop_active
  0x85, 0x80, 0x80, 0x80, 0x00,  // body size 5
  0x00,                    // 0 entries in locals list
  0xfc, 0x09, 0x01,        // data.drop 1
  0x0b,                    // end
                           // function #3 $init_active
  0x8c, 0x80, 0x80, 0x80, 0x00,  // body size 12
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0xfc, 0x08, 0x01, 0x00,  // memory.init 1
  0x0b,                    // end

  0x0b,                    // section kind: Data
  0x88, 0x80, 0x80, 0x80, 0x00,  // section length 8
  0x02,                    // data segments count 2
  0x01,                    // flag: passive
  0x00,                    // source size 0
                           // segment data
  0x00,                    // flag: active no index
  0x41, 0x00, 0x0b,        // i32.const 0
  0x00,                    // source size 0
                           // segment data
]);
new WebAssembly.Instance(new WebAssembly.Module(bytes));
