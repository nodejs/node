// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

var imported_idx = builder.addImport("xxx", "func", kSig_v_v);

var call_imported_idx = builder.addFunction("call_func", kSig_v_v)
    .addBody([kExprCallFunction, imported_idx])
    .index;

var sig_index = builder.addType(kSig_v_v);

builder.addFunction('main', kSig_v_v)
    .addBody([
      kExprBlock, kWasmStmt, kExprI32Const, 0, kExprCallIndirect, sig_index,
      kTableZero, kExprEnd
    ])
    .exportAs('main');

builder.appendToTable([call_imported_idx]);

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
  var instance = new WebAssembly.Instance(module, {xxx: {func: call_debugger}});

  instance.exports.main();
}

InspectorTest.addScript(testFunction.toString());
InspectorTest.addScript('var module_bytes = ' + JSON.stringify(module_bytes));

Protocol.Debugger.enable();
Protocol.Debugger.onPaused(handleDebuggerPaused);
InspectorTest.log('Check that inspector gets disassembled wasm code');
Protocol.Runtime.evaluate({'expression': 'testFunction(module_bytes)'});

function handleDebuggerPaused(message) {
  InspectorTest.log('Paused on debugger!');
  var frames = message.params.callFrames;
  InspectorTest.log('Number of frames: ' + frames.length);
  function dumpSourceLine(frameId, sourceMessage) {
    if (sourceMessage.error) InspectorTest.logObject(sourceMessage);
    var text = sourceMessage.result.scriptSource;
    var lineNr = frames[frameId].location.lineNumber;
    var line = text.split('\n')[lineNr];
    InspectorTest.log('[' + frameId + '] ' + line);
  }
  function next(frameId) {
    if (frameId == frames.length) return Promise.resolve();
    return Protocol.Debugger
        .getScriptSource({scriptId: frames[frameId].location.scriptId})
        .then(dumpSourceLine.bind(null, frameId))
        .then(() => next(frameId + 1));
  }
  function finished() {
    InspectorTest.log('Finished.');
    InspectorTest.completeTest();
  }
  next(0).then(finished);
}
