// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping from javascript into wasm');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

// wasm_A
let func = builder.addFunction('wasm_A', kSig_i_i)
               .addBody([
                 kExprLocalGet, 0,  // push param 0
                 kExprI32Const, 1,  // push constant 1
                 kExprI32Sub        // subtract
               ])
               .exportAs('main');

let module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log('paused');
  var frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  let action = step_actions.shift() || 'resume';
  InspectorTest.log('Debugger.' + action)
  await Protocol.Debugger[action]();
})

let step_actions = [
  'stepInto',  // # debugger
  'stepInto',  // step into instance.exports.main(1)
  'resume',    // move to breakpoint
  'resume',    // then just resume.
];

contextGroup.addScript(`
function test() {
  debugger;
  instance.exports.main(1);
}`, 0, 0, 'test.js');

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Calling instantiate function.');
    WasmInspectorTest.instantiate(module_bytes);
    const scriptId = await waitForWasmScript();
    InspectorTest.log('Setting breakpoint on i32.const');
    let msg = await Protocol.Debugger.setBreakpoint({
      'location': {
        'scriptId': scriptId,
        'lineNumber': 0,
        'columnNumber': 2 + func.body_offset
      }
    });
    printFailure(msg);
    InspectorTest.logMessage(msg.result.actualLocation);
    await Protocol.Runtime.evaluate({expression: 'test()'});
    InspectorTest.log('exports.main returned!');
  }
]);

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

async function waitForWasmScript() {
  InspectorTest.log('Waiting for wasm scripts to be parsed.');
  while (true) {
    let msg = await Protocol.Debugger.onceScriptParsed();
    let url = msg.params.url;
    if (!url.startsWith('wasm://')) {
      InspectorTest.log('Ignoring script with url ' + url);
      continue;
    }
    let scriptId = msg.params.scriptId;
    InspectorTest.log('Got wasm script: ' + url);
    return scriptId;
  }
}
