// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --liftoff --no-wasm-tier-up --expose-gc
// Flags: --no-wasm-dynamic-tiering --no-wasm-lazy-compilation
// Compile functions 0 and 2 with Turbofan, the rest with Liftoff:
// Flags: --wasm-tier-mask-for-testing=5

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 5;

function create_builder() {
  const builder = new WasmModuleBuilder();
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i))
        .exportFunc();
  }
  return builder;
}

function check(instance) {
  for (let i = 0; i < num_functions; ++i) {
    const expect_turbofan = i == 0 || i == 2;
    assertEquals(
        expect_turbofan, %IsTurboFanFunction(instance.exports['f' + i]),
        'function ' + i);
  }
}

const wire_bytes = create_builder().toBuffer();

function testTierTestingFlag() {
  print(arguments.callee.name);
  const module = new WebAssembly.Module(wire_bytes);
  const buff = %SerializeWasmModule(module);
  const instance = new WebAssembly.Instance(module);
  check(instance);
  return buff;
};

const serialized_module = testTierTestingFlag();
// Do some GCs to make sure the first module got collected and removed from the
// module cache.
gc();
gc();
gc();

(function testSerializedModule() {
  print(arguments.callee.name);
  const module = %DeserializeWasmModule(serialized_module, wire_bytes);

  const instance = new WebAssembly.Instance(module);
  check(instance);
})();
