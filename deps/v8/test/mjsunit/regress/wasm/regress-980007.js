// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction(undefined, kSig_i_i).addBody([
  kExprI64Const, 0x01,
  kExprI32ConvertI64,
  kExprI32Const, 0x80, 0x80, 0x80, 0x80, 0x78,
  kExprI32Sub,
]);
builder.instantiate();
