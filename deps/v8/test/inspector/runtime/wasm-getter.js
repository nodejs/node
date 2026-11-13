// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that WebAssembly exported functions used as getters are not eagerly invoked');

const builder = new WasmModuleBuilder();
const import_idx = builder.addImport('imports', 'onGet', kSig_v_v);
builder.addFunction('getter', kSig_i_v)
    .addBody([kExprCallFunction, import_idx, kExprI32Const, 42])
    .exportFunc();
const module_bytes = builder.toArray();

(async function test() {
  await Protocol.Runtime.enable();

  await WasmInspectorTest.instantiate(
      module_bytes, 'instance',
      '{ imports: { onGet: () => { globalThis.calls = (globalThis.calls || 0) + 1; } } }');

  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.calls = 0;
      globalThis.obj = {};
      Object.defineProperty(globalThis.obj, 'wasmProp', {
        get: instance.exports.getter,
        enumerable: true,
        configurable: true
      });
    `
  });

  const {result: {result: {objectId}}} =
      await Protocol.Runtime.evaluate({expression: 'globalThis.obj'});

  const {result: {result: properties}} =
      await Protocol.Runtime.getProperties({objectId, ownProperties: true});

  const wasmProp = properties.find(p => p.name === 'wasmProp');
  InspectorTest.log('wasmProp:');
  InspectorTest.log(`  has get: ${!!wasmProp.get}`);
  InspectorTest.log(`  has value: ${!!wasmProp.value}`);

  const {result: {result: callsResult}} =
      await Protocol.Runtime.evaluate({expression: 'globalThis.calls'});
  InspectorTest.log(`getter invocations: ${callsResult.value}`);

  InspectorTest.completeTest();
})();
