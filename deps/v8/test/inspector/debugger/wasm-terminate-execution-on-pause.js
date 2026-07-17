// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests termination on pause in Wasm');
session.setupScriptMap();

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(
    message => InspectorTest.logMessage(message.params.args[0]));
Protocol.Debugger.enable();

const builder = new WasmModuleBuilder();
const func = builder.addFunction('func', kSig_v_v)
                 .addBody([kExprNop])
                 .exportFunc();
const module_bytes = builder.toArray();

InspectorTest.runAsyncTestSuite([
  async function testTerminateOnBreakpoint() {
    InspectorTest.log('Instantiating.');
    // Spawn asynchronously:
    WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log(
        'Waiting for wasm script (ignoring first non-wasm script).');
    const [, {params: wasm_script}] = await Protocol.Debugger.onceScriptParsed(2);
    InspectorTest.log('Setting breakpoint.');
    InspectorTest.logMessage(await Protocol.Debugger.setBreakpoint({
      'location': {
        'scriptId': wasm_script.scriptId,
        'lineNumber': 0,
        'columnNumber': func.body_offset
      }
    }));
    InspectorTest.log('Calling wasm export.');
    const evaluation = Protocol.Runtime.evaluate({
      'expression': `console.log('Before Wasm execution');` +
          `instance.exports.func();` +
          `console.log('After Wasm execution (should not be reached)');`
    });
    const pause_msg = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Paused:');
    await session.logSourceLocation(pause_msg.params.callFrames[0].location);
    const terminated = Protocol.Runtime.terminateExecution();
    await Protocol.Debugger.resume();
    await terminated;
    await evaluation;
  },
]);
