// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests that cloning a module notifies the debugger');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_v_v).addBody([]).exportAs('f');
let moduleBytes = JSON.stringify(builder.toArray());

contextGroup.addScript(`
  function test(moduleBytes) {
    let wireBytes = new Uint8Array(moduleBytes);
    let module = new WebAssembly.Module(wireBytes.buffer);
    let serialized = %SerializeWasmModule(module);
    let module2 = %DeserializeWasmModule(serialized, wireBytes);
    let module3 = %CloneWasmModule(module);
  }
`);

let scriptsSeen = 0;

Protocol.Debugger.onScriptParsed(msg => {
  let url = msg.params.url;
  if (url.startsWith('wasm://')) {
    InspectorTest.log(`Got URL: ${url}`);
    if (++scriptsSeen == 3) {
      InspectorTest.log('Done!');
      InspectorTest.completeTest();
    }
  }
});

Protocol.Debugger.enable();
Protocol.Runtime.evaluate({expression: `test(${moduleBytes});`});
