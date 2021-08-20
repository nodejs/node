// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

utils.load('test/mjsunit/wasm/wasm-module-builder.js');

let {session, contextGroup, Protocol} = InspectorTest.start('Regress 10957');

var builder = new WasmModuleBuilder();
let pause = builder.addImport('imports', 'pause', kSig_v_v);
let sig = makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
let f = builder.addFunction('f', sig)
  .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprLocalGet, 6,
      kExprI32Add,
      kExprI32Add,
      kExprI32Add,
      kExprI32Add,
      kExprI32Add,
      kExprI32Add,
  ]);

builder.addFunction('main', kSig_i_v)
    .addBody([
        kExprCallFunction, pause,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprI32Const, 1,
        kExprCallFunction, f.index])
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
  async function testRegress10957() {
    await Protocol.Debugger.enable();
    InspectorTest.log('Instantiate');
    const code =
        `let instance = (${instantiate})(${JSON.stringify(module_bytes)}, {'imports': {'pause': () => { %ScheduleBreak() } }});
        instance.exports.main();
    `;
    Protocol.Runtime.evaluate({'expression': code}).then(message =>
        InspectorTest.logMessage(message.result.result.value));
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.resume();
  }
]);
