  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x0a,                    // section length 10
  0x02,                    // types count 2
  0x50, 0x00,              //  subtype extensible, supertype count 0
  0x5e, 0x7f, 0x01,        //  kind: array i32 mutable
  0x60,                    //  kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x01,              // functions count 1: 0 $func0 (result i32)

  0x0a,                    // section kind: Code
  0x13,                    // section length 19
  0x01,                    // functions count 1
                           // function #0 $func0
  0x11,                    // body size 17
  0x00,                    // 0 entries in locals list
  0x41, 0x01,              // i32.const 1
  0x41, 0x02,              // i32.const 2
  0xfb, 0x06, 0x00,        // array.new $type0
  0x41, 0x00,              // i32.const 0
  0x41, 0x2a,              // i32.const 42
  0xfe, 0x6f, 0x01, 0x00,  // array.atomic.rmw.xor acqrel $type0
  0x0b,                    // end
