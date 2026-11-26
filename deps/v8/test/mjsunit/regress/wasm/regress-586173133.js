// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-fast-api --wasm-fast-api --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const fastApi = new d8.test.FastCAPI();
const boundCallToNumber = Function.prototype.call.bind(fastApi.call_to_number);

(function TestAnyRefNullParam() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmExternRef, kWasmAnyRef], []));
  builder.addFunction('run', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kAnyRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  instance.exports.run(fastApi);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestFuncRefParam() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmExternRef, kWasmFuncRef], []));
  const runNull = builder.addFunction('runNull', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kFuncRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();
  builder.addDeclarativeElementSegment([runNull.index]);
  builder.addFunction('runFunc', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefFunc, runNull.index,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  instance.exports.runNull(fastApi);
  instance.exports.runFunc(fastApi);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestConcreteStructRefParam() {
  const builder = new WasmModuleBuilder();
  const structType = builder.addStruct([makeField(kWasmI32, true)]);
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmExternRef, wasmRefNullType(structType)], []));
  builder.addFunction('runNull', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, structType,
        kExprCallFunction, imp,
      ])
      .exportFunc();
  builder.addFunction('runStruct', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNewDefault, structType,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  instance.exports.runNull(fastApi);
  assertThrows(() => instance.exports.runStruct(fastApi), TypeError);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestNullExternRefParam() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmExternRef, kWasmNullExternRef], []));
  builder.addFunction('run', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kNullExternRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  instance.exports.run(fastApi);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestAnyRefReceiver() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmAnyRef, kWasmExternRef], []));
  builder.addFunction('runNull', makeSig([], []))
      .addBody([
        kExprRefNull, kAnyRefCode,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();
  builder.addFunction('runConverted', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  assertThrows(() => instance.exports.runNull(), TypeError);
  instance.exports.runConverted(fastApi);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestFuncRefNullReceiver() {
  const builder = new WasmModuleBuilder();
  const imp = builder.addImport(
      'm', 'f', makeSig([kWasmFuncRef, kWasmExternRef], []));
  builder.addFunction('run', makeSig([], []))
      .addBody([
        kExprRefNull, kFuncRefCode,
        kExprRefNull, kExternRefCode,
        kExprCallFunction, imp,
      ])
      .exportFunc();

  const instance = builder.instantiate({m: {f: boundCallToNumber}});
  fastApi.reset_counts();
  assertThrows(() => instance.exports.run(), TypeError);
  assertEquals(0, fastApi.fast_call_count());
})();

(function TestExternRefAndNonNullExternRefStillWork() {
  const builder = new WasmModuleBuilder();
  const nonNullExtern = wasmRefType(kWasmExternRef);
  const impNullable = builder.addImport(
      'm', 'fNullable', makeSig([kWasmExternRef, kWasmExternRef], []));
  const impNonNull = builder.addImport(
      'm', 'fNonNull', makeSig([nonNullExtern, nonNullExtern], []));
  builder.addFunction('runNullable', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, kAnyRefCode,
        kGCPrefix, kExprExternConvertAny,
        kExprCallFunction, impNullable,
      ])
      .exportFunc();
  builder.addFunction('runNonNull', makeSig([nonNullExtern, nonNullExtern], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprCallFunction, impNonNull,
      ])
      .exportFunc();

  const instance = builder.instantiate({
    m: {fNullable: boundCallToNumber, fNonNull: boundCallToNumber},
  });
  instance.exports.runNullable(fastApi);
  instance.exports.runNonNull(fastApi, 42);
})();
