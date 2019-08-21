// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --experimental-wasm-threads

load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

builder.addMemory(1, 1, /*exp*/ false, /*shared*/ true);

builder.addFunction('test', kSig_v_v).addBody([
  kExprI32Const, 0,                         //
  kExprI64Const, 0,                         //
  kExprI64Const, 0,                         //
  kAtomicPrefix, kExprI64AtomicWait, 3, 0,  //
  kExprDrop,                                //
]);

builder.instantiate();
