// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/571965281.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

Protocol.Debugger.onPaused(message => {
  InspectorTest.log(
      `Paused on ${message.params.reason}: ${message.params.data.description.split('\n')[0]}`);
  Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  contextGroup.addScript(`
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
`);

  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.array_new_data()',
  });
  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.array_init_data()',
  });
  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.array_copy()',
  });
  await Protocol.Runtime.evaluate({
    expression: 'instance.exports.throw_ref()',
  });
  InspectorTest.completeTest();
})();
