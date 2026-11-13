// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-in-js-inlining-body
// Flags: --wasm-in-js-inlining-opt --turbolev --no-maglev

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test for crbug.com/583832157 case 1 (inlined Wasm trap).');

const builder = new WasmModuleBuilder();
const struct = builder.addStruct([makeField(kWasmI32, true)]);

builder.addFunction('createStructNull', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet,
      0,
      kGCPrefix,
      kExprStructNew,
      struct,
      kGCPrefix,
      kExprExternConvertAny,
    ])
    .exportFunc();

builder.addFunction('getFieldNull', makeSig([kWasmExternRef], [kWasmI32]))
    .addBody([
      kExprLocalGet,
      0,
      kGCPrefix,
      kExprAnyConvertExtern,
      kGCPrefix,
      kExprRefCast,
      struct,
      kGCPrefix,
      kExprStructGet,
      struct,
      0,
    ])
    .exportFunc();

const wasm_bytes = JSON.stringify(builder.toArray());

const source = `
const wasm_bytes = new Uint8Array(${wasm_bytes});
const module = new WebAssembly.Module(wasm_bytes);
const instance = new WebAssembly.Instance(module, {});
const getFieldNull = instance.exports.getFieldNull;

function js_fn(x) {
  return getFieldNull(x);
}

%PrepareFunctionForOptimization(js_fn);
const s = instance.exports.createStructNull(42);
for (let i = 0; i < 20; ++i) js_fn(s);
%OptimizeFunctionOnNextCall(js_fn);
js_fn(s);

function run() {
  try {
    js_fn(null);
  } catch (e) {
    return 'caught';
  }
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  Protocol.Debugger.onPaused(msg => {
    InspectorTest.log(
        `EVENT paused: ${msg.params.callFrames[0].functionName || 'wasm'}`);
  });
  Protocol.Debugger.onResumed(() => {
    InspectorTest.log('EVENT resumed');
  });

  InspectorTest.log('Running test:');
  const evalPromise = Protocol.Runtime.evaluate({expression: 'run()'});

  InspectorTest.log('Waiting for first pause:');
  const pauseMsg1 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Paused on exception in: ${
      pauseMsg1.params.callFrames[0].functionName || 'wasm'}`);

  InspectorTest.log('Stepping over:');
  await Protocol.Debugger.stepOver();

  const result = await Promise.race([
    Protocol.Debugger.oncePaused().then(msg => ({type: 'paused', msg})),
    evalPromise.then(res => ({type: 'eval', res})),
  ]);
  InspectorTest.log(`Got event after stepOver: ${result.type}`);
  if (result.type === 'paused') {
    InspectorTest.log(
        `Paused in: ${result.msg.params.callFrames[0].functionName}`);
    await Protocol.Debugger.resume();
  }
  const evalResult = await evalPromise;
  InspectorTest.log(`Result: ${evalResult.result.result.value}`);
  InspectorTest.completeTest();
})();
