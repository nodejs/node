// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// During Turbofan optimizations, when a TrapIf/Unless node is found to always
// trap, its uses need to be marked as dead. However, in the case that one of
// these uses is a Merge or Loop node, only the input of the Merge/Loop that
// corresponds to the trap should be marked as dead.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction('test', makeSig([wasmRefNullType(0)], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprRefIsNull,
      kExprIf, kWasmI32,
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, 0, 0,
      kExprElse,
        kExprI32Const, 42,
      kExprEnd
    ])
    .exportFunc();
builder.instantiate();

// We include a clusterfuzz-generated testcase for this error verbatim.
const module = new WebAssembly.Module(new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x33,                    // section length 51
  0x09,                    // types count 9
  0x60,                    // type #0 $type0 kind: func
  0x00,                    // param count 0
  0x00,                    // return count 0
  0x60,                    // type #1 $type1 kind: func
  0x00,                    // param count 0
  0x01, 0x7d,              // return count 1:  f32
  0x60,                    // type #2 $type2 kind: func
  0x00,                    // param count 0
  0x01, 0x7c,              // return count 1:  f64
  0x60,                    // type #3 $type3 kind: func
  0x02,                    // param count 2
  0x7c, 0x7f,              // f64 i32
  0x01, 0x7d,              // return count 1:  f32
  0x60,                    // type #4 $type4 kind: func
  0x04,                    // param count 4
  0x7e, 0x7e, 0x7d, 0x7f,  // i64 i64 f32 i32
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #5 $type5 kind: func
  0x01, 0x7e,              // param count 1:  i64
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #6 $type6 kind: func
  0x07,                    // param count 7
  0x7f, 0x7e, 0x7e, 0x7d,  // i32 i64 i64 f32
  0x7c, 0x7f, 0x7d,        // f64 i32 f32
  0x01, 0x7c,              // return count 1:  f64
  0x60,                    // type #7 $type7 kind: func
  0x00,                    // param count 0
  0x01, 0x7f,              // return count 1:  i32
  0x60,                    // type #8 $type8 kind: func
  0x01, 0x7c,              // param count 1:  f64
  0x01, 0x7d,              // return count 1:  f32

  0x03,                    // section kind: Function
  0x17,                    // section length 23
  0x16,                    // functions count 22
  0x00,                    // 0 $func_0
  0x04,                    // 1 $func1 (param i64 i64 f32 i32) (result i32)
  0x00,                    // 2 $func_1_invoker
  0x05,                    // 3 $func3 (param i64) (result i32)
  0x06,                    // 4 $func4 (param i32 i64 i64 f32 f64 i32 f32) (result f64)
  0x00,                    // 5 $func_4_invoker
  0x07,                    // 6 $func6 (result i32)
  0x00,                    // 7 $func_6_invoker
  0x02,                    // 8 $func8 (result f64)
  0x00,                    // 9 $func_8_invoker
  0x03,                    // 10 $func10 (param f64 i32) (result f32)
  0x01,                    // 11 $func_11 (result f32)
  0x00,                    // 12 $func_11_invoker
  0x08,                    // 13 $func13 (param f64) (result f32)
  0x00,                    // 14 $func_13_invoker
  0x00,                    // 15 $func_15
  0x00,                    // 16 $func_15_invoker
  0x00,                    // 17 $func17
  0x00,                    // 18 $func_17_invoker
  0x02,                    // 19 $func_19 (result f64)
  0x02,                    // 20 $func_20 (result f64)
  0x00,                    // 21 $hangLimitInitializer

  0x04,                    // section kind: Table
  0x05,                    // section length 5
  0x01, 0x70, 0x01,        // table count 1:  funcref with maximum
  0x09,                    // initial size 9
  0x09,                    // maximum size 9

  0x05,                    // section kind: Memory
  0x04,                    // section length 4
  0x01, 0x03,              // memory count 1:  shared with maximum
  0x01,                    // initial size 1
  0x01,                    // maximum size 1

  0x06,                    // section kind: Global
  0x06,                    // section length 6
  0x01, 0x7f, 0x01,        // globals count 1: global #0: i32 mutable
  0x41, 0x0a, 0x0b,        // i32.const 10

  0x07,                    // section kind: Export
  0xd5, 0x01,              // section length 213
  0x0e,                    // exports count 14
                           // export # 0
  0x06,                    // field name length:  6
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x30,  // field name: func_0
  0x00, 0x00,              // kind: function index:  0
                           // export # 1
  0x0e,                    // field name length:  14
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x5f, 0x69,
  0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_1_invoker
  0x00, 0x02,              // kind: function index:  2
                           // export # 2
  0x0e,                    // field name length:  14
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x34, 0x5f, 0x69,
  0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_4_invoker
  0x00, 0x05,              // kind: function index:  5
                           // export # 3
  0x0e,                    // field name length:  14
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x36, 0x5f, 0x69,
  0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_6_invoker
  0x00, 0x07,              // kind: function index:  7
                           // export # 4
  0x0e,                    // field name length:  14
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x38, 0x5f, 0x69,
  0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_8_invoker
  0x00, 0x09,              // kind: function index:  9
                           // export # 5
  0x07,                    // field name length:  7
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x31,  // field name: func_11
  0x00, 0x0b,              // kind: function index:  11
                           // export # 6
  0x0f,                    // field name length:  15
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x31, 0x5f,
  0x69, 0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_11_invoker
  0x00, 0x0c,              // kind: function index:  12
                           // export # 7
  0x0f,                    // field name length:  15
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x33, 0x5f,
  0x69, 0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_13_invoker
  0x00, 0x0e,              // kind: function index:  14
                           // export # 8
  0x07,                    // field name length:  7
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x35,  // field name: func_15
  0x00, 0x0f,              // kind: function index:  15
                           // export # 9
  0x0f,                    // field name length:  15
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x35, 0x5f,
  0x69, 0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_15_invoker
  0x00, 0x10,              // kind: function index:  16
                           // export # 10
  0x0f,                    // field name length:  15
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x37, 0x5f,
  0x69, 0x6e, 0x76, 0x6f, 0x6b, 0x65, 0x72,
                           // field name: func_17_invoker
  0x00, 0x12,              // kind: function index:  18
                           // export # 11
  0x07,                    // field name length:  7
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x31, 0x39,  // field name: func_19
  0x00, 0x13,              // kind: function index:  19
                           // export # 12
  0x07,                    // field name length:  7
  0x66, 0x75, 0x6e, 0x63, 0x5f, 0x32, 0x30,  // field name: func_20
  0x00, 0x14,              // kind: function index:  20
                           // export # 13
  0x14,                    // field name length:  20
  0x68, 0x61, 0x6e, 0x67, 0x4c, 0x69, 0x6d, 0x69,
  0x74, 0x49, 0x6e, 0x69, 0x74, 0x69, 0x61, 0x6c,
  0x69, 0x7a, 0x65, 0x72,  // field name: hangLimitInitializer
  0x00, 0x15,              // kind: function index:  21

  0x09,                    // section kind: Element
  0x0f,                    // section length 15
  0x01, 0x00,              // segment count 1: flag: active, offset:
  0x41, 0x00, 0x0b,        // i32.const 0
  0x09,                    //  number of elements 9
  0x04,                    // index:  4
  0x06,                    // index:  6
  0x06,                    // index:  6
  0x08,                    // index:  8
  0x0a,                    // index:  10
  0x0b,                    // index:  11
  0x0b,                    // index:  11
  0x0f,                    // index:  15
  0x0f,                    // index:  15

  0x0a,                    // section kind: Code
  0xdc, 0x12,              // section length 2396
  0x16,                    // functions count 22
                           // function #0 $func_0
  0x71,                    // body size 113
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop $label2
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x80, 0x80, 0x80, 0x04,  // i32.const 8388608
  0x0b,                    // end
  0x04, 0x7f,              // if (result i32)
  0x41, 0xc1, 0xff, 0x03,  // i32.const 65473
  0x05,                    // else
  0x02, 0x7f,              // block (result i32) $label1
  0x03, 0x40,              // loop $label0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0c, 0x01,              // br $label0
  0x0b,                    // end
  0x00,                    // unreachable
  0x41, 0x00,              // i32.const 0
  0x0d, 0x01,              // br_if $label1
  0x00,                    // unreachable
  0x0b,                    // end $label0
  0x00,                    // unreachable
  0x0b,                    // end $label1
  0x0b,                    // end
  0x1a,                    // drop
  0x0c, 0x00,              // br $label2
  0x0b,                    // end $label2
  0x00,                    // unreachable
  0x0b,                    // end
                           // function #1 $func1
  0x83, 0x03,              // body size 387
  0x01,                    // 1 entries in locals list
  0x01, 0x7d,              // 1 local of type f32
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x80, 0x80, 0x02,  // i32.const 4194304
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32) $label0
  0x02, 0x40,              // block $label4
  0x42, 0x9d, 0xe4, 0xc1, 0x93, 0x7f,  // i64.const -227511779
  0x03, 0x7e,              // loop (result i64) $label1
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0xe0, 0xc4, 0x7e,  // i32.const -23968
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x81, 0x80, 0x7c,  // i32.const -65535
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x03,              // local.get $var3
  0x41, 0x69,              // i32.const -23
  0x0d, 0x02,              // br_if $label0
  0x0d, 0x00,              // br_if $label1
  0x42, 0x80, 0x80, 0x80, 0x80, 0xc0, 0x00,  // i64.const 17179869184
  0x0b,                    // end $label1
  0x02, 0x7d,              // block (result f32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x20, 0x03,              // local.get $var3
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x43, 0x00, 0x00, 0x50, 0xc1,  // f32.const -13
  0x20, 0x02,              // local.get $var2
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x75,              // i32.const -11
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x03,              // local.get $var3
  0x0b,                    // end
  0x1b,                    // select
  0x22, 0x04,              // local.tee $var4
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x20, 0x04,              // local.get $var4
  0x20, 0x04,              // local.get $var4
  0x5b,                    // f32.eq
  0x1b,                    // select
  0x0b,                    // end
  0x20, 0x03,              // local.get $var3
  0x10, 0x01,              // call $func1
  0x03, 0x7f,              // loop (result i32) $label2
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0xa8, 0xba, 0x7e,  // i32.const -25304
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x01,        // i32.const 128
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0xff, 0x00,        // i32.const 127
  0x20, 0x03,              // local.get $var3
  0x45,                    // i32.eqz
  0x0d, 0x02,              // br_if $label0
  0x22, 0x03,              // local.tee $var3
  0x0d, 0x00,              // br_if $label2
  0x20, 0x03,              // local.get $var3
  0x0b,                    // end $label2
  0x45,                    // i32.eqz
  0x0d, 0x01,              // br_if $label0
  0x20, 0x03,              // local.get $var3
  0x45,                    // i32.eqz
  0x0d, 0x01,              // br_if $label0
  0x41, 0xdc, 0xbc, 0x7e,  // i32.const -24996
  0x0d, 0x01,              // br_if $label0
  0x22, 0x03,              // local.tee $var3
  0x04, 0x40,              // if
  0x02, 0x40,              // block $label5
  0x02, 0x7f,              // block (result i32) $label3
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x80, 0x80, 0x80, 0x78,  // i32.const -2147483648
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x03,              // local.get $var3
  0x20, 0x03,              // local.get $var3
  0x0d, 0x00,              // br_if $label3
  0x0d, 0x03,              // br_if $label4
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x20, 0x03,              // local.get $var3
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0c, 0x01,              // br $label5
  0x0b,                    // end $label3
  0x1a,                    // drop
  0x03, 0x7f,              // loop (result i32) $label6
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x20, 0x03,              // local.get $var3
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x03,              // local.get $var3
  0x0d, 0x00,              // br_if $label6
  0x41, 0x01,              // i32.const 1
  0x0b,                    // end $label6
  0x1a,                    // drop
  0x0c, 0x02,              // br $label4
  0x0b,                    // end $label5
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0xa7, 0x7f,        // i32.const -89
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0xc0, 0x00,  // i32.const 8192
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x20, 0x03,              // local.get $var3
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x93, 0x7f,        // i32.const -109
  0x0c, 0x02,              // br $label0
  0x05,                    // else
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x81, 0x80, 0x80, 0x80, 0x78,  // i32.const -2147483647
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0b,                    // end
  0x0b,                    // end $label4
  0x41, 0xff, 0xff, 0x7d,  // i32.const -32769
  0x0b,                    // end $label0
  0x0b,                    // end
                           // function #2 $func_1_invoker
  0x21,                    // body size 33
  0x00,                    // 0 entries in locals list
  0x42, 0xfc, 0x82, 0xdd, 0xff, 0x0f,  // i64.const 4294394236
  0x42, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,  // i64.const 9223372036854775807
  0x43, 0x00, 0x00, 0xea, 0x42,  // f32.const 117
  0x41, 0xfc, 0xe0, 0xa8, 0xb3, 0x7a,  // i32.const -1502990212
  0x10, 0x01,              // call $func1
  0x1a,                    // drop
  0x0b,                    // end
                           // function #3 $func3
  0xb2, 0x02,              // body size 306
  0x01,                    // 1 entries in locals list
  0x02, 0x7f,              // 2 locals of type i32
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x78,              // i32.const -8
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32) $label0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x80, 0x08,        // i32.const 1024
  0x0b,                    // end
  0x04, 0x7f,              // if (result i32)
  0x41, 0x00,              // i32.const 0
  0x05,                    // else
  0x02, 0x7f,              // block (result i32) $label4
  0x41, 0x00,              // i32.const 0
  0x41, 0x81, 0x7e,        // i32.const -255
  0x45,                    // i32.eqz
  0x0d, 0x02,              // br_if $label0
  0x04, 0x40,              // if
  0x03, 0x40,              // loop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x9f, 0xd8, 0x89, 0x7c,  // i32.const -8229857
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x00,              // i32.const 0
  0x28, 0x02, 0x03,        // i32.load offset=3
  0x1a,                    // drop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0xde, 0x88, 0x7e,  // i32.const -31650
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop $label1
  0x23, 0x00,              // global.get $global0
  0x04, 0x40,              // if
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0c, 0x01,              // br $label1
  0x05,                    // else
  0x41, 0x80, 0x08,        // i32.const 1024
  0x0f,                    // return
  0x0b,                    // end
  0x00,                    // unreachable
  0x0b,                    // end $label1
  0x00,                    // unreachable
  0x0b,                    // end
  0x00,                    // unreachable
  0x05,                    // else
  0x03, 0x40,              // loop $label3
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32) $label2
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x00,              // i32.const 0
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0b,                    // end
  0x41, 0x01,              // i32.const 1
  0xfe, 0x12, 0x00, 0x16,  // i32.atomic.load8_u offset=22
  0x0b,                    // end
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label2
  0x0b,                    // end $label2
  0x0d, 0x00,              // br_if $label3
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x7c,        // i32.const -512
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0xe0, 0xd8, 0x02,  // i32.const 44128
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x80, 0x02,  // i32.const 32768
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0xbe, 0x7f,        // i32.const -66
  0x0c, 0x03,              // br $label4
  0x0b,                    // end
  0x00,                    // unreachable
  0x0b,                    // end $label3
  0x00,                    // unreachable
  0x0b,                    // end
  0x00,                    // unreachable
  0x0b,                    // end $label4
  0x0b,                    // end
  0x0b,                    // end $label0
  0x0b,                    // end
                           // function #4 $func4
  0x17,                    // body size 23
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x20, 0x04,              // local.get $var4
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x00,              // i32.const 0
  0x2b, 0x03, 0x02,        // f64.load offset=2
  0x0b,                    // end
                           // function #5 $func_4_invoker
  0x74,                    // body size 116
  0x00,                    // 0 entries in locals list
  0x41, 0x8d, 0xb0, 0x7e,  // i32.const -26611
  0x42, 0xd9, 0xec, 0x7e,  // i64.const -18855
  0x42, 0x80, 0x01,        // i64.const 128
  0x43, 0x00, 0x00, 0x00, 0x4f,  // f32.const 2.14748365e+09
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x40,  // f64.const 64
  0x41, 0x4c,              // i32.const -52
  0x43, 0x00, 0x00, 0x80, 0x5f,  // f32.const 1.84467441e+19
  0x10, 0x04,              // call $func4
  0x1a,                    // drop
  0x41, 0x20,              // i32.const 32
  0x42, 0x81, 0x80, 0x80, 0x80, 0x78,  // i64.const -2147483647
  0x42, 0xe6, 0xd4, 0x9c, 0xfc, 0x0f,  // i64.const 4287048294
  0x43, 0x00, 0x00, 0xa0, 0x40,  // f32.const 5
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x43,  // f64.const 9223372036854776000
  0x41, 0x7f,              // i32.const -1
  0x43, 0x00, 0x00, 0x80, 0x80,  // f32.const -1.17549435e-38
  0x10, 0x04,              // call $func4
  0x1a,                    // drop
  0x41, 0xff, 0xa6, 0xc8, 0xb1, 0x7b,  // i32.const -1238232193
  0x42, 0xb9, 0x7f,        // i64.const -71
  0x42, 0x80, 0x80, 0x80, 0x80, 0x08,  // i64.const 2147483648
  0x43, 0x00, 0x00, 0x00, 0x5d,  // f32.const 5.76460752e+17
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x43,  // f64.const 36028797018963970
  0x41, 0x96, 0xe0, 0x7e,  // i32.const -20458
  0x43, 0x00, 0x00, 0x00, 0x58,  // f32.const 5.62949953e+14
  0x10, 0x04,              // call $func4
  0x1a,                    // drop
  0x0b,                    // end
                           // function #6 $func6
  0x6f,                    // body size 111
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x90, 0xc2, 0x00,  // i32.const 8464
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop $label1
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x00,              // i32.const 0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop $label0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x7c,              // i32.const -4
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x6f,              // i32.const -17
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x7f,              // loop (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x41, 0x80, 0x80, 0x02,  // i32.const 32768
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x80, 0x80, 0x7e,  // i32.const -32768
  0x0b,                    // end
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label0
  0x0c, 0x01,              // br $label1
  0x0b,                    // end $label0
  0x00,                    // unreachable
  0x45,                    // i32.eqz
  0x00,                    // unreachable
  0x0d, 0x00,              // br_if $label1
  0x00,                    // unreachable
  0x0b,                    // end $label1
  0x00,                    // unreachable
  0x0b,                    // end
                           // function #7 $func_6_invoker
  0x0e,                    // body size 14
  0x00,                    // 0 entries in locals list
  0x10, 0x06,              // call $func6
  0x1a,                    // drop
  0x10, 0x06,              // call $func6
  0x1a,                    // drop
  0x10, 0x06,              // call $func6
  0x1a,                    // drop
  0x10, 0x06,              // call $func6
  0x1a,                    // drop
  0x0b,                    // end
                           // function #8 $func8
  0x22,                    // body size 34
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x43,  // f64.const 9223372036854776000
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x44, 0x1a, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffc01a
  0x0b,                    // end
                           // function #9 $func_8_invoker
  0x05,                    // body size 5
  0x00,                    // 0 entries in locals list
  0x10, 0x08,              // call $func8
  0x1a,                    // drop
  0x0b,                    // end
                           // function #10 $func10
  0x1a,                    // body size 26
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x43, 0x00, 0x00, 0x00, 0x00,  // f32.const 0.0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x43, 0x00, 0x00, 0x80, 0xd6,  // f32.const -7.03687442e+13
  0x0b,                    // end
                           // function #11 $func_11
  0x1a,                    // body size 26
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x43, 0x00, 0x00, 0x00, 0x5a,  // f32.const 9.00719925e+15
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x43, 0x00, 0x00, 0x2c, 0xc2,  // f32.const -43
  0x0b,                    // end
                           // function #12 $func_11_invoker
  0x08,                    // body size 8
  0x00,                    // 0 entries in locals list
  0x10, 0x0b,              // call $func_11
  0x1a,                    // drop
  0x10, 0x0b,              // call $func_11
  0x1a,                    // drop
  0x0b,                    // end
                           // function #13 $func13
  0x1a,                    // body size 26
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x43, 0x00, 0x00, 0x00, 0xc5,  // f32.const -2048
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x43, 0x75, 0xe3, 0xff, 0xff,  // f32.const -nan:0x7fe375
  0x0b,                    // end
                           // function #14 $func_13_invoker
  0x26,                    // body size 38
  0x00,                    // 0 entries in locals list
  0x44, 0x81, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffff81
  0x10, 0x0d,              // call $func13
  0x1a,                    // drop
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x41,  // f64.const 262144
  0x10, 0x0d,              // call $func13
  0x1a,                    // drop
  0x44, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffffc1
  0x10, 0x0d,              // call $func13
  0x1a,                    // drop
  0x0b,                    // end
                           // function #15 $func_15
  0x1e,                    // body size 30
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0b,                    // end
                           // function #16 $func_15_invoker
  0x06,                    // body size 6
  0x00,                    // 0 entries in locals list
  0x10, 0x0f,              // call $func_15
  0x10, 0x0f,              // call $func_15
  0x0b,                    // end
                           // function #17 $func17
  0x10,                    // body size 16
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0b,                    // end
                           // function #18 $func_17_invoker
  0x08,                    // body size 8
  0x00,                    // 0 entries in locals list
  0x10, 0x11,              // call $func17
  0x10, 0x11,              // call $func17
  0x10, 0x11,              // call $func17
  0x0b,                    // end
                           // function #19 $func_19
  0x34,                    // body size 52
  0x00,                    // 0 entries in locals list
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // f64.const 0.0
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x7c,              // loop (result f64) $label0
  0x23, 0x00,              // global.get $global0
  0x04, 0x7c,              // if (result f64)
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x0c, 0x01,              // br $label0
  0x05,                    // else
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x6d, 0x40,  // f64.const 236
  0x0b,                    // end
  0x0b,                    // end $label0
  0x0b,                    // end
                           // function #20 $func_20
  0xda, 0x07,              // body size 986
  0x03,                    // 3 entries in locals list
  0x04, 0x7f,              // 4 locals of type i32
  0x01, 0x7e,              // 1 local of type i64
  0x02, 0x7d,              // 2 locals of type f32
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff,  // f64.const -1.7976931348623157e+308
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7c,              // block (result f64) $label14
  0x03, 0x40,              // loop $label3
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0xc0,  // f64.const -13
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x40,              // block $label1
  0x03, 0x40,              // loop $label2
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x40,  // f64.const 4096
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x80, 0x7f,        // i32.const -128
  0x22, 0x02,              // local.tee $var2
  0x04, 0x7f,              // if (result i32)
  0x20, 0x00,              // local.get $var0
  0x05,                    // else
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0xc0, 0x89, 0xcf, 0xfa, 0xef, 0x41,  // f64.const 4292246606
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x40,              // loop $label0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0xf5, 0xff, 0xff, 0xef, 0x41,  // f64.const 4294967208
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x86, 0x52,        // i32.const -5882
  0x22, 0x00,              // local.tee $var0
  0x21, 0x03,              // local.set $var3
  0x20, 0x01,              // local.get $var1
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label0
  0x0b,                    // end $label0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0xc0,  // f64.const -1024
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x01,              // local.get $var1
  0x45,                    // i32.eqz
  0x0d, 0x02,              // br_if $label1
  0x20, 0x04,              // local.get $var4
  0x10, 0x03,              // call $func3
  0x0d, 0x01,              // br_if $label2
  0x41, 0x74,              // i32.const -12
  0x21, 0x00,              // local.set $var0
  0x0c, 0x03,              // br $label3
  0x0b,                    // end
  0x21, 0x02,              // local.set $var2
  0x03, 0x7f,              // loop (result i32) $label4
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x4d, 0x45, 0x1d, 0x91, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffff911d454d
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x01,              // local.get $var1
  0x0d, 0x00,              // br_if $label4
  0x20, 0x02,              // local.get $var2
  0x22, 0x00,              // local.tee $var0
  0x22, 0x01,              // local.tee $var1
  0x0b,                    // end $label4
  0x0d, 0x00,              // br_if $label2
  0x0b,                    // end $label2
  0x03, 0x40,              // loop $label10
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x40,  // f64.const 16
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x66, 0x40,  // f64.const 181
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x01,              // local.get $var1
  0x21, 0x02,              // local.set $var2
  0x41, 0x07,              // i32.const 7
  0x11, 0x00, 0x00,        // call_indirect
  0x03, 0x7f,              // loop (result i32) $label11
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f,  // f64.const 1
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x4e, 0xc0,  // f64.const -61
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x42, 0x81, 0x80, 0x80, 0x80, 0x78,  // i64.const -2147483647
  0x42, 0x80, 0x80, 0x02,  // i64.const 32768
  0x20, 0x00,              // local.get $var0
  0x1b,                    // select
  0x21, 0x04,              // local.set $var4
  0x41, 0xb1, 0x98, 0x7e,  // i32.const -29647
  0x0b,                    // end
  0x04, 0x40,              // if
  0x03, 0x40,              // loop $label5
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xc3,  // f64.const -1125899906842624
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x10, 0x06,              // call $func6
  0x41, 0x0f,              // i32.const 15
  0x71,                    // i32.and
  0x41, 0x82, 0x80, 0x7e,  // i32.const -32766
  0xfe, 0x00, 0x02, 0x00,  // memory.atomic.notify
  0x04, 0x40,              // if
  0x20, 0x00,              // local.get $var0
  0x20, 0x01,              // local.get $var1
  0x20, 0x02,              // local.get $var2
  0x1b,                    // select
  0x04, 0x7f,              // if (result i32)
  0x41, 0xcf, 0xe6, 0x9d, 0x99, 0x04,  // i32.const 1126658895
  0x22, 0x00,              // local.tee $var0
  0x05,                    // else
  0x41, 0x8c, 0xe2, 0x84, 0xbb, 0x06,  // i32.const 1734422796
  0x0b,                    // end
  0x1a,                    // drop
  0x05,                    // else
  0x43, 0x97, 0xff, 0xff, 0xff,  // f32.const -nan:0x7fff97
  0x21, 0x06,              // local.set $var6
  0x0b,                    // end
  0x20, 0x02,              // local.get $var2
  0x0d, 0x00,              // br_if $label5
  0x42, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01,  // i64.const 34359738368
  0x21, 0x04,              // local.set $var4
  0x0b,                    // end $label5
  0x0b,                    // end
  0x03, 0x40,              // loop $label8
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x20, 0xfa, 0xef, 0x40,  // f64.const 65489
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x06,              // local.get $var6
  0x1a,                    // drop
  0x03, 0x7f,              // loop (result i32) $label7
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x43,  // f64.const 144115188075855870
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x7f,              // loop (result i32) $label6
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4d, 0x40,  // f64.const 58
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x43, 0x50, 0xff, 0x37, 0xca,  // f32.const -3014612
  0x21, 0x06,              // local.set $var6
  0x20, 0x02,              // local.get $var2
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label6
  0x41, 0x6e,              // i32.const -18
  0x0b,                    // end $label6
  0x22, 0x03,              // local.tee $var3
  0x0d, 0x04,              // br_if $label1
  0x20, 0x02,              // local.get $var2
  0x21, 0x00,              // local.set $var0
  0x20, 0x03,              // local.get $var3
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label7
  0x41, 0x80, 0x60,        // i32.const -4096
  0x0b,                    // end $label7
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label8
  0x20, 0x01,              // local.get $var1
  0x04, 0x7f,              // if (result i32)
  0x02, 0x7f,              // block (result i32) $label9
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x8a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffff8a
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0xd7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffffd7
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0xb9, 0x7f,        // i32.const -71
  0x02, 0x7f,              // block (result i32)
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xc3,  // f64.const -9223372036854776000
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x00,              // i32.const 0
  0x0b,                    // end
  0x0d, 0x00,              // br_if $label9
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x42,  // f64.const 281474976710656
  0x20, 0x03,              // local.get $var3
  0x41, 0x04,              // i32.const 4
  0x11, 0x03, 0x00,        // call_indirect (param f64 i32) (result f32)
  0x1a,                    // drop
  0x05,                    // else
  0x20, 0x01,              // local.get $var1
  0x45,                    // i32.eqz
  0x0d, 0x03,              // br_if $label8
  0x0b,                    // end
  0x20, 0x02,              // local.get $var2
  0x22, 0x01,              // local.tee $var1
  0x0b,                    // end $label9
  0x05,                    // else
  0x41, 0x81, 0x01,        // i32.const 129
  0x22, 0x01,              // local.tee $var1
  0x22, 0x00,              // local.tee $var0
  0x0b,                    // end
  0x45,                    // i32.eqz
  0x0d, 0x02,              // br_if $label10
  0x0b,                    // end $label8
  0x20, 0x01,              // local.get $var1
  0x41, 0x0f,              // i32.const 15
  0x71,                    // i32.and
  0x41, 0x80, 0x80, 0x20,  // i32.const 524288
  0x22, 0x01,              // local.tee $var1
  0xfe, 0x00, 0x02, 0x00,  // memory.atomic.notify
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label11
  0x41, 0x80, 0x80, 0x20,  // i32.const 524288
  0x41, 0x81, 0x80, 0x7c,  // i32.const -65535
  0x20, 0x00,              // local.get $var0
  0x1b,                    // select
  0x0b,                    // end $label11
  0x22, 0x00,              // local.tee $var0
  0x0d, 0x00,              // br_if $label10
  0x41, 0x04,              // i32.const 4
  0x42, 0xd9, 0xd0, 0xb0, 0x7f,  // i64.const -1300391
  0xfe, 0x18, 0x03, 0x00,  // i64.atomic.store
  0x0c, 0x00,              // br $label10
  0x0b,                    // end $label10
  0x00,                    // unreachable
  0x0b,                    // end $label1
  0x03, 0x7f,              // loop (result i32) $label12
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x54, 0x40,  // f64.const 82
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0xb1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffffb1
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x02,              // local.get $var2
  0x0d, 0x00,              // br_if $label12
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xc3,  // f64.const -9007199254740992
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x00,              // local.get $var0
  0x45,                    // i32.eqz
  0x0d, 0x00,              // br_if $label12
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x40,  // f64.const 128
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x03, 0x7c,              // loop (result f64) $label13
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xb8,  // f64.const -1.1754943508222875e-38
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x03,              // local.get $var3
  0x0d, 0x00,              // br_if $label13
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xc3,  // f64.const -9223372036854776000
  0x0b,                    // end $label13
  0x20, 0x00,              // local.get $var0
  0x0d, 0x02,              // br_if $label14
  0x1a,                    // drop
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x42,  // f64.const 35184372088832
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x20, 0x01,              // local.get $var1
  0x0d, 0x00,              // br_if $label12
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xbf,  // f64.const -1
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x41, 0x80, 0x7e,        // i32.const -256
  0x0b,                    // end $label12
  0x0d, 0x00,              // br_if $label3
  0x0b,                    // end $label3
  0x23, 0x00,              // global.get $global0
  0x45,                    // i32.eqz
  0x04, 0x40,              // if
  0x44, 0x88, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // f64.const -nan:0xfffffffffff88
  0x0f,                    // return
  0x0b,                    // end
  0x23, 0x00,              // global.get $global0
  0x41, 0x01,              // i32.const 1
  0x6b,                    // i32.sub
  0x24, 0x00,              // global.set $global0
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,  // f64.const -2
  0x0b,                    // end $label14
  0x0b,                    // end
                           // function #21 $hangLimitInitializer
  0x06,                    // body size 6
  0x00,                    // 0 entries in locals list
  0x41, 0x0a,              // i32.const 10
  0x24, 0x00,              // global.set $global0
  0x0b,                    // end

  0x0b,                    // section kind: Data
  0x0f,                    // section length 15
  0x01, 0x00,              // data segments count 1: flag: active no index
  0x41, 0x00, 0x0b,        // i32.const 0
  0x09,                    // source size 9
  0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x26,                    // segment data
]));
