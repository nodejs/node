  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x04,                    // section length 4
  0x01, 0x60,              // types count 1:  kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x00,              // functions count 1: 0 $test

  0x05,                    // section kind: Memory
  0x07,                    // section length 7
  0x02,                    // memory count 2
  0x03,                    //  shared with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1
  0x03,                    //  shared with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1

  0x07,                    // section kind: Export
  0x22,                    // section length 34
  0x03,                    // exports count 3
                           // export # 0
  0x0a,                    // field name length:  10
  0x24, 0x6d, 0x79, 0x4d, 0x65, 0x6d, 0x6f, 0x72,
  0x79, 0x30,              // field name: $myMemory0
  0x02, 0x00,              // kind: memory memory index
                           // export # 1
  0x0a,                    // field name length:  10
  0x24, 0x6d, 0x79, 0x4d, 0x65, 0x6d, 0x6f, 0x72,
  0x79, 0x31,              // field name: $myMemory1
  0x02, 0x01,              // kind: memory memory index
                           // export # 2
  0x04,                    // field name length:  4
  0x74, 0x65, 0x73, 0x74,  // field name: test
  0x00, 0x00,              // kind: function index:  0

  0x0a,                    // section kind: Code
  0x5e,                    // section length 94
  0x01,                    // functions count 1
                           // function #0 $test
  0x5c,                    // body size 92
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0xfe, 0x10, 0x62, 0x00, 0x01, 0x01,  // i32.atomic.load acqrel offset=1
  0xfe, 0x33, 0x62, 0x01, 0x11, 0x02,  // i32.atomic.rmw.or $$myMemory1 acqrel offset=2
  0xfe, 0x17, 0x62, 0x01, 0x01, 0x03,  // i32.atomic.store $$myMemory1 acqrel offset=3
  0xfe, 0x10, 0x62, 0x00, 0x00, 0x01,  // i32.atomic.load offset=1
  0xfe, 0x33, 0x62, 0x01, 0x00, 0x02,  // i32.atomic.rmw.or $$myMemory1 offset=2
  0xfe, 0x17, 0x62, 0x01, 0x00, 0x03,  // i32.atomic.store $$myMemory1 offset=3
  0xfe, 0x10, 0x42, 0x00, 0x01,  // i32.atomic.load offset=1
  0xfe, 0x33, 0x42, 0x01, 0x02,  // i32.atomic.rmw.or $$myMemory1 offset=2
  0xfe, 0x17, 0x42, 0x01, 0x03,  // i32.atomic.store $$myMemory1 offset=3
  0xfe, 0x10, 0x22, 0x01, 0x01,  // i32.atomic.load acqrel offset=1
  0xfe, 0x33, 0x22, 0x11, 0x02,  // i32.atomic.rmw.or acqrel offset=2
  0xfe, 0x17, 0x22, 0x01, 0x03,  // i32.atomic.store acqrel offset=3
  0x0b,                    // end
