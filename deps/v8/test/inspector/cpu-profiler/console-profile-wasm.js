// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that console profiles contain wasm function names.');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

// Add fibonacci function.
var builder = new WasmModuleBuilder();
builder.addFunction('fib', kSig_i_i)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprI32Const, 2,
      kExprI32LeS,  // i < 2 ?
      kExprBrIf, 0, // --> return i
      kExprI32Const, 1, kExprI32Sub,  // i - 1
      kExprCallFunction, 0, // fib(i - 1)
      kExprGetLocal, 0, kExprI32Const, 2, kExprI32Sub,  // i - 2
      kExprCallFunction, 0, // fib(i - 2)
      kExprI32Add
    ])
    .exportFunc();
let module_bytes = builder.toArray();

function compile(bytes) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    view[i] = bytes[i] | 0;
  }
  let module = new WebAssembly.Module(buffer);
  let instance = new WebAssembly.Instance(module);
  return instance;
}

function checkError(message)
{
  if (message.error) {
    InspectorTest.log("Error: ");
    InspectorTest.logMessage(message);
    InspectorTest.completeTest();
  }
}

(async function test() {
  Protocol.Profiler.enable();
  checkError(await Protocol.Profiler.start());
  let found_fib_in_profile = false;
  let finished_profiles = 0;
  Protocol.Profiler.onConsoleProfileFinished(e => {
    ++finished_profiles;
    if (e.params.profile.nodes.some(n => n.callFrame.functionName === 'fib'))
      found_fib_in_profile = true;
  });
  InspectorTest.log('Compiling wasm.');
  checkError(await Protocol.Runtime.evaluate({
    expression: 'const instance = (' + compile + ')(' +
        JSON.stringify(module_bytes) + ');'
  }));
  InspectorTest.log(
      'Running fib with increasing input until it shows up in the profile.');
  for (let i = 1; !found_fib_in_profile; ++i) {
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'console.profile(\'profile\');'}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'instance.exports.fib(' + i + ');'}));
    checkError(await Protocol.Runtime.evaluate(
        {expression: 'console.profileEnd(\'profile\');'}));
    if (finished_profiles != i) {
      InspectorTest.log(
          'Missing consoleProfileFinished message (expected ' + i + ', got ' +
          finished_profiles + ')');
    }
  }
  InspectorTest.log('Found fib in profile.');
  InspectorTest.completeTest();
})().catch(e => InspectorTest.log('caught: ' + e));
