// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-code-coverage --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test wasm code coverage and frame inspection on break.');

(async function test() {
  await Protocol.Debugger.enable();

  const script = `
    ${utils.read('test/mjsunit/wasm/wasm-module-builder.js')}

    const builder = new WasmModuleBuilder();
    builder.addImport('mod', 'func', kSig_v_v);
    builder.addFunction('w\\u00e9sm', kSig_v_v)
        .addBody([kExprCallFunction, 0])
        .exportAs('main');

    function call_debugger() {
      debugger;
    }

    const instance = builder.instantiate({mod: {func: call_debugger}});
    function run() {
      instance.exports.main();
    }
  `;

  contextGroup.addScript(script, 0, 0, 'test.js');

  Protocol.Runtime.evaluate({expression: 'run();'});
  let message = await Protocol.Debugger.oncePaused();

  let names = [];
  for (let frame of message.params.callFrames) {
    names.push(frame.functionName);
  }
  InspectorTest.log(`has wésm: ${names.includes('$w\u00e9sm')}`);

  await Protocol.Debugger.resume();

  let {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: '(%DebugCollectWasmCoverage()).length > 0'});
  InspectorTest.log(`coverage collected: ${result.value}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
