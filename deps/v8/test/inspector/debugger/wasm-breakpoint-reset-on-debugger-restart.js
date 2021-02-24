// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that breakpoints do not survive a restart of the debugger.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();
const func =
    builder.addFunction('func', kSig_v_v).addBody([kExprNop]).exportFunc();
const module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async msg => {
  await session.logSourceLocation(msg.params.callFrames[0].location);
  Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiating.');
    // Spawn asynchronously:
    WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log(
        'Waiting for wasm script (ignoring first non-wasm script).');
    const [, {params: wasm_script}] = await Protocol.Debugger.onceScriptParsed(2);
    InspectorTest.log('Setting breakpoint.');
    await Protocol.Debugger.setBreakpoint({
      'location': {
        'scriptId': wasm_script.scriptId,
        'lineNumber': 0,
        'columnNumber': func.body_offset
      }
    });
    for (let run of [0, 1]) {
      InspectorTest.log('Calling func.');
      await Protocol.Runtime.evaluate({'expression': 'instance.exports.func()'});
      InspectorTest.log('func returned.');
      if (run == 1) continue;
      InspectorTest.log('Restarting debugger.');
      await Protocol.Debugger.disable();
      await Protocol.Debugger.enable();
    }
  }
]);
