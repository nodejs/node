// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.startRecGroup();
let $struct1 = builder.nextTypeIndex() + 1;
let $struct0 = builder.addStruct({fields: [], descriptor: $struct1});
/* $struct1 */ builder.addStruct({fields: [], describes: $struct0});
let $sig2 = builder.addType(makeSig([], [wasmRefType($struct1).exact()]));
builder.endRecGroup();

builder.addFunction("none", $sig2).addBody([
  kExprRefNull, kNullRefCode,
  kGCPrefix, kExprRefGetDesc, $struct0,
]);

builder.addFunction("bottom", $sig2).addBody([
  kExprUnreachable,
  kGCPrefix, kExprRefGetDesc, $struct0,
]);

builder.addFunction("non-null-none", $sig2).addBody([
  kExprRefNull, kNullRefCode,
  kExprRefAsNonNull,
  kGCPrefix, kExprRefGetDesc, $struct0,
]);

// Just to show that we don't have a similar problem with nullability of bottom.
builder.addFunction(
    "convert_extern", makeSig([], [wasmRefType(kWasmAnyRef)])).addBody([
  kExprUnreachable,
  kGCPrefix, kExprAnyConvertExtern,
]);

builder.addFunction(
    "convert_any", makeSig([], [wasmRefType(kWasmExternRef)])).addBody([
  kExprUnreachable,
  kGCPrefix, kExprExternConvertAny,
]);

// Does not fail to validate.
builder.instantiate();
