// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test scope inspection and stepping after a trap.');
session.setupScriptMap();

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();

// Create a function which computes the div of the first two arguments.
builder.addFunction('div', kSig_i_iii)
    .addLocals(
        {i32_count: 2}, ['a', 'b', 'unused', 'local_zero', 'local_const_11'])
    .addBody([
      kExprI32Const, 11,  // const 11
      kExprLocalSet, 4,   // set local #4 ('local_const_11')
      kExprLocalGet, 0,   // param 0
      kExprLocalGet, 1,   // param 1
      kExprI32DivS        // div
    ])
    .exportFunc();

const module_bytes = JSON.stringify(builder.toArray());

function instantiate(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  let module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module);
}

function getShortLocationString(location) {
  return `${location.lineNumber}:${location.columnNumber}`;
}

let actions =
    ['stepInto', 'resume', 'stepInto', 'resume', 'stepInfo', 'resume'];
Protocol.Debugger.onPaused(async msg => {
  InspectorTest.log('Paused at:');
  for (let [nr, frame] of msg.params.callFrames.entries()) {
    InspectorTest.log(`--- ${nr} ---`);
    await session.logSourceLocation(frame.location);
    if (/^wasm/.test(frame.url)) await printLocalScope(frame);
  }
  InspectorTest.log('-------------');
  let action = actions.shift();
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

contextGroup.addScript(instantiate.toString());
contextGroup.addScript(call_div.toString());

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  InspectorTest.log('Instantiating.');
  await Protocol.Runtime.evaluate(
      {'expression': `const instance = instantiate(${module_bytes});`});
  InspectorTest.log('Calling div function.');
  await Protocol.Runtime.evaluate({'expression': 'call_div()'});
  InspectorTest.log('Finished.');
  InspectorTest.completeTest();
})();

async function printLocalScope(frame) {
  InspectorTest.log(`scope at ${frame.functionName} (${
      frame.location.lineNumber}:${frame.location.columnNumber}):`);
  for (let scope of frame.scopeChain) {
    if (scope.type != 'local') continue;
    let properties = await Protocol.Runtime.getProperties(
        {'objectId': scope.object.objectId});
    for (let value of properties.result.result) {
      let msg = await Protocol.Runtime.getProperties(
          {objectId: value.value.objectId});
      let prop_str = p => `"${p.name}": ${p.value.value}`;
      let value_str = msg.result.result.map(prop_str).join(', ');
      InspectorTest.log(`   ${value.name}: ${value_str}`);
    }
  }
}
