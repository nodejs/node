// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(v8:10266): Figure out why this fails on tsan with --always-opt.
// Flags: --no-always-opt

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that console profiles contain wasm function names.');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

// Add fibonacci function, calling back and forth between JS and Wasm to also
// check for the occurrence of the wrappers.
var builder = new WasmModuleBuilder();
const imp_index = builder.addImport('q', 'f', kSig_i_i);
builder.addFunction('fib', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Const, 2,
      kExprI32LeS,  // i < 2 ?
      kExprBrIf, 0, // --> return i
      kExprI32Const, 1, kExprI32Sub,  // i - 1
      kExprCallFunction, imp_index, // imp(i - 1)
      kExprLocalGet, 0, kExprI32Const, 2, kExprI32Sub,  // i - 2
      kExprCallFunction, imp_index, // imp(i - 2)
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
  let fib = undefined;
  function imp(i) { return fib(i); }
  let instance = new WebAssembly.Instance(module, {q: {f: imp}});
  fib = instance.exports.fib;
  return instance;
}

function checkError(message) {
  if (!message.error) return;
  InspectorTest.log('Error: ');
  InspectorTest.logMessage(message);
  InspectorTest.completeTest();
}

(async function test() {
  Protocol.Profiler.enable();
  checkError(await Protocol.Profiler.start());
  let found_good_profile = false;
  let finished_profiles = 0;
  Protocol.Profiler.onConsoleProfileFinished(e => {
    ++finished_profiles;
    let function_names =
        e.params.profile.nodes.map(n => n.callFrame.functionName);
    // InspectorTest.log(function_names.join(', '));
    // Check for at least one full cycle of
    // fib -> wasm-to-js -> imp -> js-to-wasm -> fib.
    const expected = ['fib', 'wasm-to-js:i:i', 'imp', 'js-to-wasm:i:i', 'fib'];
    for (let i = 0; i <= function_names.length - expected.length; ++i) {
      if (expected.every((val, idx) => val == function_names[i + idx])) {
        found_good_profile = true;
      }
    }
  });
  InspectorTest.log('Compiling wasm.');
  checkError(await Protocol.Runtime.evaluate({
    expression: 'const instance = (' + compile + ')(' +
        JSON.stringify(module_bytes) + ');'
  }));
  InspectorTest.log(
      'Running fib with increasing input until it shows up in the profile.');
  for (let i = 1; !found_good_profile; ++i) {
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
  InspectorTest.log('Found expected functions in profile.');
  InspectorTest.completeTest();
})().catch(e => InspectorTest.log('caught: ' + e));
