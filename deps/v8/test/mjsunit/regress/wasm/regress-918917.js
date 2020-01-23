// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_v_v)
  .addLocals({i32_count: 1}).addLocals({f32_count: 1}).addLocals({f64_count: 1})
  .addBody([
kExprLocalGet, 1,
kExprLocalGet, 2,
kExprLocalGet, 0,
kExprIf, kWasmI32,
  kExprI32Const, 1,
kExprElse,
  kExprUnreachable,
  kExprEnd,
kExprUnreachable
]);
builder.instantiate();
