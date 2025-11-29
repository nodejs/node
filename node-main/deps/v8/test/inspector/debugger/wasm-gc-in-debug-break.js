// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc
utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests GC within DebugBreak');
session.setupScriptMap();

let builder = new WasmModuleBuilder();

let f_index = builder.addImport('foo', 'bar', kSig_v_r);

builder.addFunction('wasm_A', kSig_v_r)
    .addBody([
      kExprLocalGet, 0,           // -
      kExprCallFunction, f_index  // -
    ])
    .exportAs('main');

let module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async message => {
  let frames = message.params.callFrames;
  await session.logSourceLocation(frames[0].location);
  await Protocol.Runtime.evaluate({expression: 'gc()'});
  InspectorTest.log('GC triggered');
  let action = 'resume';
  InspectorTest.log('Debugger.' + action);
  await Protocol.Debugger[action]();
})

contextGroup.addScript(`
function test() {
  debug(instance.exports.main);
  instance.exports.main({val: "Hello World"});
}`, 0, 0, 'test.js');

InspectorTest.runAsyncTestSuite([async function test() {
  utils.setLogConsoleApiMessageCalls(true);
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await WasmInspectorTest.instantiate(
      module_bytes, 'instance', '{foo: {bar: (x) => console.log(x.val)}}');
  await Protocol.Runtime.evaluate(
      {expression: 'test()', includeCommandLineAPI: true});
  InspectorTest.log('exports.main returned!');
}]);
