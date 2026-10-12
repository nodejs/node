  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x09,                    // section length 9
  0x02,                    // types count 2
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x7d,              // return count 1:  f32
  0x60,                    //  kind: func
  0x01, 0x7d,              // param count 1:  f32
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x03,                    // section length 3
  0x02,                    // functions count 2
  0x00,                    // 0 $func0 (result f32)
  0x01,                    // 1 $func1 (param f32)

  0x05,                    // section kind: Memory
  0x04,                    // section length 4
  0x01, 0x01,              // memory count 1:  with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1

  0x0a,                    // section kind: Code
  0x33,                    // section length 51
  0x02,                    // functions count 2
                           // function #0 $func0
  0x16,                    // body size 22
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0xfc, 0x30, 0x01, 0x00,  // f32.load_f16
  0x41, 0x00,              // i32.const 0
  0xfc, 0x30, 0x01, 0x04,  // f32.load_f16 offset=4
  0x92,                    // f32.add
  0x41, 0x00,              // i32.const 0
  0xfc, 0x30, 0x00, 0x00,  // f32.load_f16 align=1
  0x92,                    // f32.add
  0x0b,                    // end
                           // function #1 $func1
  0x1a,                    // body size 26
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x20, 0x00,              // local.get $var0
  0xfc, 0x31, 0x01, 0x00,  // f32.store_f16
  0x41, 0x00,              // i32.const 0
  0x20, 0x00,              // local.get $var0
  0xfc, 0x31, 0x01, 0x04,  // f32.store_f16 offset=4
  0x41, 0x00,              // i32.const 0
  0x20, 0x00,              // local.get $var0
  0xfc, 0x31, 0x00, 0x00,  // f32.store_f16 align=1
  0x0b,                    // end
