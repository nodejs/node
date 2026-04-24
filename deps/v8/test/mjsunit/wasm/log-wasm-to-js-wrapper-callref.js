// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Increase the profiler sampling interval to avoid a data race between
// interval-triggered samples and explicitly triggered samples. The goal of the
// big interval is to avoid any interval-triggered samples.
// Flags: --cpu-profiler-sampling-interval=1000000
// Flags: --experimental-wasm-type-reflection

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let sampleCollected = false;
function OnProfilerSampleCallback(profile) {
  profile = profile.replaceAll('\\', '/');
  profile = JSON.parse(profile);
  let functionNames = profile.nodes.map(n => n.callFrame.functionName);
  for (let i = 0; i < functionNames.length; ++i) {
    if (functionNames[i].startsWith('js-to-wasm') &&
        functionNames[i + 1]?.startsWith('main') &&
        functionNames[i + 2]?.startsWith('wasm-to-js') &&
        functionNames[i + 3]?.startsWith('imp')) {
      sampleCollected = true;
      return;
    }
  }
}

const builder = new WasmModuleBuilder();
const sigId = builder.addType(kSig_i_i);
const g = builder.addImportedGlobal('m', 'val', kWasmAnyFunc);
builder.addFunction('main', sigId)
    .addBody([
      kExprLocalGet,
      0,
      kExprGlobalGet,
      g,
      kGCPrefix,
      kExprRefCast,
      sigId,
      kExprCallRef,
      sigId,
    ])
    .exportFunc();
const wasm_module = builder.toModule();

d8.profiler.setOnProfileEndListener(OnProfilerSampleCallback);
function imp(i) {
  d8.profiler.triggerSample();
  console.profileEnd();
}
const wrapped_imp =
    new WebAssembly.Function({parameters: ['i32'], results: ['i32']}, imp);
let instance = new WebAssembly.Instance(wasm_module, {m: {val: wrapped_imp}});

// In its default configuration, this test is deterministic. However, in an
// isolate test it can happen that the first isolate compiles `main` and the
// second isolate calls `main` before the first isolate logs it, i.e. the
// profiler of the second isolate is not yet aware of `main` and therefore does
// not symbolize it. Therefore we try multiple times.
for (let i = 0; i < 5; ++i) {
  console.profile();
  instance.exports.main(3);
  if (sampleCollected) break;
}

assertTrue(sampleCollected);
