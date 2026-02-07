// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.startRecGroup();
let $desc = builder.nextTypeIndex() + 1;
let $struct = builder.addStruct({descriptor: $desc});
/* $desc */ builder.addStruct({describes: $struct});
let $otherdesc = builder.nextTypeIndex() + 1;
let $other = builder.addStruct({descriptor: $otherdesc});
/* $otherdesc */ builder.addStruct({describes: $other});
builder.endRecGroup();
let $successblocktype = builder.addType(makeSig([], [wasmRefNullType($other)]));
let $failblocktype = builder.addType(
    makeSig([], [wasmRefNullType($struct).exact()]));

builder.addFunction("succeed", kSig_v_v).exportFunc()
  .addLocals(wasmRefNullType($otherdesc), 1)
  .addBody([
    kExprBlock, $successblocktype,
      kGCPrefix, kExprStructNewDefault, $desc,
      kGCPrefix, kExprStructNewDefaultDesc, $struct,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCastDesc, 0b00, 0, kWasmExact, $struct, $other,
      kExprDrop,
      kExprRefNull, kNullRefCode,
    kExprEnd,  // block
    kExprDrop,
  ]);

builder.addFunction("fail", kSig_v_v).exportFunc()
  .addLocals(wasmRefNullType($otherdesc), 1)
  .addBody([
    kExprBlock, $failblocktype,
      kGCPrefix, kExprStructNewDefault, $desc,
      kGCPrefix, kExprStructNewDefaultDesc, $struct,
      kExprLocalGet, 0,
      kGCPrefix, kExprBrOnCastDescFail, 0b00, 0, kWasmExact, $struct, $other,
      kExprDrop,
      kExprRefNull, kNullRefCode,
    kExprEnd,  // block
    kExprDrop,
  ]);

let instance = builder.instantiate();
assertThrows(() => instance.exports.succeed(), WebAssembly.RuntimeError,
             "dereferencing a null pointer");
assertThrows(() => instance.exports.fail(), WebAssembly.RuntimeError,
             "dereferencing a null pointer");
