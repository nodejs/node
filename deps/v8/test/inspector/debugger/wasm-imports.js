// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start('Tests imports in wasm');

// Build two modules A and B. A defines function func, which contains a
// breakpoint. This function is then imported by B and called via main. The
// breakpoint must be hit.
// This failed before (http://crbug.com/v8/5971).

var builder_a = new WasmModuleBuilder();
var func = builder_a.addFunction('func', kSig_v_v)
                   .addBody([kExprNop])
                   .exportFunc();
var module_a_bytes = builder_a.toArray();

var builder_b = new WasmModuleBuilder();
var import_idx = builder_b.addImport('imp', 'f', kSig_v_v);
builder_b.addFunction('main', kSig_v_v)
    .addBody([kExprCallFunction, import_idx])
    .exportFunc();
var module_b_bytes = builder_b.toArray();

function instantiate(bytes, imp) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }

  var module = new WebAssembly.Module(buffer);
  // Add to global instances array.
  instances.push(new WebAssembly.Instance(module, imp));
}

session.setupScriptMap();

// Main promise chain:
Protocol.Runtime.enable()
    .then(() => Protocol.Debugger.enable())
    .then(() => InspectorTest.log('Installing code and global variable.'))
    .then(
        () => WasmInspectorTest.evalWithUrl(
            'var instances = [];\n' + instantiate.toString(), 'setup'))
    .then(() => InspectorTest.log('Calling instantiate function for module A.'))
    .then(
        () =>
            (WasmInspectorTest.evalWithUrl(
                 'instantiate(' + JSON.stringify(module_a_bytes) + ')',
                 'instantiateA'),
             0))
    .then(() => InspectorTest.log('Waiting for wasm script to be parsed.'))
    .then(waitForWasmScript)
    .then(url => (InspectorTest.log('Setting breakpoint in line 1:'), url))
    .then(
        url =>
            Protocol.Debugger.setBreakpointByUrl(
                {lineNumber: 0, columnNumber: func.body_offset, url: url}))
    .then(printFailure)
    .then(msg => session.logSourceLocations(msg.result.locations))
    .then(() => InspectorTest.log('Calling instantiate function for module B.'))
    .then(
        () =>
            (WasmInspectorTest.evalWithUrl(
                 'instantiate(' + JSON.stringify(module_b_bytes) +
                     ', {imp: {f: instances[0].exports.func}})',
                 'instantiateB'),
             0))
    .then(() => InspectorTest.log('Calling main function on module B.'))
    .then(() => WasmInspectorTest.evalWithUrl('instances[1].exports.main()', 'runWasm'))
    .then(() => InspectorTest.log('exports.main returned.'))
    .then(() => InspectorTest.log('Finished.'))
    .then(InspectorTest.completeTest);

// Separate promise chain for the asynchronous pause:
Protocol.Debugger.oncePaused()
    .then(msg => msg.params.callFrames[0].location)
    .then(
        loc =>
            (InspectorTest.log(
                 'Paused at ' + loc.lineNumber + ':' + loc.columnNumber + '.'),
             loc))
    .then(session.logSourceLocation.bind(session))
    .then(
        () => InspectorTest.log(
            'Getting current stack trace via "new Error().stack".'))
    .then(() => WasmInspectorTest.evalWithUrl('new Error().stack', 'getStack'))
    .then(msg => InspectorTest.log(msg.result.result.value))
    .then(Protocol.Debugger.resume);

function printFailure(message) {
  if (!message.result) {
    InspectorTest.logMessage(message);
  }
  return message;
}

function waitForWasmScript(msg) {
  if (!msg || !msg.params.url.startsWith('wasm://')) {
    return Protocol.Debugger.onceScriptParsed().then(waitForWasmScript);
  }
  InspectorTest.log('Got wasm script!');
  return Promise.resolve(msg.params.url);
}
