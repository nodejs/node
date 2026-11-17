// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/572403047.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));
contextGroup.addScript(`
const builder = new WasmModuleBuilder();
const fn = builder.addImport('m', 'fn', kSig_v_v);
builder.addFunction('main', kSig_v_v)
    .addBody([kExprCallFunction, fn])
    .exportFunc();
const instance = builder.instantiate({m: {fn: Math.sin}});
`);

let objectId;
let paused_count = 0;
Protocol.Debugger.onPaused(() => {
  ++paused_count;
  Protocol.Debugger.stepInto();
});

async function test() {
  await Protocol.Debugger.enable();
  ({result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression: 'Math.sin'}));
  await Protocol.Debugger.setBreakpointOnFunctionCall({objectId});
  await Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});
  InspectorTest.log(`paused_count: ${paused_count}`);
  InspectorTest.completeTest();
}
test();
