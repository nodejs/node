// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_iii).addBody([
  kExprI32Const, 0x41,  // i32.const 0x41
  kExprLoop, 0x7c,      // loop f64
  kExprLocalGet, 0x00,  // get_local 0
  kExprLocalGet, 0x01,  // get_local 1
  kExprBrIf, 0x01,      // br_if depth=1
  kExprLocalGet, 0x00,  // get_local 0
  kExprI32Rol,          // i32.rol
  kExprBrIf, 0x00,      // br_if depth=0
  kExprUnreachable,     // unreachable
  kExprEnd,             // end
  kExprUnreachable,     // unreachable
]);
builder.instantiate();
