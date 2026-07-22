  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x1b,                    // section length 27
  0x02,                    // types count 2
  0x4e,                    // rec. group definition
  0x03,                    // recursive group size 3
  0x50, 0x00,              // type #0 $type0 subtype extensible, supertype count 0
  0x4d, 0x01,              //  descriptor 1
  0x5f, 0x00,              //  kind: struct, field count 0
  0x50, 0x00,              // type #1 $type1 subtype extensible, supertype count 0
  0x4c, 0x00,              //  describes 0
  0x4d, 0x02,              //  descriptor 2
  0x5f, 0x00,              //  kind: struct, field count 0
  0x50, 0x00,              // type #2 $type2 subtype extensible, supertype count 0
  0x4c, 0x01,              //  describes 1
  0x5f, 0x00,              //  kind: struct, field count 0
                           // end of rec. group
  0x60,                    // type #3 $type3 kind: func
  0x00,                    // param count 0
  0x01, 0x6e,              // return count 1:  anyref

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x03,              // functions count 1: 0 $func0 (result anyref)

  0x0a,                    // section kind: Code
  0x1c,                    // section length 28
  0x01,                    // functions count 1
                           // function #0 $func0
  0x1a,                    // body size 26
  0x00,                    // 0 entries in locals list
  0xd0, 0x71,              // ref.null none
  0xfb, 0x01, 0x02,        // struct.new_default $type2
  0xfb, 0x01, 0x01,        // struct.new_default $type1
  0xfb, 0x23, 0x62, 0x00,  // ref.cast_desc exact $type0
  0xfb, 0x22, 0x00,        // ref.get_desc $type0
  0xfb, 0x01, 0x02,        // struct.new_default $type2
  0xfb, 0x25, 0x00, 0x00, 0x6b, 0x01,  // br_on_cast_desc 0 (ref struct) (ref $type1)
  0x0b,                    // end
