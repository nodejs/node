// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests tiering down wasm code when debugger is enabled');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

const source = `
const num_functions = 200;

// Create a simple Wasm module.
function create_builder(delta = 0) {
  const builder = new WasmModuleBuilder();
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i + delta))
        .exportFunc();
  }
  return builder;
}

function checkDebugCode(instance) {
  for (let i = 0; i < num_functions; ++i) {
    // Call the function once because of lazy compilation.
    instance.exports['f' + i]();
    if (!%IsWasmDebugFunction(instance.exports['f' + i])) {
      throw new Error('Expected debug code for f' + i);
    }
  }
}

function hasDebugCode(instance) {
  for (let i = 0; i < num_functions; ++i) {
    if (%IsWasmDebugFunction(instance.exports['f' + i])) {
      return true;
    }
  }
  return false;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  async function waitForNoDebugCode(instanceVar) {
    while (true) {
      const {result: {result}} = await Protocol.Runtime.evaluate({
        expression: `hasDebugCode(${instanceVar})`,
      });
      if (!result.value) return;
      await InspectorTest.waitForPendingTasks();
    }
  }

  // testEnterDebugMode
  InspectorTest.log('Testing sync instantiation and debug mode');
  await Protocol.Runtime.evaluate({
    expression: 'var instance = create_builder(0).instantiate();',
  });
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({expression: 'checkDebugCode(instance);'});
  await Protocol.Runtime.evaluate({
    expression: 'var instance2 = create_builder(1).instantiate();',
  });
  await Protocol.Runtime.evaluate({expression: 'checkDebugCode(instance2);'});
  await Protocol.Debugger.disable();
  await waitForNoDebugCode('instance');
  await waitForNoDebugCode('instance2');

  // testEnterDebugModeAsync
  // First test: enable the debugger *after* compiling the module.
  InspectorTest.log(
      'Testing async instantiation: enable debugger after compiling');
  await Protocol.Runtime.evaluate({
    expression: 'var instancePromise1 = create_builder(2).asyncInstantiate();',
  });
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate({
    expression:
        'var instanceAsync1; instancePromise1.then(i => instanceAsync1 = i);',
  });
  await InspectorTest.waitForPendingTasks();

  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate(
      {expression: 'checkDebugCode(instanceAsync1);'});

  await Protocol.Runtime.evaluate({
    expression: 'var instancePromise2 = create_builder(3).asyncInstantiate();',
  });
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate({
    expression:
        'var instanceAsync2; instancePromise2.then(i => instanceAsync2 = i);',
  });
  await InspectorTest.waitForPendingTasks();

  await Protocol.Runtime.evaluate(
      {expression: 'checkDebugCode(instanceAsync2);'});
  await Protocol.Debugger.disable();
  await waitForNoDebugCode('instanceAsync1');
  await waitForNoDebugCode('instanceAsync2');

  // Second test: enable the debugger *while* compiling the module.
  InspectorTest.log(
      'Testing async instantiation: enable debugger while compiling');
  await Protocol.Runtime.evaluate({
    expression: 'var instancePromise3 = create_builder(4).asyncInstantiate();',
  });
  await Protocol.Debugger.enable();
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate({
    expression:
        'var instanceAsync3; instancePromise3.then(i => instanceAsync3 = i);',
  });
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate(
      {expression: 'checkDebugCode(instanceAsync3);'});
  await Protocol.Debugger.disable();
  await waitForNoDebugCode('instanceAsync3');

  InspectorTest.log('Finished');
  InspectorTest.completeTest();
})();
