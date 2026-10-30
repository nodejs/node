// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/1032042.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

(async function test() {
  await Protocol.Debugger.enable();

  // Record the ID of the first Wasm script reported.
  let scriptId;
  Protocol.Debugger.onScriptParsed(({params}) => {
    if (scriptId === undefined && params.url.startsWith('wasm://')) {
      scriptId = params.scriptId;
    }
  });

  // Create a simple Wasm script, which will be caught by the event listener.
  await Protocol.Runtime.evaluate({
    expression: `
      const builder = new WasmModuleBuilder();
      builder.addFunction('sub', kSig_i_ii)
      // input is 2 args of type int and output is int
      .addBody([
        kExprLocalGet, 0, // local.get i0
        kExprLocalGet, 1, // local.get i1
        kExprI32Sub]) // i32.sub i0 i1
      .exportFunc();
      const instance = builder.instantiate();
    `,
  });

  // By now we should have recorded the ID of the Wasm script above.
  InspectorTest.log(`Recorded Wasm scriptId: ${scriptId !== undefined}`);

  // Disable and re-enable the Debugger and collect the reported
  // script IDs.
  const scriptIds = new Set();
  await Protocol.Debugger.disable();
  Protocol.Debugger.onScriptParsed(({params}) => {
    scriptIds.add(params.scriptId);
  });
  await Protocol.Debugger.enable();

  // Make sure the Wasm script was reported.
  InspectorTest.log(
      `Wasm script reported on re-enable: ${scriptIds.has(scriptId)}`);
  InspectorTest.completeTest();
})();
