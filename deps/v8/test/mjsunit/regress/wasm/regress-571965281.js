// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

send(JSON.stringify({id: 1, method: 'Debugger.enable'}));
send(JSON.stringify({
  id: 2,
  method: 'Debugger.setPauseOnExceptions',
  params: {state: 'all'},
}));

const builder = new WasmModuleBuilder();
const array = builder.addArray(kWasmI32);
const segment = builder.addPassiveDataSegment([]);

builder.addFunction('array_new_data', kSig_v_v)
    .addBody([
      kExprI32Const, 0,
      kExprI32Const, 1,
      kGCPrefix, kExprArrayNewData, array, segment,
      kExprDrop,
    ])
    .exportFunc();

builder.addFunction('array_init_data', kSig_v_v)
    .addBody([
      kExprRefNull, array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprArrayInitData, array, segment,
    ])
    .exportFunc();

builder.addFunction('array_copy', kSig_v_v)
    .addBody([
      kExprRefNull, array,
      kExprI32Const, 0,
      kExprRefNull, array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprArrayCopy, array, array,
    ])
    .exportFunc();

builder.addFunction('throw_ref', kSig_v_v)
    .addBody([
      kExprRefNull, kExnRefCode,
      kExprThrowRef,
    ])
    .exportFunc();

const instance = builder.instantiate();
assertTraps(kTrapDataSegmentOutOfBounds, instance.exports.array_new_data);
assertTraps(kTrapNullDereference, instance.exports.array_init_data);
assertTraps(kTrapNullDereference, instance.exports.array_copy);
assertTraps(kTrapRethrowNull, instance.exports.throw_ref);
