// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --wasm-dynamic-tiering --liftoff
// Make the test faster:
// Flags: --wasm-tiering-budget=1000

// This test busy-waits for tier-up to be complete, hence it does not work in
// predictable mode where we only have a single thread.
// Flags: --no-predictable

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const num_functions = 3;

function create_builder() {
  const builder = new WasmModuleBuilder();
  builder.addImport("foo", "bar", kSig_i_v);
  for (let i = 0; i < num_functions; ++i) {
    builder.addFunction('f' + i, kSig_i_v)
        .addBody(wasmI32Const(i))
        .exportFunc();
  }
  return builder;
}

const wire_bytes = create_builder().toBuffer();

function serializeModule() {
  const module = new WebAssembly.Module(wire_bytes);
  let instance = new WebAssembly.Instance(module, {foo: {bar: () => 1}});
  // Execute {f1} until it gets tiered up.
  while (%IsLiftoffFunction(instance.exports.f1)) {
    instance.exports.f1();
  }
  // Execute {f2} once, so that the module knows that this is a used function.
  instance.exports.f2();
  const buff = %SerializeWasmModule(module);
  return buff;
};

const serialized_module = serializeModule();
// Do some GCs to make sure the first module got collected and removed from the
// module cache.
gc();
gc();
gc();

(function testSerializedModule() {
  print(arguments.callee.name);
  const module = %DeserializeWasmModule(serialized_module, wire_bytes);

  const instance = new WebAssembly.Instance(module, {foo: {bar: () => 1}});

  assertTrue(%IsTurboFanFunction(instance.exports.f1));
  assertTrue(%IsLiftoffFunction(instance.exports.f2));
  assertTrue(
      !%IsLiftoffFunction(instance.exports.f0) &&
      !%IsTurboFanFunction(instance.exports.f0));
})();
