// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestS128InSignatureThrows() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('foo', kSig_s_i)
    .addBody([
      kExprLocalGet, 0,
      kSimdPrefix,
      kExprI32x4Splat])
    .exportFunc()
  const instance = builder.instantiate();
  assertThrows(() => instance.exports.foo(33), TypeError);
})();

(function TestS128ParamInSignatureThrows() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction('foo', kSig_i_s)
      .addBody([
          kExprLocalGet, 0,
          kSimdPrefix,
          kExprI32x4ExtractLane, 1])
      .exportFunc();
  const instance = builder.instantiate();
  assertThrows(() => instance.exports.foo(10), TypeError);
})();

(function TestImportS128Return() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addImport('', 'f', makeSig([], [kWasmS128]));
  builder.addFunction('foo', kSig_v_v)
      .addBody([kExprCallFunction, 0, kExprDrop])
      .exportFunc();
  const instance = builder.instantiate({'': {f: _ => 1}});
  assertThrows(() => instance.exports.foo(), TypeError);
})();

(function TestS128ImportThrows() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_index = builder.addType(kSig_i_i);
  let sig_s128_index = builder.addType(kSig_i_s);
  let index = builder.addImport('', 'func', sig_s128_index);
  builder.addFunction('foo', sig_index)
    .addBody([
      kExprLocalGet, 0,
      kSimdPrefix,
      kExprI32x4Splat,
      kExprCallFunction, index])
    .exportFunc();
  const instance = builder.instantiate({'': {func: _ => {}}});
  assertThrows(() => instance.exports.foo(14), TypeError);
})();

(function TestS128GlobalConstructor() {
  assertThrows(() => new WebAssembly.Global({value: 'v128'}), TypeError);
})();
