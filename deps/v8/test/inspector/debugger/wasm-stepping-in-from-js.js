// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping from javascript into wasm');
session.setupScriptMap();

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

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

function instantiate(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  let module = new WebAssembly.Module(buffer);
  // Set global variable.
  instance = new WebAssembly.Instance(module);
}

let evalWithUrl = (code, url) => Protocol.Runtime.evaluate(
    {'expression': code + '\n//# sourceURL=v8://test/' + url});

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
}
//# sourceURL=test.js`);

(async function Test() {
  await Protocol.Debugger.enable();
  InspectorTest.log('Installing code and global variable.');
  await evalWithUrl('var instance;\n' + instantiate.toString(), 'setup');
  InspectorTest.log('Calling instantiate function.');
  evalWithUrl(
      'instantiate(' + JSON.stringify(module_bytes) + ')', 'callInstantiate');
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
  InspectorTest.log('Finished!');
  InspectorTest.completeTest();
})();

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
