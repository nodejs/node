// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Lower the maximum code space size to detect missed garbage collection
// earlier.
// Flags: --wasm-max-code-space=2

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests repeated stepping through a large function (should not OOM)');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

const body = [kExprLocalGet, 0];
// Stepping through a long function will repeatedly recreate stepping code, with
// corresponding side tables. This should not run OOM
// (https://crbug.com/1168564).
// We use calls such that stack checks are executed reliably.
for (let i = 0; i < 500; ++i) body.push(...wasmI32Const(i), kExprI32Add);
const func_test =
    builder.addFunction('test', kSig_i_i).addBody(body).exportFunc();
const module_bytes = builder.toArray();

let paused = 0;
Protocol.Debugger.onPaused(msg => {
  ++paused;
  if (paused % 50 == 0) InspectorTest.log(`Paused ${paused} and running...`);
  Protocol.Debugger.stepOver();
});

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Setting up global instance variable.');
    WasmInspectorTest.instantiate(module_bytes);
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);

    InspectorTest.log('Got wasm script: ' + wasmScript.url);

    InspectorTest.log('Setting breakpoint');
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId: wasmScript.scriptId,
        lineNumber: 0,
        columnNumber: func_test.body_offset
      }
    });

    await Protocol.Runtime.evaluate({ expression: 'instance.exports.test()' });
    InspectorTest.log('test function returned.');
    InspectorTest.log(`Paused ${paused} times.`);
  }
]);
