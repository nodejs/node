// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let table = builder.addTable(kWasmFuncRef, 3, 3);
let sigFinal = builder.addType(makeSig([], [kWasmI32]));
let sigNonFinal = builder.addType(makeSig([], [kWasmI32]), kNoSuperType, false);

builder.addFunction("callFinal", makeSig([kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  kExprCallIndirect, sigFinal, table.index,
]).exportFunc();

builder.addFunction("callNonFinal", makeSig([kWasmI32], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  kExprCallIndirect, sigNonFinal, table.index,
]).exportFunc();

assertTraps(kTrapNullFunc, builder.instantiate().exports.callFinal);
assertTraps(kTrapNullFunc, builder.instantiate().exports.callNonFinal);
