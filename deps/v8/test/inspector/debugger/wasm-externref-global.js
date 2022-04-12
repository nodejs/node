// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test wasm scope information with externref globals');

InspectorTest.runAsyncTestSuite([
  async function test() {
    let builder = new WasmModuleBuilder();
    builder.addImportedGlobal('m', 'global', kWasmExternRef, false);
    let func = builder.addFunction('func', kSig_v_v)
        .addBody([
          kExprGlobalGet, 0,  //
          kExprDrop,          //
        ])
        .exportAs('main');
    let moduleBytes = JSON.stringify(builder.toArray());

    function test(moduleBytes) {
      let module = new WebAssembly.Module((new Uint8Array(moduleBytes)).buffer);
      let global = 'hello, world';
      instance = new WebAssembly.Instance(module, {m: {global}});
    }

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
      if (scope.type != 'module') continue;

      let moduleObjectProps = (await Protocol.Runtime.getProperties({
                              'objectId': scope.object.objectId
                            })).result.result;

      for (let prop of moduleObjectProps) {
        if (prop.name != 'globals') continue;
        let subProps = (await Protocol.Runtime.getProperties({
                            objectId: prop.value.objectId
                          })).result.result[0];
        let subsubProps = (await Protocol.Runtime.getProperties({
                            objectId: subProps.value.objectId
                          })).result.result;
        let values =
            subsubProps.map((value) => `"${value.name}": ${value.value.value}`)
                .join(', ');
        InspectorTest.log(`   ${prop.name}: {"name": ${subProps.name}, ${values}}`);
      }
    }
  }
]);
