// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  const arr = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d,  // wasm magic
    0x01, 0x00, 0x00, 0x00,  // wasm version

    0x01,                    // section kind: Type
    0x0b,                    // section length 11
    0x02,                    // types count 2
    0x60,                    //  kind: func
    0x02,                    // param count 2
    0x7e, 0x7c,              // i64 f64
    0x01, 0x7e,              // return count 1:  i64
    0x60,                    //  kind: func
    0x00,                    // param count 0
    0x01, 0x7c,              // return count 1:  f64

    0x03,                    // section kind: Function
    0x02,                    // section length 2
    0x01, 0x00,              // functions count 1: 0 $func0 (param i64 f64) (result i64)

    0x04,                    // section kind: Table
    0x05,                    // section length 5
    0x01, 0x70, 0x01,        // table count 1:  funcref with maximum
    0x06,                    // initial size 6
    0x06,                    // maximum size 6

    0x0a,                    // section kind: Code
    0x72,                    // section length 114
    0x01,                    // functions count 1
                             // function #0 $func0
    0x70,                    // body size 112
    0x04,                    // 4 entries in locals list
    0xf4, 0x02, 0x7f,        // 372 locals of type i32
    0x76, 0x7e,              // 118 locals of type i64
    0x1f, 0x7d,              // 31 locals of type f32
    0x01, 0x7c,              // 1 local of type f64
    0x02, 0x7e,              // block (result i64) $label4
    0x02, 0x40,              // block $label2
    0x02, 0x7f,              // block (result i32) $label0
    0x03, 0x40,              // loop $label3
    0x02, 0x7c,              // block (result f64)
    0x03, 0x7f,              // loop (result i32) $label1
    0x41, 0x00,              // i32.const 0
    0x41, 0x00,              // i32.const 0
    0x0d, 0x03,              // br_if $label0
    0x21, 0x8e, 0x01,        // local.set $var142
    0x41, 0x00,              // i32.const 0
    0x0d, 0x00,              // br_if $label1
    0x41, 0x00,              // i32.const 0
    0x0b,                    // end $label1
    0x41, 0x00,              // i32.const 0
    0x0d, 0x02,              // br_if $label0
    0x22, 0x8f, 0x01,        // local.tee $var143
    0x22, 0xf1, 0x02,        // local.tee $var369
    0x22, 0xbf, 0x02,        // local.tee $var319
    0x22, 0xf2, 0x02,        // local.tee $var370
    0x22, 0xd0, 0x02,        // local.tee $var336
    0x21, 0x90, 0x01,        // local.set $var144
    0x02, 0x7f,              // block (result i32)
    0x41, 0x00,              // i32.const 0
    0x04, 0x40,              // if
    0x41, 0x00,              // i32.const 0
    0x0d, 0x05,              // br_if $label2
    0x0b,                    // end
    0x41, 0x00,              // i32.const 0
    0x0b,                    // end
    0x21, 0x91, 0x01,        // local.set $var145
    0x41, 0x00,              // i32.const 0
    0x11, 0x01, 0x00,        // call_indirect (result f64)
    0x0b,                    // end
    0x21, 0x8b, 0x04,        // local.set $var523
    0x41, 0x00,              // i32.const 0
    0x0d, 0x00,              // br_if $label3
    0x0b,                    // end $label3
    0x0c, 0x01,              // br $label2
    0x0b,                    // end $label0
    0x0d, 0x00,              // br_if $label2
    0x0b,                    // end $label2
    0x42, 0x80, 0x7f,        // i64.const -128
    0x22, 0xbd, 0x03,        // local.tee $var445
    0x41, 0x00,              // i32.const 0
    0x0d, 0x00,              // br_if $label4
    0x22, 0xc7, 0x03,        // local.tee $var455
    0x0b,                    // end $label4
    0x0b,                    // end
  ]);
  WebAssembly.instantiate(arr);

  function g() {
    Realm.createAllowCrossRealmAccess();
    return this;
  }

  Object.defineProperty(this.d8.__proto__, "then", {"get": g});
  return "then";
}

const worker = new Worker(f, {"type": "function"});
worker.getMessage();
