// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping out from javascript to a wasm caller');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

let pause = builder.addImport('imp', 'pause', kSig_v_v);
let func = builder.addFunction('wasm_main', kSig_v_v)
               .addBody([kExprCallFunction, pause])
               .exportAs('main');

let module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log('Paused at:');
  var frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  await Protocol.Debugger.stepOut();
});

contextGroup.addScript(`
let pause = true;
function pauseAlternating() {
  if (pause) debugger;
  pause = !pause;
}
`);

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiating.');
    const instantiate_code =
        `const instance = (${WasmInspectorTest.instantiateFromBuffer})(${
            JSON.stringify(module_bytes)}, {imp: {pause: pauseAlternating}});`;
    WasmInspectorTest.evalWithUrl(instantiate_code, 'instantiate');
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
    const scriptId = wasmScript.scriptId;

    InspectorTest.log('Running exports.main.');
    InspectorTest.log('>>> First round');
    await Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});
    InspectorTest.log('exports.main returned.');

    InspectorTest.log('After stepping out of the last script, we should stop right at the beginning of the next script.');
    InspectorTest.log('>>> Second round');
    await Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});
    InspectorTest.log('exports.main returned.');

    InspectorTest.log('The next cycle should work as before (stopping at the "debugger" statement), after stopping at script entry.');
    InspectorTest.log('>>> Third round');
    await Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});
    InspectorTest.log('exports.main returned.');
  }
]);
