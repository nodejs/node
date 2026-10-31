// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper --cpu-profiler-sampling-interval=1000000

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

d8.profiler.setOnProfileEndListener(OnProfilerSampleCallback);
// Enable profiling on the main isolate before it has any NativeModule.
console.profile();

const builder = new WasmModuleBuilder();
const imp_index = builder.addImport('q', 'f', kSig_i_i);
builder.addFunction('main', kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, imp_index,
    ])
    .exportFunc();
const wire_bytes = builder.toBuffer();

function workerCode() {
  onmessage = ({data: bytes}) => {
    const module = new WebAssembly.Module(bytes);
    // Keep the instance alive until the main thread finishes profiling so the
    // compiled wasm-to-js wrapper stays in the engine-wide
    // WasmImportWrapperCache.
    globalThis.instance = new WebAssembly.Instance(module, {q: {f: (i) => i}});
    postMessage(module);
  };
}

const worker = new Worker(workerCode, {type: 'function'});
worker.postMessage(wire_bytes);
const wasm_module = worker.getMessage();

function imp(i) {
  d8.profiler.triggerSample();
  console.profileEnd();
  return i;
}
const instance = new WebAssembly.Instance(wasm_module, {q: {f: imp}});
instance.exports.main(3);
worker.terminate();

assertTrue(sampleCollected);
