// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test scope inspection and stepping after a trap.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();

// Create a function which computes the div of the first two arguments.
builder.addFunction('div', kSig_i_iii, ['a', 'b', 'unused'])
    .addLocals(kWasmI32, 2, ['local_zero', 'local_const_11'])
    .addBody([
      kExprI32Const, 11,  // const 11
      kExprLocalSet, 4,   // set local #4 ('local_const_11')
      kExprLocalGet, 0,   // param 0
      kExprLocalGet, 1,   // param 1
      kExprI32DivS        // div
    ])
    .exportFunc();

const module_bytes = builder.toArray();

function getShortLocationString(location) {
  return `${location.lineNumber}:${location.columnNumber}`;
}

let actions = ['stepInto', 'resume', 'stepInto', 'resume'];
Protocol.Debugger.onPaused(async msg => {
  InspectorTest.log('Paused at:');
  for (let [nr, frame] of msg.params.callFrames.entries()) {
    InspectorTest.log(`--- ${nr} ---`);
    await session.logSourceLocation(frame.location);
    if (/^wasm/.test(session.getCallFrameUrl(frame)))
      await printLocalScope(frame);
  }
  InspectorTest.log('-------------');
  let action = actions.shift();
  if (!action) {
    InspectorTest.log('ERROR: no more expected action');
    action = 'resume';
  }
  InspectorTest.log(`-> ${action}`);
  Protocol.Debugger[action]();
});

function call_div() {
  instance.exports.div(0, 1, 4711);  // does not trap
  try {
    instance.exports.div(1, 0, 4711);  // traps (div by zero)
  } catch (e) {
    e.stack;  // step target of first pause
  }
  try {
    instance.exports.div(0x80000000, -1, 4711);  // traps (unrepresentable)
  } catch (e) {
    e.stack;  // step target of second pause
  }
}

contextGroup.addScript(call_div.toString());

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    InspectorTest.log('Instantiating.');
    await WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log('Calling div function.');
    await Protocol.Runtime.evaluate({'expression': 'call_div()'});
  }
]);

async function printLocalScope(frame) {
  InspectorTest.log(`scope at ${frame.functionName} (${
      frame.location.lineNumber}:${frame.location.columnNumber}):`);
  for (let scope of frame.scopeChain) {
    if (scope.type != 'local') continue;
    let properties = await Protocol.Runtime.getProperties(
        {'objectId': scope.object.objectId});
    for (let {name, value} of properties.result.result) {
      value = await WasmInspectorTest.getWasmValue(value);
      InspectorTest.log(`   ${name}: ${value}`);
    }
  }
}
