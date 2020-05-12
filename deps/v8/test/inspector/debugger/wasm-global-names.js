// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test wasm global names');

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addImportedGlobal('module_name', 'imported_global', kWasmI32, false);
let func = builder.addFunction('func', kSig_v_i)
               .addBody([
                 kExprGlobalGet, 0,  //
                 kExprDrop,          //
               ])
               .exportAs('main');
var o = builder.addGlobal(kWasmI32, func).exportAs('exported_global');
builder.addGlobal(kWasmI32);  // global2
let moduleBytes = JSON.stringify(builder.toArray());

function test(moduleBytes) {
  let module = new WebAssembly.Module((new Uint8Array(moduleBytes)).buffer);
  let imported_global_value = 123;
  instance = new WebAssembly.Instance(
      module, {module_name: {imported_global: imported_global_value}});
}

(async function() {
  try {
    Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({
      expression: `
      let instance;
      ${test.toString()}
      test(${moduleBytes});`
    });

    InspectorTest.log('Waiting for wasm script to be parsed.');
    let scriptId;
    while (true) {
      let msg = await Protocol.Debugger.onceScriptParsed();
      if (msg.params.url.startsWith('wasm://')) {
        scriptId = msg.params.scriptId;
        break;
      }
    }

    InspectorTest.log('Setting breakpoint in wasm.');
    await Protocol.Debugger.setBreakpoint(
        {location: {scriptId, lineNumber: 0, columnNumber: func.body_offset}});

    InspectorTest.log('Running main.');
    Protocol.Runtime.evaluate({expression: 'instance.exports.main()'});

    let msg = await Protocol.Debugger.oncePaused();
    let callFrames = msg.params.callFrames;
    InspectorTest.log('Paused in debugger.');
    let scopeChain = callFrames[0].scopeChain;
    for (let scope of scopeChain) {
      if (scope.type != 'global') continue;

      let globalObjectProps = (await Protocol.Runtime.getProperties({
                                'objectId': scope.object.objectId
                              })).result.result;

      for (let prop of globalObjectProps) {
        let subProps = (await Protocol.Runtime.getProperties({
                         objectId: prop.value.objectId
                       })).result.result;
        let values = subProps.map((value) => `"${value.name}"`).join(', ');
        InspectorTest.log(`   ${prop.name}: {${values}}`);
      }
    }

    InspectorTest.log('Finished.');
  } catch (exc) {
    InspectorTest.log(`Failed with exception: ${exc}.`);
  } finally {
    InspectorTest.completeTest();
  }
})();
