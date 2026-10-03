// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} =
    InspectorTest.start('Test scope inspection of an implicit inline trap.');
session.setupScriptMap();

const builder = new WasmModuleBuilder();
builder.addMemory(1, 1);
builder.addFunction('trigger_oob_trap', makeSig([], [kWasmI32]))
    .addBody([
      ...wasmI32Const(42),     // constant to keep on the expression stack
      ...wasmI32Const(100000), // highly out of bounds index
      kExprI32LoadMem, 0, 0,
      kExprDrop                // drop the loaded value, leaving only 42
    ])
    .exportFunc();

const module_bytes = builder.toArray();

Protocol.Debugger.onPaused(async msg => {
  InspectorTest.log('Paused at:');
  for (let [nr, frame] of msg.params.callFrames.entries()) {
    InspectorTest.log(`--- ${nr} ---`);
    await session.logSourceLocation(frame.location);
    if (/^wasm/.test(session.getCallFrameUrl(frame))) {
      for (var scope of frame.scopeChain) {
        InspectorTest.log(` - scope (${scope.type}):`);
        if (scope.type === 'wasm-expression-stack') {
          let objectId = scope.object.objectId;
          objectId = (await Protocol.Runtime.callFunctionOn({
            functionDeclaration: 'function() { return this.stack }',
            objectId
          })).result.result.objectId;
          var scope_properties =
              await Protocol.Runtime.getProperties({objectId});
          let str = (await Promise.all(scope_properties.result.result.map(
                         elem => WasmInspectorTest.getWasmValue(elem.value))))
                        .join(', ');
          InspectorTest.log(`   stack: [${str}]`);
        } else if (scope.type === 'module') {
          InspectorTest.log(`   instance: <module-scope>`);
          InspectorTest.log(`   module: <module-scope>`);
          InspectorTest.log(`   functions: <module-scope>`);
          InspectorTest.log(`   memories: <module-scope>`);
        } else {
          var properties = await Protocol.Runtime.getProperties(
              {'objectId': scope.object.objectId});
          for (let {name, value} of properties.result.result) {
            let valStr = value.value !== undefined ? value.value : JSON.stringify(value);
            InspectorTest.log(`   ${name}: ${valStr}`);
          }
        }
      }
    }
  }
  InspectorTest.log('-------------');
  Protocol.Debugger.resume();
});

function call_trap() {
  try {
    instance.exports.trigger_oob_trap();
  } catch (e) {
    e.stack;
  }
}

contextGroup.addScript(call_trap.toString());

InspectorTest.runAsyncTestSuite([
  async function test() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    InspectorTest.log('Instantiating.');
    await WasmInspectorTest.instantiate(module_bytes);
    InspectorTest.log('Calling trapping function.');
    await Protocol.Runtime.evaluate({'expression': 'call_trap()'});
  }
]);
