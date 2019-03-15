// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

let {session, contextGroup, Protocol} = InspectorTest.start('Tests how wasm scripts are reported');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

// Add two empty functions. Both should be registered as individual scripts at
// module creation time.
var builder = new WasmModuleBuilder();
builder.addFunction('nopFunction', kSig_v_v).addBody([kExprNop]);
builder.addFunction('main', kSig_v_v)
    .addBody([kExprBlock, kWasmStmt, kExprI32Const, 2, kExprDrop, kExprEnd])
    .exportAs('main');
var module_bytes = builder.toArray();

function testFunction(bytes) {
  var buffer = new ArrayBuffer(bytes.length);
  var view = new Uint8Array(buffer);
  for (var i = 0; i < bytes.length; i++) {
    view[i] = bytes[i] | 0;
  }

  // Compilation triggers registration of wasm scripts.
  new WebAssembly.Module(buffer);
}

contextGroup.addScript(testFunction.toString(), 0, 0, 'v8://test/testFunction');
contextGroup.addScript('var module_bytes = ' + JSON.stringify(module_bytes));

Protocol.Debugger.enable();
Protocol.Debugger.onScriptParsed(handleScriptParsed);
InspectorTest.log(
    'Check that inspector gets two wasm scripts at module creation time.');
Protocol.Runtime
    .evaluate({
      'expression': '//# sourceURL=v8://test/runTestRunction\n' +
          'testFunction(module_bytes)'
    })
    .then(checkFinished);

var num_scripts = 0;
var missing_sources = 0;

function checkFinished() {
  if (missing_sources == 0)
    InspectorTest.completeTest();
}

function handleScriptParsed(messageObject)
{
  var url = messageObject.params.url;
  InspectorTest.log("Script #" + num_scripts + " parsed. URL: " + url);
  ++num_scripts;

  if (url.startsWith("wasm://")) {
    ++missing_sources;
    function dumpScriptSource(message) {
      InspectorTest.log("Source for " + url + ":");
      InspectorTest.log(message.result.scriptSource);
      --missing_sources;
    }

    Protocol.Debugger.getScriptSource({scriptId: messageObject.params.scriptId})
        .then(dumpScriptSource.bind(null))
        .then(checkFinished);
  }
}
