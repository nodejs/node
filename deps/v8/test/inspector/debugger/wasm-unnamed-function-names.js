// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests unnamed function in wasm scripts');

var builder = new WasmModuleBuilder();

var imported_idx = builder.addImport('mode', 'import_func', kSig_v_v);

// Unnamed non export function.
var function_idx = builder.addFunction(undefined, kSig_v_v)
                       .addBody([kExprCallFunction, imported_idx])
                       .index;

// Unnamed export function.
builder.addFunction(undefined, kSig_v_v)
    .addBody([kExprCallFunction, function_idx])
    .exportAs('main');

var module_bytes = builder.toArray();

function testFunction(bytes) {
  function call_debugger() {
    debugger;
  }

  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    view[i] = bytes[i] | 0;
  }

  var module = new WebAssembly.Module(buffer);
  var instance =
      new WebAssembly.Instance(module, {mode: {import_func: call_debugger}});

  instance.exports.main();
}

contextGroup.addScript(testFunction.toString());

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Debugger.enable();
    Protocol.Debugger.onPaused(handleDebuggerPaused);
    InspectorTest.log('Running testFunction with generated wasm bytes...');
    await Protocol.Runtime.evaluate(
        {'expression': 'testFunction(' + JSON.stringify(module_bytes) + ')'});
  }
]);

function logStackTrace(messageObject) {
  var frames = messageObject.params.callFrames;
  InspectorTest.log('Number of frames: ' + frames.length);
  for (var i = 0; i < frames.length; ++i) {
    InspectorTest.log('  - [' + i + '] ' + frames[i].functionName);
  }
}

function handleDebuggerPaused(messageObject) {
  InspectorTest.log('Paused on \'debugger;\'');
  logStackTrace(messageObject);
  Protocol.Debugger.resume();
}
