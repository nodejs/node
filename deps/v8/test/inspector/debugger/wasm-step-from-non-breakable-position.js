// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Step into a function that starts with a non-breakable opcode (i.e. ' +
    'block), then step from there. See https://crbug.com/1137710.');
session.setupScriptMap();

var builder = new WasmModuleBuilder();

var callee = builder.addFunction('callee', kSig_v_v)
                 .addBody([kExprBlock, kWasmStmt, kExprEnd])
                 .index;

var main = builder.addFunction('main', kSig_v_i)
               .addBody([kExprCallFunction, callee])
               .exportFunc();

var module_bytes = builder.toArray();

InspectorTest.runAsyncTestSuite([
  async function test() {
    InspectorTest.logProtocolCommandCalls('Debugger.stepInto');
    InspectorTest.logProtocolCommandCalls('Debugger.resume');

    await Protocol.Debugger.enable();
    InspectorTest.log('Setting up global instance variable.');
    WasmInspectorTest.instantiate(module_bytes);
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);

    InspectorTest.log(`Got wasm script: ${wasmScript.url}`);

    // Set a breakpoint in 'main', at the call.
    InspectorTest.log(`Setting breakpoint on offset ${main.body_offset}`);
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId: wasmScript.scriptId,
        lineNumber: 0,
        columnNumber: main.body_offset
      }
    });

    InspectorTest.log('Running main function.');
    Protocol.Runtime.evaluate({ expression: 'instance.exports.main()' });
    for (let action of ['stepInto', 'stepInto', 'resume']) {
      const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
      await session.logSourceLocation(callFrames[0].location);
      Protocol.Debugger[action]();
    }
    InspectorTest.log('exports.main returned.');
  }
]);
