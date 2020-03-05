// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests remove breakpoint from wasm scripts.');
session.setupScriptMap();

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();

// wasm_A
let func = builder.addFunction('wasm_A', kSig_i_i)
    .addBody([
      // clang-format off
      kExprLocalGet, 0,              // Line 1: get input
      kExprI32Const, 1,              // Line 2: get constant 1
      kExprI32Sub                    // Line 3: decrease
      // clang-format on
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

let breakCount = 0;
let breakpointId = 0;

Protocol.Debugger.onPaused(async message => {
  breakCount++;
  InspectorTest.log("paused No " + breakCount);
  var frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  if (breakCount == 1) {
    InspectorTest.log('Remove breakpoint with breakpointId: ' + breakpointId);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
  }
  let action= 'resume';
  InspectorTest.log('Debugger.' + action)
  await Protocol.Debugger[action]();
})

contextGroup.addScript(`
function test() {
  instance.exports.main(1);
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
  InspectorTest.log(
      'Setting breakpoint on line 3 of wasm function');
  let msg = await Protocol.Debugger.setBreakpoint(
      {'location': {'scriptId': scriptId, 'lineNumber': 0, 'columnNumber': func.body_offset + 4}});
  printFailure(msg);
  InspectorTest.logMessage(msg.result.actualLocation);
  InspectorTest.logMessage('BreakpointId: '+ msg.result.breakpointId);
  breakpointId = msg.result.breakpointId;
  await Protocol.Runtime.evaluate({ expression: 'test()' });
  await Protocol.Runtime.evaluate({ expression: 'test()' });
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
