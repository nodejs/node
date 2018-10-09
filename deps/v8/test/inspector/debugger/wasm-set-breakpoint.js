// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests stepping through wasm scripts');

utils.load('test/mjsunit/wasm/wasm-constants.js');
utils.load('test/mjsunit/wasm/wasm-module-builder.js');

var builder = new WasmModuleBuilder();

var func_a_idx =
    builder.addFunction('wasm_A', kSig_v_v).addBody([kExprNop, kExprNop]).index;

// wasm_B calls wasm_A <param0> times.
builder.addFunction('wasm_B', kSig_v_i)
    .addBody([
      // clang-format off
      kExprLoop, kWasmStmt,               // while
        kExprGetLocal, 0,                 // -
        kExprIf, kWasmStmt,               // if <param0> != 0
          kExprGetLocal, 0,               // -
          kExprI32Const, 1,               // -
          kExprI32Sub,                    // -
          kExprSetLocal, 0,               // decrease <param0>
          kExprCallFunction, func_a_idx,  // -
          kExprBr, 1,                     // continue
          kExprEnd,                       // -
        kExprEnd,                         // break
      // clang-format on
    ])
    .exportAs('main');

var module_bytes = builder.toArray();

function instantiate(bytes) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  var module = new WebAssembly.Module(buffer);
  // Set global variable.
  instance = new WebAssembly.Instance(module);
}

var evalWithUrl = (code, url) => Protocol.Runtime.evaluate(
    {'expression': code + '\n//# sourceURL=v8://test/' + url});

var wasm_B_scriptId;
var sources = {};
var urls = {};
var afterTwoSourcesCallback;

Protocol.Debugger.onPaused((msg) => {
  var loc = msg.params.callFrames[0].location;
  InspectorTest.log(loc.lineNumber);
  Protocol.Debugger.resume();
});
Protocol.Debugger.enable()
    .then(() => InspectorTest.log('Installing code an global variable.'))
    .then(
        () => evalWithUrl('var instance;\n' + instantiate.toString(), 'setup'))
    .then(() => InspectorTest.log('Calling instantiate function.'))
    .then(
        () =>
            (evalWithUrl(
                 'instantiate(' + JSON.stringify(module_bytes) + ')',
                 'callInstantiate'),
             0))
    .then(waitForTwoWasmScripts)
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 8 (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 8}}))
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 7 (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 7}}))
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 6 (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 6}}))
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 5 (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 5}}))
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 3 (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 3}}))
    .then(
        () => InspectorTest.log(
            'Setting breakpoint on line 4  (on the setlocal before the call), url ' +
            urls[wasm_B_scriptId]))
    .then(
        () => Protocol.Debugger.setBreakpoint(
            {'location': {'scriptId': wasm_B_scriptId, 'lineNumber': 4}}))
    .then(printFailure)
    .then(() => evalWithUrl('instance.exports.main(4)', 'runWasm'))
    .then(() => InspectorTest.log('exports.main returned!'))
    .then(() => InspectorTest.log('Finished!'))
    .then(InspectorTest.completeTest);

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

function waitForTwoWasmScripts() {
  var num = 0;
  InspectorTest.log('Waiting for two wasm scripts to be parsed.');
  var promise = new Promise(fulfill => gotBothSources = fulfill);
  function waitForMore() {
    if (num == 2) return promise;
    Protocol.Debugger.onceScriptParsed()
        .then(handleNewScript)
        .then(waitForMore);
  }
  function handleNewScript(msg) {
    var url = msg.params.url;
    if (!url.startsWith('wasm://')) {
      InspectorTest.log('Ignoring script with url ' + url);
      return;
    }
    num += 1;
    var scriptId = msg.params.scriptId;
    urls[scriptId] = url;
    InspectorTest.log('Got wasm script: ' + url);
    if (url.substr(-2) == '-1') wasm_B_scriptId = scriptId;
    InspectorTest.log('Requesting source for ' + url + '...');
    Protocol.Debugger.getScriptSource({scriptId: scriptId})
        .then(printFailure)
        .then(msg => sources[scriptId] = msg.result.scriptSource)
        .then(InspectorTest.log)
        .then(() => Object.keys(sources).length == 2 ? gotBothSources() : 0);
  }
  waitForMore();
  return promise;
}
