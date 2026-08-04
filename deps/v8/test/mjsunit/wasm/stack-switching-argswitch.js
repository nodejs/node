// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestSwitchF64Arg() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // Recursive type group for coroutines that pass f64 and the next continuation.
  builder.startRecGroup();
  let sig_idx = builder.addType(makeSig([kWasmF64, wasmRefNullType(builder.nextTypeIndex() + 1)], [kWasmF64]));
  let cont_idx = builder.addCont(sig_idx);
  builder.endRecGroup();

  let f64_tag = builder.addTag(makeSig([], [kWasmF64]));

  let target = builder.addFunction("target", sig_idx)
      .addBody([
          // Get the f64 argument, increment by 1.
          kExprLocalGet, 0,
          ...wasmF64Const(1.0),
          kExprF64Add,
          // Switch back to the caller.
          kExprLocalGet, 1, // The return continuation
          kExprSwitch, cont_idx, f64_tag,
          // When we are switched back to, we get [f64, switcher_cont]
          kExprDrop, // drop cont
          kExprReturn, // return f64
      ]).exportFunc();

  let switch_func = builder.addFunction("switch_func", sig_idx)
      .addBody([
          // Ignore initial arguments, switch to target with 42.0
          ...wasmF64Const(42.0),
          kExprRefFunc, target.index,
          kExprContNew, cont_idx,
          kExprSwitch, cont_idx, f64_tag,
          // When we are switched back to, we get [f64, switcher_cont]
          kExprDrop, // drop cont
          kExprReturn, // return f64
      ]).exportFunc();

  // Helper to check the f64 value and return i32.
  builder.addFunction("main", kSig_i_v)
      .addBody([
          // Initial f64 and null continuation for switch_func
          ...wasmF64Const(0.0),
          kExprRefNull, cont_idx,
          kExprRefFunc, switch_func.index,
          kExprContNew, cont_idx,
          // Resume switch_func.
          kExprResume, cont_idx, 1,
            kOnSwitch, f64_tag,
          // Result is on stack: f64. Check if it's 43.
          ...wasmF64Const(43.0),
          kExprF64Eq,
      ]).exportFunc();

  let instance = builder.instantiate();
  assertEquals(1, instance.exports.main());
})();

(function TestSwitchMultiArgF64() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let types = [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmF64];

  builder.startRecGroup();
  let sig_idx = builder.addType(makeSig(types.concat([wasmRefNullType(builder.nextTypeIndex() + 1)]), [kWasmF64]));
  let cont_idx = builder.addCont(sig_idx);
  builder.endRecGroup();

  let tag = builder.addTag(makeSig([], [kWasmF64]));

  let target = builder.addFunction("target", sig_idx)
      .addBody([
          // Check i32, i64, f32, f64.
          kExprLocalGet, 0, ...wasmI32Const(10), kExprI32Eq,
          kExprLocalGet, 1, ...wasmI64Const(20n), kExprI64Eq, kExprI32And,
          kExprLocalGet, 2, ...wasmF32Const(3.0), kExprF32Eq, kExprI32And,
          kExprLocalGet, 3, ...wasmF64Const(4.0), kExprF64Eq, kExprI32And,
          kExprIf, kWasmVoid,
          kExprElse,
            kExprUnreachable,
          kExprEnd,
          // Modify f64 (local 4) and switch back.
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kExprLocalGet, 2,
          kExprLocalGet, 3,
          kExprLocalGet, 4,
          ...wasmF64Const(1.0),
          kExprF64Add,
          kExprLocalGet, 5, // return cont
          kExprSwitch, cont_idx, tag,
          kExprDrop, kExprReturn,
      ]).exportFunc();

  let switch_func = builder.addFunction("switch_func", sig_idx)
      .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kExprLocalGet, 2,
          kExprLocalGet, 3,
          kExprLocalGet, 4,
          kExprRefFunc, target.index,
          kExprContNew, cont_idx,
          kExprSwitch, cont_idx, tag,
          kExprDrop, kExprReturn,
      ]).exportFunc();

  builder.addFunction("main", kSig_i_v)
      .addBody([
          ...wasmI32Const(10),
          ...wasmI64Const(20n),
          ...wasmF32Const(3.0),
          ...wasmF64Const(4.0),
          ...wasmF64Const(100.0),
          kExprRefNull, cont_idx,
          kExprRefFunc, switch_func.index,
          kExprContNew, cont_idx,
          kExprResume, cont_idx, 1,
            kOnSwitch, tag,
          // Result: f64. Should be 101.
          ...wasmF64Const(101.0),
          kExprF64Eq,
      ]).exportFunc();

  let instance = builder.instantiate();
  assertEquals(1, instance.exports.main());
})();

// Negative test: Tag return type does not match the supplied continuation's return type.
(function TestSwitchTagReturnMismatch() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.startRecGroup();
  let sig_idx = builder.addType(makeSig([kWasmF64, wasmRefNullType(builder.nextTypeIndex() + 1)], [kWasmF32])); // Cont returns f32
  let cont_idx = builder.addCont(sig_idx);
  builder.endRecGroup();

  let f64_tag = builder.addTag(makeSig([], [kWasmF64])); // Tag returns f64

  let target = builder.addFunction("target", sig_idx)
      .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kExprSwitch, cont_idx, f64_tag,
          kExprDrop, // drop cont
          kExprReturn, // return f32
      ]).exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError, /return\(s\) from continuation .* do not match tag/);
})();

// Negative test: Tag return type does not match the return continuation's return type.
(function TestSwitchTagReturnMismatchReturnCont() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // The return continuation returns f32
  let return_sig_idx = builder.addType(makeSig([kWasmF32, wasmRefNullType(0)], [kWasmF32]));
  let return_cont_idx = builder.addCont(return_sig_idx);

  builder.startRecGroup();
  let sig_idx = builder.addType(makeSig([kWasmF64, wasmRefNullType(return_cont_idx)], [kWasmF64])); // Cont returns f64
  let cont_idx = builder.addCont(sig_idx);
  builder.endRecGroup();

  let f64_tag = builder.addTag(makeSig([], [kWasmF64])); // Tag returns f64

  let target = builder.addFunction("target", sig_idx)
      .addBody([
          kExprLocalGet, 0,
          kExprLocalGet, 1,
          kExprSwitch, cont_idx, f64_tag, // f64_tag returns f64, but return_cont expects f32 (its return type)
          kExprDrop, // drop cont
          kExprReturn, // return f64
      ]).exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError, /tag .*'s return types should be a subtype of return continuation .*'s return types/);
})();
