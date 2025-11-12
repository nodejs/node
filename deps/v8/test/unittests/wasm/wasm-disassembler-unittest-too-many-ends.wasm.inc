0x00, 0x61, 0x73, 0x6d,  // wasm magic
0x01, 0x00, 0x00, 0x00,  // wasm version

0x01,                    // section kind: Type
0x04,                    // section length 4
0x01, 0x60,              // types count 1:  kind: func
0x00,                    // param count 0
0x00,                    // return count 0

0x03,                    // section kind: Function
0x02,                    // section length 2
0x01, 0x00,              // functions count 1: 0 $doubleEnd

0x07,                    // section kind: Export
0x0d,                    // section length 13
0x01,                    // exports count 1: export # 0
0x09,                    // field name length: 9
0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x45, 0x6e,
0x64,                    // field name: doubleEnd
0x00, 0x00,              // kind: function index: 0

0x0a,                    // section kind: Code
0x07,                    // section length 7
0x01,                    // functions count 1
                          // function #0 $doubleEnd
0x05,                    // body size 5
0x00,                    // 0 entries in locals list
0x01,                    // nop
0x0b,                    // end
0x0b,                    // end
0x0b,                    // end
