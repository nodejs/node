  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x1d,                    // section length 29
  0x07,                    // types count 7
  0x60,                    // type #0 $type0 kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0
  0x60,                    // type #1 $type1 kind: func
  0x01, 0x7f,              // param count 1:  i32
  0x00,                    // return count 0
  0x60,                    // type #2 $type2 kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #3 $type3 kind: func
  0x00,                    // param count 0
  0x01, 0x7e,              // return count 1:  i64
  0x60,                    // type #4 $type4 kind: func
  0x00,                    // param count 0
  0x01, 0x7d,              // return count 1:  f32
  0x60,                    // type #5 $type5 kind: func
  0x00,                    // param count 0
  0x01, 0x7c,              // return count 1:  f64
  0x60,                    // type #6 $type6 kind: func
  0x01, 0x7e,              // param count 1:  i64
  0x01, 0x7c,              // return count 1:  f64

  0x02,                    // section kind: Import
  0x30,                    // section length 48
  0x02,                    // imports count 2
                           // import #0
  0x03,                    // module name length: 3
  0x65, 0x6e, 0x76,        // module name: env
  0x0f,                    // field name length: 15
  0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x65, 0x64, 
  0x5f, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 
                           // field name: imported_global
  0x03, 0x7f, 0x00,        // kind: global i32 immutable
                           // import #1
  0x03,                    // module name length: 3
  0x65, 0x6e, 0x76,        // module name: env
  0x11,                    // field name length: 17
  0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x65, 0x64, 
  0x5f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 
  0x6e,                    // field name: imported_function
  0x00, 0x00,              // kind: function

  0x03,                    // section kind: Function
  0x09,                    // section length 9
  0x08,                    // functions count 8
  0x01,                    // 1 $func1 (param i32)
  0x02,                    // 2 $func2 (result i32)
  0x03,                    // 3 $func3 (result i64)
  0x04,                    // 4 $func4 (result f32)
  0x05,                    // 5 $func5 (result f64)
  0x00,                    // 6 $func6
  0x00,                    // 7 $func7
  0x00,                    // 8 $exported_function

  0x04,                    // section kind: Table
  0x04,                    // section length 4
  0x01, 0x70, 0x00,        // table count 1:  funcref no maximum
  0x04,                    // initial size 4

  0x05,                    // section kind: Memory
  0x04,                    // section length 4
  0x01, 0x01,              // memory count 1:  with maximum
  0x00,                    // initial size 0
  0x01,                    // maximum size 1

  0x06,                    // section kind: Global
  0x10,                    // section length 16
  0x03,                    // globals count 3
  0x7f, 0x01,              // global #1: i32 mutable
  0x41, 0x00, 0x0b,        // i32.const 0
  0x7f, 0x00,              // global #2: i32 immutable
  0x23, 0x00, 0x0b,        // global.get $env.imported_global
  0x7e, 0x00,              // global #3: i64 immutable
  0x42, 0x00, 0x0b,        // i64.const 0

  0x07,                    // section kind: Export
  0x27,                    // section length 39
  0x02,                    // exports count 2
                           // export # 0
  0x0f,                    // field name length: 15
  0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x65, 0x64, 
  0x5f, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 
                           // field name: exported_global
  0x03, 0x03,              // kind: global index: 3
                           // export # 1
  0x11,                    // field name length: 17
  0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x65, 0x64, 
  0x5f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 
  0x6e,                    // field name: exported_function
  0x00, 0x08,              // kind: function index: 8

  0x09,                    // section kind: Element
  0x0a,                    // section length 10
  0x01, 0x00,              // segment count 1: flag: active, offset:
  0x41, 0x00, 0x0b,        // i32.const 0
  0x04,                    // number of elements 4
  0x00,                    // index: 0
  0x01,                    // index: 1
  0x01,                    // index: 1
  0x03,                    // index: 3

  0x0a,                    // section kind: Code
  0xdc, 0x07,              // section length 988
  0x08,                    // functions count 8
                           // function #1 $func1
  0x14,                    // body size 20
  0x01,                    // 1 entries in locals list
  0x01, 0x7e,              // 1 local of type i64
  0x20, 0x00,              // local.get $var0
  0x22, 0x00,              // local.tee $var0
  0x21, 0x00,              // local.set $var0
  0x20, 0x01,              // local.get $var1
  0x22, 0x01,              // local.tee $var1
  0x21, 0x01,              // local.set $var1
  0x23, 0x01,              // global.get $global1
  0x24, 0x01,              // global.set $global1
  0x0b,                    // end
                           // function #2 $func2
  0x8c, 0x01,              // body size 140
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x45,                    // i32.eqz
  0x41, 0x01,              // i32.const 1
  0x46,                    // i32.eq
  0x41, 0x7f,              // i32.const -1
  0x47,                    // i32.ne
  0x41, 0xff, 0xff, 0xff, 0xff, 0x07,  // i32.const 2147483647
  0x48,                    // i32.lt_s
  0x41, 0x80, 0x80, 0x80, 0x80, 0x78,  // i32.const -2147483648
  0x49,                    // i32.lt_u
  0x41, 0x00,              // i32.const 0
  0x4a,                    // i32.gt_s
  0x41, 0x00,              // i32.const 0
  0x4b,                    // i32.gt_u
  0x41, 0x00,              // i32.const 0
  0x4c,                    // i32.le_s
  0x41, 0x00,              // i32.const 0
  0x4d,                    // i32.le_u
  0x41, 0x00,              // i32.const 0
  0x4e,                    // i32.ge_s
  0x41, 0x00,              // i32.const 0
  0x4f,                    // i32.ge_u
  0x67,                    // i32.clz
  0x68,                    // i32.ctz
  0x69,                    // i32.popcnt
  0x41, 0x00,              // i32.const 0
  0x6a,                    // i32.add
  0x41, 0x00,              // i32.const 0
  0x6b,                    // i32.sub
  0x41, 0x00,              // i32.const 0
  0x6c,                    // i32.mul
  0x41, 0x00,              // i32.const 0
  0x6d,                    // i32.div_s
  0x41, 0x00,              // i32.const 0
  0x6e,                    // i32.div_u
  0x41, 0x00,              // i32.const 0
  0x6f,                    // i32.rem_s
  0x41, 0x00,              // i32.const 0
  0x70,                    // i32.rem_u
  0x41, 0x00,              // i32.const 0
  0x71,                    // i32.and
  0x41, 0x00,              // i32.const 0
  0x72,                    // i32.or
  0x41, 0x00,              // i32.const 0
  0x73,                    // i32.xor
  0x41, 0x00,              // i32.const 0
  0x74,                    // i32.shl
  0x41, 0x00,              // i32.const 0
  0x75,                    // i32.shr_s
  0x41, 0x00,              // i32.const 0
  0x76,                    // i32.shr_u
  0x41, 0x00,              // i32.const 0
  0x77,                    // i32.rotl
  0x41, 0x00,              // i32.const 0
  0x78,                    // i32.rotr
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xa7,                    // i32.wrap_i64
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xa8,                    // i32.trunc_f32_s
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xa9,                    // i32.trunc_f32_u
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xaa,                    // i32.trunc_f64_s
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xab,                    // i32.trunc_f64_u
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xbc,                    // i32.reinterpret_f32
  0xc0,                    // i32.extend8_s
  0xc1,                    // i32.extend16_s
  0x0b,                    // end
                           // function #3 $func3
  0xc0, 0x01,              // body size 192
  0x00,                    // 0 entries in locals list
  0x42, 0x00,              // i64.const 0
  0x50,                    // i64.eqz
  0x1a,                    // drop
  0x42, 0x01,              // i64.const 1
  0x42, 0x7f,              // i64.const -1
  0x51,                    // i64.eq
  0x1a,                    // drop
  0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f,  // i64.const -9223372036854775808
  0x42, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,  // i64.const 9223372036854775807
  0x52,                    // i64.ne
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x53,                    // i64.lt_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x54,                    // i64.lt_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x55,                    // i64.gt_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x56,                    // i64.gt_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x57,                    // i64.le_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x58,                    // i64.le_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x59,                    // i64.ge_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x42, 0x00,              // i64.const 0
  0x5a,                    // i64.ge_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0x79,                    // i64.clz
  0x7a,                    // i64.ctz
  0x7b,                    // i64.popcnt
  0x42, 0x00,              // i64.const 0
  0x7c,                    // i64.add
  0x42, 0x00,              // i64.const 0
  0x7d,                    // i64.sub
  0x42, 0x00,              // i64.const 0
  0x7e,                    // i64.mul
  0x42, 0x00,              // i64.const 0
  0x7f,                    // i64.div_s
  0x42, 0x00,              // i64.const 0
  0x80,                    // i64.div_u
  0x42, 0x00,              // i64.const 0
  0x81,                    // i64.rem_s
  0x42, 0x00,              // i64.const 0
  0x82,                    // i64.rem_u
  0x42, 0x00,              // i64.const 0
  0x83,                    // i64.and
  0x42, 0x00,              // i64.const 0
  0x84,                    // i64.or
  0x42, 0x00,              // i64.const 0
  0x85,                    // i64.xor
  0x42, 0x00,              // i64.const 0
  0x86,                    // i64.shl
  0x42, 0x00,              // i64.const 0
  0x87,                    // i64.shr_s
  0x42, 0x00,              // i64.const 0
  0x88,                    // i64.shr_u
  0x42, 0x00,              // i64.const 0
  0x89,                    // i64.rotl
  0x42, 0x00,              // i64.const 0
  0x8a,                    // i64.rotr
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xac,                    // i64.extend_i32_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xad,                    // i64.extend_i32_u
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xae,                    // i64.trunc_f32_s
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xaf,                    // i64.trunc_f32_u
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xb0,                    // i64.trunc_f64_s
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xb1,                    // i64.trunc_f64_u
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xbd,                    // i64.reinterpret_f64
  0xc2,                    // i64.extend8_s
  0xc3,                    // i64.extend16_s
  0xc4,                    // i64.extend32_s
  0x0b,                    // end
                           // function #4 $func4
  0x9f, 0x01,              // body size 159
  0x00,                    // 0 entries in locals list
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x43, 0x00, 0x00, 0x00, 0x80,  // f32.const -0.0
  0x5b,                    // f32.eq
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x80, 0x3f,  // f32.const 1
  0x43, 0x00, 0x00, 0x80, 0xbf,  // f32.const -1
  0x5c,                    // f32.ne
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x80, 0x7f,  // f32.const inf
  0x43, 0x00, 0x00, 0x80, 0xff,  // f32.const -inf
  0x5d,                    // f32.lt
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0xc0, 0x7f,  // f32.const nan
  0x43, 0x00, 0x00, 0xc0, 0xff,  // f32.const -nan
  0x5e,                    // f32.gt
  0x1a,                    // drop
  0x43, 0x01, 0x00, 0x80, 0x7f,  // f32.const +nan:0x1
  0x43, 0xff, 0xff, 0x8f, 0x7f,  // f32.const +nan:0xfffff
  0x5f,                    // f32.le
  0x1a,                    // drop
  0x43, 0xcd, 0xcc, 0xcc, 0x3d,  // f32.const 0.100000001
  0x43, 0x3c, 0xb4, 0x96, 0x49,  // f32.const 1234567.5
  0x60,                    // f32.ge
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x8b,                    // f32.abs
  0x8c,                    // f32.neg
  0x8d,                    // f32.ceil
  0x8e,                    // f32.floor
  0x8f,                    // f32.trunc
  0x90,                    // f32.nearest
  0x91,                    // f32.sqrt
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x92,                    // f32.add
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x93,                    // f32.sub
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x94,                    // f32.mul
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x95,                    // f32.div
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x96,                    // f32.min
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x97,                    // f32.max
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x98,                    // f32.copysign
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xb2,                    // f32.convert_i32_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xb3,                    // f32.convert_i32_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xb4,                    // f32.convert_i64_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xb5,                    // f32.convert_i64_u
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xb6,                    // f32.demote_f64
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xbe,                    // f32.reinterpret_i32
  0x0b,                    // end
                           // function #5 $func5
  0xeb, 0x01,              // body size 235
  0x00,                    // 0 entries in locals list
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,  // f64.const -0.0
  0x61,                    // f64.eq
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,  // f64.const 1
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xbf,  // f64.const -1
  0x62,                    // f64.ne
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f,  // f64.const inf
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff,  // f64.const -inf
  0x63,                    // f64.lt
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f,  // f64.const nan
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xff,  // f64.const -nan
  0x64,                    // f64.gt
  0x1a,                    // drop
  0x44, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x7f,  // f64.const +nan:0x1
  0x44, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,  // f64.const +nan:0xfffffffffffff
  0x65,                    // f64.le
  0x1a,                    // drop
  0x44, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xb9, 0x3f,  // f64.const 0.1
  0x44, 0x00, 0x00, 0x00, 0x80, 0x87, 0xd6, 0x32, 0x41,  // f64.const 1234567.5
  0x66,                    // f64.ge
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0x99,                    // f64.abs
  0x9a,                    // f64.neg
  0x9b,                    // f64.ceil
  0x9c,                    // f64.floor
  0x9d,                    // f64.trunc
  0x9e,                    // f64.nearest
  0x9f,                    // f64.sqrt
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa0,                    // f64.add
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa1,                    // f64.sub
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa2,                    // f64.mul
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa3,                    // f64.div
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa4,                    // f64.min
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa5,                    // f64.max
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0xa6,                    // f64.copysign
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xb7,                    // f64.convert_i32_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0xb8,                    // f64.convert_i32_u
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xb9,                    // f64.convert_i64_s
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xba,                    // f64.convert_i64_u
  0x1a,                    // drop
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0xbb,                    // f64.promote_f32
  0x1a,                    // drop
  0x42, 0x00,              // i64.const 0
  0xbf,                    // f64.reinterpret_i64
  0x0b,                    // end
                           // function #6 $func6
  0x3c,                    // body size 60
  0x00,                    // 0 entries in locals list
  0x10, 0x07,              // call $func7
  0x42, 0x00,              // i64.const 0
  0x41, 0x00,              // i32.const 0
  0x11, 0x06, 0x00,        // call_indirect (param i64) (result f64)
  0x0f,                    // return
  0x02, 0x40,              // block $label0
  0x03, 0x40,              // loop $label1
  0x02, 0x7e,              // block (result i64)
  0x03, 0x7e,              // loop (result i64) $label2
  0x0c, 0x03,              // br $label0
  0x41, 0x00,              // i32.const 0
  0x0d, 0x02,              // br_if $label1
  0x41, 0x00,              // i32.const 0
  0x0e, 0x03, 0x03, 0x02, 0x02, 0x00,  // br_table $label0 $label1 $label1 $label2
  0x42, 0x00,              // i64.const 0
  0x0b,                    // end $label2
  0x0b,                    // end
  0x1a,                    // drop
  0x0b,                    // end $label1
  0x0b,                    // end $label0
  0x42, 0x00,              // i64.const 0
  0x42, 0x01,              // i64.const 1
  0x41, 0x00,              // i32.const 0
  0x1b,                    // select
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x04, 0x7e,              // if (result i64)
  0x42, 0x00,              // i64.const 0
  0x05,                    // else
  0x42, 0x01,              // i64.const 1
  0x0b,                    // end
  0x1a,                    // drop
  0x0b,                    // end
                           // function #7 $func7
  0xa4, 0x01,              // body size 164
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x28, 0x02, 0x00,        // i32.load
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x29, 0x03, 0x03,        // i64.load offset=3
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2a, 0x01, 0x00,        // f32.load align=2
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2b, 0x02, 0x03,        // f64.load offset=3 align=4
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2c, 0x00, 0x00,        // i32.load8_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2d, 0x00, 0x00,        // i32.load8_u
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2e, 0x01, 0x00,        // i32.load16_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x2f, 0x01, 0x00,        // i32.load16_u
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x30, 0x00, 0x00,        // i64.load8_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x31, 0x00, 0x00,        // i64.load8_u
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x32, 0x01, 0x00,        // i64.load16_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x33, 0x01, 0x00,        // i64.load16_u
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x34, 0x02, 0x00,        // i64.load32_s
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x35, 0x02, 0x00,        // i64.load32_u
  0x1a,                    // drop
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x36, 0x02, 0x00,        // i32.store
  0x41, 0x00,              // i32.const 0
  0x42, 0x00,              // i64.const 0
  0x37, 0x03, 0x00,        // i64.store
  0x41, 0x00,              // i32.const 0
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x38, 0x02, 0x00,        // f32.store
  0x41, 0x00,              // i32.const 0
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0x39, 0x03, 0x00,        // f64.store
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x3a, 0x00, 0x00,        // i32.store8
  0x41, 0x00,              // i32.const 0
  0x41, 0x00,              // i32.const 0
  0x3b, 0x01, 0x00,        // i32.store16
  0x41, 0x00,              // i32.const 0
  0x42, 0x00,              // i64.const 0
  0x3c, 0x00, 0x00,        // i64.store8
  0x41, 0x00,              // i32.const 0
  0x42, 0x00,              // i64.const 0
  0x3d, 0x01, 0x00,        // i64.store16
  0x41, 0x00,              // i32.const 0
  0x42, 0x00,              // i64.const 0
  0x3e, 0x02, 0x00,        // i64.store32
  0x3f, 0x00,              // memory.size
  0x40, 0x00,              // memory.grow
  0x1a,                    // drop
  0x0b,                    // end
                           // function #8 $exported_function
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x01,                    // nop
  0x00,                    // unreachable
  0x0b,                    // end

  0x0b,                    // section kind: Data
  0x0b,                    // section length 11
  0x01, 0x00,              // data segments count 1: flag: active no index
  0x23, 0x00, 0x0b,        // global.get $env.imported_global
  0x05,                    // source size 5
  0x66, 0x6f, 0x6f, 0x0a, 0x00,  // segment data
