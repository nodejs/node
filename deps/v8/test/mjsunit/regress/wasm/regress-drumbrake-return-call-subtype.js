// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const functionType = builder.addType(kSig_v_v);
const calleeType =
    builder.addType(makeSig([], [wasmRefNullType(functionType)]));
const callerType = builder.addType(makeSig([], [kWasmFuncRef]));

// A tail callee may return a subtype of its caller's declared result type.
const callee = builder.addFunction('callee', calleeType)
                   .addBody([kExprRefNull, functionType]);
builder.addFunction('direct', callerType)
    .addBody([kExprReturnCall, callee.index])
    .exportFunc();
builder.addFunction('indirect', callerType)
    .addBody([
      kExprI32Const, 0,
      kExprReturnCallIndirect, calleeType, kTableZero,
    ])
    .exportFunc();
builder.addFunction('ref', callerType)
    .addBody([
      kExprRefFunc, callee.index,
      kExprReturnCallRef, calleeType,
    ])
    .exportFunc();
builder.appendToTable([callee.index]);

const instance = builder.instantiate();
assertEquals(null, instance.exports.direct());
assertEquals(null, instance.exports.indirect());
assertEquals(null, instance.exports.ref());
