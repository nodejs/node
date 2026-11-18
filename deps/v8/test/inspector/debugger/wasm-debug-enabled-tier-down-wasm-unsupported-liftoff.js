// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-testing-opcode-in-wasm --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that tiering up and tiering down works even if functions cannot be ' +
    'compiled with Liftoff.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

const source = `
function create_builder(i) {
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_i_i)
      .addBody([
        kExprLocalGet, 0, kExprNopForTestingUnsupportedInLiftoff,
        ...wasmI32Const(i), kExprI32Add
      ])
      .exportFunc();
  return builder;
}

function checkNotLiftoff(instance) {
  if (%IsLiftoffFunction(instance.exports.main)) {
    throw new Error('Expected function to not be compiled with Liftoff');
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  // Sync.
  InspectorTest.log('Test sync compilation');
  await Protocol.Runtime.evaluate({
    expression: 'var instance = create_builder(0).instantiate();',
  });

  // Test recompilation.
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({
    expression: 'checkNotLiftoff(instance);',
  });
  await Protocol.Runtime.evaluate({
    expression: 'var newInstance = create_builder(1).instantiate();',
  });
  await Protocol.Runtime.evaluate({
    expression: 'checkNotLiftoff(newInstance);',
  });

  // Async.
  InspectorTest.log('Test async compilation');
  await Protocol.Debugger.disable();

  await Protocol.Runtime.evaluate({
    expression:
        'var asyncInstancePromise = create_builder(2).asyncInstantiate();',
  });
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate({
    expression:
        'var asyncInstance; asyncInstancePromise.then(i => asyncInstance = i);',
  });
  await InspectorTest.waitForPendingTasks();

  // Test recompilation.
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate({
    expression: 'checkNotLiftoff(asyncInstance);',
  });

  await Protocol.Runtime.evaluate({
    expression:
        'var newAsyncInstancePromise = create_builder(3).asyncInstantiate();',
  });
  await InspectorTest.waitForPendingTasks();
  await Protocol.Runtime.evaluate({
    expression:
        'var newAsyncInstance; newAsyncInstancePromise.then(i => newAsyncInstance = i);',
  });
  await InspectorTest.waitForPendingTasks();

  await Protocol.Runtime.evaluate({
    expression: 'checkNotLiftoff(newAsyncInstance);',
  });

  InspectorTest.log('Finished');
  InspectorTest.completeTest();
})();
