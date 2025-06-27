// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Increase the profiler sampling interval to avoid a data race between
// interval-triggered samples and explicitly triggered samples. The goal of the
// big interval is to avoid any interval-triggered samples.
// Flags: --cpu-profiler-sampling-interval=1000000

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let sampleCollected = false;
function OnProfilerSampleCallback(profile) {
  profile = profile.replaceAll('\\', '/');
  profile = JSON.parse(profile);
  let wasm_to_js_index = 0;
  let js_to_wasm_index = 0;
  let fib_index = 0;
  let imp_index = 0;
  let functionNames = profile.nodes.map(n => n.callFrame.functionName);
  for (let i = 0; i < functionNames.length; ++i) {
    if (functionNames[i].startsWith('js-to-wasm')) {
      assertTrue(functionNames[i + 1].startsWith('main'));
      assertTrue(functionNames[i + 2].startsWith('wasm-to-js'));
      assertTrue(functionNames[i + 3].startsWith('imp'));
      // {sampleCollected} is set at the end because the asserts above don't
      // show up in the test runner, probably because this function is called as
      // a callback from d8.
      sampleCollected = true;
      return;
    }
  }
  assertUnreachable();
}

const builder = new WasmModuleBuilder();
const sigId = builder.addType(kSig_i_i);
const imp_index = builder.addImport('q', 'f', sigId);
const table = builder.addTable(kWasmAnyFunc, 10).index;
builder.addActiveElementSegment(table, wasmI32Const(9), [imp_index]);
builder.addFunction('main', sigId)
    .addBody([
      kExprLocalGet,
      0,
      kExprI32Const,
      9,
      kExprCallIndirect,
      sigId,
      table,
    ])
    .exportFunc();
const wasm_module = builder.toModule();

d8.profiler.setOnProfileEndListener(OnProfilerSampleCallback);
function imp(i) {
  d8.profiler.triggerSample();
  console.profileEnd();
}
let instance = new WebAssembly.Instance(wasm_module, {q: {f: imp}});
console.profile();
instance.exports.main(3);
assertTrue(sampleCollected);
