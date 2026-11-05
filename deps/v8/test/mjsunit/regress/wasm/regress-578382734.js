// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestPromisingWrapperNotExportedFunction() {
  const builder = new WasmModuleBuilder();
  const fn = builder.addFunction('f', kSig_i_v)
      .addBody([kExprI32Const, 42])
      .exportFunc();
  builder.addDeclarativeElementSegment([fn.index]);
  builder.addFunction('get_f', makeSig([], [kWasmFuncRef]))
      .addBody([kExprRefFunc, fn.index])
      .exportFunc();
  builder.addFunction('accept_funcref', makeSig([kWasmFuncRef], []))
      .addBody([])
      .exportFunc();
  builder.addFunction('id_externref', makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([kExprLocalGet, 0])
      .exportFunc();
  const inst = builder.instantiate();
  const promising = WebAssembly.promising(inst.exports.f);

  assertThrows(
      () => WebAssembly.promising(promising), TypeError,
      /Argument 0 must be a WebAssembly exported function/);

  const table = new WebAssembly.Table({element: 'anyfunc', initial: 1});
  assertThrows(() => table.set(0, promising), TypeError);
  assertThrows(() => table.grow(1, promising), TypeError);
  assertThrows(
      () => new WebAssembly.Table({element: 'anyfunc', initial: 1}, promising),
      TypeError);

  assertThrows(
      () => new WebAssembly.Global({value: 'anyfunc', mutable: true}, promising),
      TypeError);
  const global = new WebAssembly.Global({value: 'anyfunc', mutable: true});
  assertThrows(() => { global.value = promising; }, TypeError);

  assertThrows(() => inst.exports.accept_funcref(promising), TypeError);
  assertSame(promising, inst.exports.id_externref(promising));
  assertSame(inst.exports.f, inst.exports.get_f());
})();

(function TestPromisingWrapperAsWasmImport() {
  const builder1 = new WasmModuleBuilder();
  const counter = builder1.addGlobal(kWasmI32, true, false).exportAs('count');
  builder1.addFunction('f', kSig_i_v)
      .addBody([
        kExprGlobalGet, counter.index,
        kExprI32Const, 1,
        kExprI32Add,
        kExprGlobalSet, counter.index,
        kExprI32Const, 42,
      ])
      .exportFunc();
  const inst1 = builder1.instantiate();
  const promising = WebAssembly.promising(inst1.exports.f);

  // Importing a promising wrapper as () -> externref treats it as a JS
  // function returning a Promise, rather than failing with LinkError.
  const builderExtern = new WasmModuleBuilder();
  const impExtern = builderExtern.addImport('env', 'p', kSig_r_v);
  builderExtern.addFunction('call_p', kSig_r_v)
      .addBody([kExprCallFunction, impExtern])
      .exportFunc();
  const instExtern = builderExtern.instantiate({env: {p: promising}});
  const promise = instExtern.exports.call_p();
  assertInstanceof(promise, Promise);
  assertEquals(1, inst1.exports.count.value);
  assertPromiseResult(promise, v => assertEquals(42, v));

  // Importing a promising wrapper as () -> i32 calls the JS wrapper and
  // converts the returned Promise object to i32 (0), rather than taking a
  // direct Wasm-to-Wasm call returning 42. Re-exporting it creates a valid
  // Wasm exported function.
  const builderI32 = new WasmModuleBuilder();
  const impI32 = builderI32.addImport('env', 'p', kSig_i_v);
  builderI32.addExport('reexported_p', impI32);
  builderI32.addFunction('call_p', kSig_i_v)
      .addBody([kExprCallFunction, impI32])
      .exportFunc();
  const instI32 = builderI32.instantiate({env: {p: promising}});
  assertEquals(0, instI32.exports.call_p());
  assertEquals(2, inst1.exports.count.value);
  assertEquals(0, instI32.exports.reexported_p());
  assertEquals(3, inst1.exports.count.value);
  assertPromiseResult(
      WebAssembly.promising(instI32.exports.reexported_p)(),
      v => assertEquals(0, v));

  // Wrapping a promising wrapper in WebAssembly.Suspending and importing (and
  // re-exporting across modules) suspends and unwraps the Promise to 42.
  const suspending = new WebAssembly.Suspending(promising);
  const instSusp = builderI32.instantiate({env: {p: suspending}});
  assertPromiseResult(
      WebAssembly.promising(instSusp.exports.call_p)(),
      v => assertEquals(42, v));

  const builderChain = new WasmModuleBuilder();
  const impChain = builderChain.addImport('env', 'p', kSig_i_v);
  builderChain.addFunction('call_chain', kSig_i_v)
      .addBody([kExprCallFunction, impChain])
      .exportFunc();
  const instChain =
      builderChain.instantiate({env: {p: instSusp.exports.reexported_p}});
  assertPromiseResult(
      WebAssembly.promising(instChain.exports.call_chain)(),
      v => assertEquals(42, v));
})();

(function TestPromisingWrapperIncompatibleSignature() {
  const sigExn = makeSig([], [kWasmExnRef]);
  const builder1 = new WasmModuleBuilder();
  builder1.addFunction('f_exn', sigExn)
      .addBody([kExprRefNull, kExnRefCode])
      .exportFunc();
  const inst1 = builder1.instantiate();
  const promisingExn = WebAssembly.promising(inst1.exports.f_exn);

  assertThrows(() => promisingExn(), TypeError);
  assertThrows(
      () => WebAssembly.promising(promisingExn), TypeError,
      /Argument 0 must be a WebAssembly exported function/);

  const suspendingExn = new WebAssembly.Suspending(promisingExn);
  assertInstanceof(suspendingExn, WebAssembly.Suspending);

  const table = new WebAssembly.Table({element: 'anyfunc', initial: 1});
  assertThrows(() => table.set(0, promisingExn), TypeError);
  assertThrows(
      () => new WebAssembly.Global(
          {value: 'anyfunc', mutable: true}, promisingExn),
      TypeError);

  // Importing promisingExn with an incompatible signature must be treated as a
  // JS callable (throwing TypeError when called), not a direct Wasm-to-Wasm
  // call returning null exnref.
  const builder2 = new WasmModuleBuilder();
  const impExn = builder2.addImport('env', 'p', sigExn);
  builder2.addFunction('call_exn', kSig_i_v)
      .addBody([kExprCallFunction, impExn, kExprRefIsNull])
      .exportFunc();
  const inst2 = builder2.instantiate({env: {p: promisingExn}});
  assertThrows(() => inst2.exports.call_exn(), TypeError);
})();
