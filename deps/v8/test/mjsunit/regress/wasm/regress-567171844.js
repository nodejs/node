// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors --wasm-js-interop

const wasm_bytes = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x1a,                    // section length 26
  0x06,                    // types count 6
  0x5e, 0x6f, 0x01,        // type #0 $type0 kind: array externref mutable
  0x5e, 0x70, 0x01,        // type #1 $type1 kind: array funcref mutable
  0x5e, 0x78, 0x01,        // type #2 $type2 kind: array i8 mutable
  0x60,                    // type #3 $type3 kind: func
  0x04,                    // param count 4
  0x63, 0x00, 0x63, 0x01,  // (ref null $type0) (ref null $type1)
  0x63, 0x02, 0x6f,        // (ref null $type2) externref
  0x00,                    // return count 0
  0x60,                    // type #4 $type4 kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0
  0x60,                    // type #5 $type5 kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0

  0x02,                    // section kind: Import
  0x51,                    // section length 81
  0x04,                    // imports count 4
  0x12,                    // module name length:  18
  0x77, 0x61, 0x73, 0x6d, 0x3a, 0x6a, 0x73, 0x2d,
  0x70, 0x72, 0x6f, 0x74, 0x6f, 0x74, 0x79, 0x70,
  0x65, 0x73,              // module name: wasm:js-prototypes
  0x0c,                    // field name length:  12
  0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x75, 0x72,
  0x65, 0x41, 0x6c, 0x6c,  // field name: configureAll
  0x00, 0x03,              // kind or compact flag: function (param (ref null $type0) (ref null $type1) (ref null $type2) externref)
                           // import #0
  0x01,                    // module name length:  1
  0x63,                    // module name: c
  0x0c,                    // field name length:  12
  0x63, 0x6f, 0x6e, 0x73, 0x74, 0x72, 0x75, 0x63,
  0x74, 0x6f, 0x72, 0x73,  // field name: constructors
  0x03, 0x6f, 0x00,        // kind or compact flag: global externref immutable
                           // import #1
  0x03,                    // module name length:  3
  0x6d, 0x6f, 0x64,        // module name: mod
  0x0a,                    // field name length:  10
  0x6f, 0x75, 0x74, 0x65, 0x72, 0x5f, 0x66, 0x75,
  0x6e, 0x63,              // field name: outer_func
  0x00, 0x04,              // kind or compact flag: function
                           // import #2
  0x01,                    // module name length:  1
  0x63,                    // module name: c
  0x05,                    // field name length:  5
  0x70, 0x72, 0x6f, 0x74, 0x6f,  // field name: proto
  0x03, 0x6f, 0x00,        // kind or compact flag: global externref immutable
                           // import #3

  0x03,                    // section kind: Function
  0x03,                    // section length 3
  0x02,                    // functions count 2
  0x04,                    // 2 $myMethod
  0x05,                    // 3 $start

  0x08,                    // section kind: Start
  0x01,                    // section length 1
  0x03,                    // index:  3

  0x09,                    // section kind: Element
  0x0b,                    // section length 11
  0x02,                    // segment count 2
  0x05, 0x6f, 0x01,        // flag: passive, element type: externref number of elements 1:
  0x23, 0x00, 0x0b,        // global.get $c.constructors
  0x01, 0x00, 0x01,        // flag: passive, element type: function number of elements 1:
  0x02,                    // index:  2

  0x0c,                    // section kind: DataCount
  0x01,                    // section length 1
  0x01,                    // data segments count 1:

  0x0a,                    // section kind: Code
  0x25,                    // section length 37
  0x02,                    // functions count 2
                           // function #2 $myMethod
  0x04,                    // body size 4
  0x00,                    // 0 entries in locals list
  0x10, 0x01,              // call $mod.outer_func
  0x0b,                    // end
                           // function #3 $start
  0x1e,                    // body size 30
  0x00,                    // 0 entries in locals list
  0x41, 0x00,              // i32.const 0
  0x41, 0x01,              // i32.const 1
  0xfb, 0x0a, 0x00, 0x00,  // array.new_elem $type0 $elem0
  0x41, 0x00,              // i32.const 0
  0x41, 0x01,              // i32.const 1
  0xfb, 0x0a, 0x01, 0x01,  // array.new_elem $type1 $elem1
  0x41, 0x00,              // i32.const 0
  0x41, 0x0e,              // i32.const 14
  0xfb, 0x09, 0x02, 0x00,  // array.new_data $type2 0
  0x23, 0x00,              // global.get $c.constructors
  0x10, 0x00,              // call $wasm:js-prototypes.configureAll
  0x0b,                    // end

  0x0b,                    // section kind: Data
  0x11,                    // section length 17
  0x01, 0x01,              // data segments count 1: flag: passive
  0x0e,                    // source size 14
  0x01, 0x00, 0x01, 0x00, 0x08, 0x6d, 0x79, 0x4d,
  0x65, 0x74, 0x68, 0x6f, 0x64, 0x7f,
                           // segment data

  0x00,                    // section kind: Unknown
  0x19,                    // section length 25
  0x04,                    // section name length:  4
  0x6e, 0x61, 0x6d, 0x65,  // section name: name
  0x01,                    // name type: function
  0x12,                    // payload length: 18
  0x02,                    // names count 2
  0x02, 0x08,              // index 2 name length:  8
  0x6d, 0x79, 0x4d, 0x65, 0x74, 0x68, 0x6f, 0x64,  // name: myMethod
  0x03, 0x05,              // index 3 name length:  5
  0x73, 0x74, 0x61, 0x72, 0x74,  // name: start
]);

const myProto = {};
const constructors = {};
const handler = { apply: function() { return 42; } };
const proxy = new Proxy(function() {}, handler);

const wasm_module = new WebAssembly.Module(wasm_bytes,
  { builtins: ["js-prototypes"] });
const wasm_instance = new WebAssembly.Instance(
  wasm_module,
  {
    mod: {
      outer_func: proxy
    },
    c: { proto: myProto, constructors: constructors }
  }
);

for (let i = 0; i < 1000; i++) {
  constructors.myMethod();
}

handler.apply = 123;

assertThrows(() => constructors.myMethod(), TypeError,
  "number 123 is not a function");
