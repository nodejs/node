// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test needs --wasm-tier-up because we can't serialize and deserialize
// Liftoff code.
// Flags: --allow-natives-syntax --experimental-wasm-reftypes --wasm-tier-up

load("test/mjsunit/wasm/wasm-module-builder.js");

(function testExternRefNull() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_r_v)
      .addBody([kExprRefNull, kWasmExternRef])
      .exportFunc();

  var wire_bytes = builder.toBuffer();
  var module = new WebAssembly.Module(wire_bytes);
  var buffer = %SerializeWasmModule(module);
  module = %DeserializeWasmModule(buffer, wire_bytes);
  var instance = new WebAssembly.Instance(module);

  assertEquals(null, instance.exports.main());
})();

(function testExternRefIsNull() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('main', kSig_i_r)
      .addBody([kExprLocalGet, 0, kExprRefIsNull])
      .exportFunc();

  var wire_bytes = builder.toBuffer();
  var module = new WebAssembly.Module(wire_bytes);
  var buffer = %SerializeWasmModule(module);
  module = %DeserializeWasmModule(buffer, wire_bytes);
  var instance = new WebAssembly.Instance(module);

  assertEquals(0, instance.exports.main({'hello' : 'world'}));
  assertEquals(0, instance.exports.main(1234));
  assertEquals(0, instance.exports.main(0));
  assertEquals(0, instance.exports.main(123.4));
  assertEquals(0, instance.exports.main(undefined));
  assertEquals(1, instance.exports.main(null));
  assertEquals(0, instance.exports.main(print));
})();
