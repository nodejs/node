// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x12,                    // section length 18
  0x03,                    // types count 3
  0x60,                    //  kind: func
  0x01, 0x7e,              // param count 1:  i64
  0x01, 0x7e,              // return count 1:  i64
  0x60,                    //  kind: func
  0x02,                    // param count 2
  0x7e, 0x7e,              // i64 i64
  0x01, 0x7e,              // return count 1:  i64
  0x60,                    //  kind: func
  0x03,                    // param count 3
  0x7e, 0x7e, 0x7f,        // i64 i64 i32
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x04,                    // section length 4
  0x03,                    // functions count 3
  0x00,                    // 0 $corrupt_load (param i64) (result i64)
  0x01,                    // 1 $oob_read (param i64 i64) (result i64)
  0x02,                    // 2 $oob_write8 (param i64 i64 i32)

  0x05,                    // section kind: Memory
  0x05,                    // section length 5
  0x02,                    // memory count 2
  0x04,                    //  mem64 no maximum
  0x02,                    // initial size 2
  0x04,                    //  mem64 no maximum
  0x01,                    // initial size 1

  0x07,                    // section kind: Export
  0x32,                    // section length 50
  0x05,                    // exports count 5
                           // export # 0
  0x02,                    // field name length:  2
  0x6d, 0x30,              // field name: m0
  0x02, 0x00,              // kind: memory memory index
                           // export # 1
  0x02,                    // field name length:  2
  0x6d, 0x31,              // field name: m1
  0x02, 0x01,              // kind: memory memory index
                           // export # 2
  0x0c,                    // field name length:  12
  0x63, 0x6f, 0x72, 0x72, 0x75, 0x70, 0x74, 0x5f,
  0x6c, 0x6f, 0x61, 0x64,  // field name: corrupt_load
  0x00, 0x00,              // kind: function index:  0
                           // export # 3
  0x08,                    // field name length:  8
  0x6f, 0x6f, 0x62, 0x5f, 0x72, 0x65, 0x61, 0x64,  // field name: oob_read
  0x00, 0x01,              // kind: function index:  1
                           // export # 4
  0x0a,                    // field name length:  10
  0x6f, 0x6f, 0x62, 0x5f, 0x77, 0x72, 0x69, 0x74,
  0x65, 0x38,              // field name: oob_write8
  0x00, 0x02,              // kind: function index:  2

  0x0a,                    // section kind: Code
  0x37,                    // section length 55
  0x03,                    // functions count 3
                           // function #0 $corrupt_load
  0x11,                    // body size 17
  0x00,                    // 0 entries in locals list
  0x42, 0x00,              // i64.const 0
  0x41, 0xc1, 0x00,        // i32.const 65
  0x20, 0x00,              // local.get $var0
  0xfc, 0x0b, 0x00,        // memory.fill
  0x42, 0x00,              // i64.const 0
  0x29, 0x03, 0x00,        // i64.load
  0x0b,                    // end
                           // function #1 $oob_read
  0x10,                    // body size 16
  0x00,                    // 0 entries in locals list
  0x42, 0x00,              // i64.const 0
  0x41, 0x00,              // i32.const 0
  0x20, 0x00,              // local.get $var0
  0xfc, 0x0b, 0x00,        // memory.fill
  0x20, 0x01,              // local.get $var1
  0x29, 0x03, 0x00,        // i64.load
  0x0b,                    // end
                           // function #2 $oob_write8
  0x12,                    // body size 18
  0x00,                    // 0 entries in locals list
  0x42, 0x00,              // i64.const 0
  0x41, 0x00,              // i32.const 0
  0x20, 0x00,              // local.get $var0
  0xfc, 0x0b, 0x00,        // memory.fill
  0x20, 0x01,              // local.get $var1
  0x20, 0x02,              // local.get $var2
  0x3a, 0x00, 0x00,        // i32.store8
  0x0b,                    // end

  0x00,                    // section kind: Unknown
  0x2c,                    // section length 44
  0x04,                    // section name length:  4
  0x6e, 0x61, 0x6d, 0x65,  // section name: name
  0x01,                    // name type: function
  0x25,                    // payload length: 37
  0x03,                    // names count 3
  0x00, 0x0c,              // index 0 name length:  12
  0x63, 0x6f, 0x72, 0x72, 0x75, 0x70, 0x74, 0x5f,
  0x6c, 0x6f, 0x61, 0x64,  // name: corrupt_load
  0x01, 0x08,              // index 1 name length:  8
  0x6f, 0x6f, 0x62, 0x5f, 0x72, 0x65, 0x61, 0x64,  // name: oob_read
  0x02, 0x0a,              // index 2 name length:  10
  0x6f, 0x6f, 0x62, 0x5f, 0x77, 0x72, 0x69, 0x74,
  0x65, 0x38,              // name: oob_write8
]);

async function run() {
  const { instance } = await WebAssembly.instantiate(bytes);
  const ex = instance.exports;
  const m1 = new BigUint64Array(ex.m1.buffer);
  const SENTINEL = 0x1122334455667788n;
  m1[0] = SENTINEL;

  ex.corrupt_load(8n);
  const r = ex.corrupt_load(256n);

  assertEquals(r, 0x4141414141414141n);
}
run();
