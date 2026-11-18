// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test stepping over a wasm exception catch block');
session.setupScriptMap();

const builder = new WasmModuleBuilder();
const func2 =
    builder.addFunction('func2', kSig_v_v).addBody([kExprUnreachable]);
const func1 = builder.addFunction('func1', kSig_v_v).addBody([
  kExprCallFunction, func2.index
]);
builder.addFunction('export_f', kSig_v_v)
    .addBody([kExprCallFunction, func1.index])
    .exportAs('wasmFn');

const module_bytes = builder.toArray();

contextGroup.addScript(`
  var wasmFn;
  function f() {
    debugger;
    try {
      wasmFn();
    } catch(e) {
      var x = 1;
    }
  }
  function g() {
    f();
    var z = 3;
  }
  function h() {
    g();
    var w = 4;
  }
  function run(bytes) {
    const module = new WebAssembly.Module(new Uint8Array(bytes));
    const instance = new WebAssembly.Instance(module);
    wasmFn = instance.exports.wasmFn;
    h();
  }
`);

let step_count = 0;

Protocol.Debugger.onPaused(async message => {
  const frames = message.params.callFrames;
  InspectorTest.log('Paused on:');
  for (const frame of frames) {
    InspectorTest.log(`  ${frame.functionName} (${frame.url})`);
  }
  await session.logSourceLocation(frames[0].location);

  if (step_count < 2) {
    step_count++;
    InspectorTest.log('Stepping over...');
    await Protocol.Debugger.stepOver();
  } else {
    InspectorTest.log('Resuming...');
    await Protocol.Debugger.resume();
  }
});

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.evaluate(
      {expression: `run(${JSON.stringify(module_bytes)})`, awaitPromise: true});
  InspectorTest.completeTest();
})();
