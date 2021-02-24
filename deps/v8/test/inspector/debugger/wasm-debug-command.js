// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start('Tests debug command for wasm');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

// wasm_A
builder.addFunction('wasm_A', kSig_i_i)
    .addBody([
      // clang-format off
      kExprLocalGet, 0,              // Line 1: get input
      kExprI32Const, 1,              // Line 2: get constant 1
      kExprI32Sub                    // Line 3: decrease
      // clang-format on
    ])
    .exportAs('main');

let module_bytes = builder.toArray();

let breakCount;

Protocol.Debugger.onPaused(async message => {
  breakCount++;
  InspectorTest.log("paused No " + breakCount);
  var frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  let action = 'resume';
  InspectorTest.log('Debugger.' + action)
  await Protocol.Debugger[action]();
})

let breakpointId;

contextGroup.addScript(`
function test() {
  debug(instance.exports.main);
  instance.exports.main(3, 2);
}
//# sourceURL=test.js`);

InspectorTest.runAsyncTestSuite([
  async function test() {
    breakCount = 0;
    breakpointId = 0;
    await Protocol.Debugger.enable();
    WasmInspectorTest.instantiate(module_bytes);
    await waitForWasmScript();
    await Protocol.Runtime.evaluate({ expression: 'test()', includeCommandLineAPI: true});
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
