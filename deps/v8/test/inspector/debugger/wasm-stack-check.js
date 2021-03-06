// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests pausing a running script and stepping');

var builder = new WasmModuleBuilder();

let pause = builder.addImport('imports', 'pause', kSig_v_v);
let f = builder.addFunction('f', kSig_i_i)
  .addBody([
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Add]);
let main = builder.addFunction('main', kSig_i_v)
    .addBody([
        kExprCallFunction, pause,
        kExprI32Const, 12, kExprCallFunction, f.index])
  .exportFunc();

var module_bytes = builder.toArray();

function instantiate(bytes, imports) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; ++i) {
    view[i] = bytes[i] | 0;
  }
  const module = new WebAssembly.Module(buffer);
  return new WebAssembly.Instance(module, imports);
}

InspectorTest.runAsyncTestSuite([
  async function testPauseAndStep() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiate');
    const instantiate_code = `var instance = (${instantiate})(${JSON.stringify(module_bytes)}, {'imports': {'pause': () => { %ScheduleBreak() } }});`;
    WasmInspectorTest.evalWithUrl(instantiate_code, 'instantiate');
    InspectorTest.log('Wait for script');
    const [, {params: wasmScript}] = await Protocol.Debugger.onceScriptParsed(2);
    InspectorTest.log('Got wasm script: ' + wasmScript.url);

    InspectorTest.log('Run');
    Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});
    InspectorTest.log('Expecting to pause at ' + (f.body_offset - 1));
    await waitForPauseAndStep('stepInto');
    await waitForPauseAndStep('stepInto');
    await waitForPauseAndStep('stepInto');
    await waitForPauseAndStep('stepInto');
    await waitForPauseAndStep('resume');
  }
]);

async function waitForPauseAndStep(stepAction) {
  const msg = await Protocol.Debugger.oncePaused();
  await inspect(msg.params.callFrames[0]);
  Protocol.Debugger[stepAction]();
}

async function inspect(frame) {
  let loc = frame.location;
  let line = [`Paused at offset ${loc.columnNumber}`];
  // Inspect only the top wasm frame.
  for (var scope of frame.scopeChain) {
    if (scope.type == 'module') continue;
    var scope_properties =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    let str = scope_properties.result.result.map(
        elem => WasmInspectorTest.getWasmValue(elem.value)).join(', ');
    line.push(`${scope.type}: [${str}]`);
  }
  InspectorTest.log(line.join('; '));
}
