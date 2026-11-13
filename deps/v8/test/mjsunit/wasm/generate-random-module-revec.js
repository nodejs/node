// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-revectorize
// Also enable all features that are used in the randomly generated module. This
// should stay in sync with `EnableExperimentalWasmFeatures` in
// `test/common/wasm/fuzzer-common.cc`.
// Flags: --experimental-fuzzing --wasm-staging
// Enable mixed exception handling cases (legacy & exnref together) while
// this isn't allowed in production (yet).
// Flags: --wasm-allow-mixed-eh-for-testing

function TestModule(module) {
  assertInstanceof(module, WebAssembly.Module);
  try {
    const instance = new WebAssembly.Instance(module);
    %WasmTierUpFunction(instance.exports.main);
  } catch (e) {
    if (!(e instanceof RangeError) || !e.message.includes('Out of memory')) {
      throw e;
    }
  }
}

(function TestGenerateRandomRevecModule() {
  print(arguments.callee.name);
  TestModule(%WasmGenerateRandomRevecModule());
  TestModule(%WasmGenerateRandomRevecModule(4));
  TestModule(%WasmGenerateRandomRevecModule(4.2));
  TestModule(%WasmGenerateRandomRevecModule(new Uint8Array([11, 27])));
  TestModule(%WasmGenerateRandomRevecModule(new Uint8Array([11, 27]).buffer));
})();
