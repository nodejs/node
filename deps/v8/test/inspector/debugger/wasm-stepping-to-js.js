// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start('Tests stepping to javascript from wasm');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

// wasm_A
let func = builder.addFunction('wasm_A', kSig_v_v)
    .addBody([
      // clang-format off
      kExprNop,   // Line 1
      // clang-format on
    ])
    .exportAs('main');

let module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log("paused");
  var frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  let action = step_actions.shift() || 'resume';
  InspectorTest.log('Debugger.' + action)
  await Protocol.Debugger[action]();
})

let step_actions = [
  // start of run 1
  'resume',    // move to breakpoint
  // then get back to Javascript.
  'stepOut',
  'resume',
  // end of run 1

  // start of run 2
  'resume',    // move to breakpoint
  // then get back to Javascript.
  'stepOver',
  'resume',
  // end of run 2

  // start of run 3
  'resume',    // move to breakpoint
  // then get back to Javascript.
  'stepInto',
  'resume',
  // end of run 3
];

contextGroup.addScript(`
function test() {
  debugger;
  instance.exports.main();
  var x = 1;
  x++;
}
//# sourceURL=test.js`);

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Calling instantiate function.');
    WasmInspectorTest.instantiate(module_bytes);
    const scriptId = await waitForWasmScript();
    InspectorTest.log(
        'Setting breakpoint at start of wasm function');
    let msg = await Protocol.Debugger.setBreakpoint(
        {'location': {'scriptId': scriptId, 'lineNumber': 0, 'columnNumber': func.body_offset}});
    printFailure(msg);
    InspectorTest.logMessage(msg.result.actualLocation);

    for (var i=1; i<=3; i++) {
      InspectorTest.log('Start run '+ i);
      await Protocol.Runtime.evaluate({ expression: 'test()' });
      InspectorTest.log('exports.main returned!');
      InspectorTest.log('Finished run '+ i +'!\n');
    }
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
