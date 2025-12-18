// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --wasm-deopt --wasm-inlining-ignore-call-counts --allow-natives-syntax
// Flags: --liftoff --wasm-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const moduleBuilder = new WasmModuleBuilder();
const typeII2I = moduleBuilder.addType(kSig_i_ii);
const funcAdd = moduleBuilder.addFunction("add", typeII2I)
  .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
  .exportFunc();
const tableIndex = moduleBuilder.addTable(kWasmFuncRef, 2).index;
moduleBuilder.addActiveElementSegment(tableIndex,
  wasmI32Const(0),
  [[kExprRefFunc, funcAdd.index]],
  kWasmFuncRef);
const typeFuncIRef2I = makeSig(
  [kWasmI32, wasmRefType(typeII2I)],
  [kWasmI32]);
moduleBuilder.addFunction("callRef", typeFuncIRef2I)
  // The sum of the locals in callRef and callIndirect must be > 40000+25524 = 65524 = 0xFFF4
  .addLocals(kWasmI64, 40000)
  .addBody([
    kExprI32Const, 12,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallRef, typeII2I
  ])
  .exportFunc();
moduleBuilder.addFunction("callRef2", typeII2I)
  .addLocals(kWasmI64, 25525)
  .addBody([
    kExprI32Const, 13,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprTableGet, kTableZero,
    kGCPrefix, kExprRefCast, typeII2I,
    kExprCallRef, typeII2I,
  ])
  .exportFunc();
const instanceExports = moduleBuilder.instantiate().exports;
instanceExports.callRef(instanceExports.add, instanceExports.callRef2);
%WasmTierUpFunction(instanceExports.callRef);
