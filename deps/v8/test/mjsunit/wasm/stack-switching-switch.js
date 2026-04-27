// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_v_v = builder.addCont(sig_v_v);
// Target of switch must take a continuation as its last parameter.
let sig_v_c = builder.addType(makeSig([wasmRefType(cont_v_v)], []));
let cont_v_c = builder.addCont(sig_v_c);
let tag0_index = builder.addTag(kSig_v_v);

let nop = builder.addFunction("nop", sig_v_c)
    .addBody([]).exportFunc();

let switch_tag0 = builder.addFunction("switch_tag0", kSig_v_v)
    .addBody([
        kExprRefFunc, nop.index,
        kExprContNew, cont_v_c,
        kExprSwitch, cont_v_c, tag0_index
    ]).exportFunc();

builder.addFunction("resume_switch", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_v_v,
          kExprRefFunc, switch_tag0.index,
          kExprContNew, cont_v_v,
          kExprResume, cont_v_v, 1,
            kOnSwitch, tag0_index,
          kExprReturn,
        kExprEnd,
        kExprDrop,
    ]).exportFunc();

builder.addFunction("resume_suspend_handle_switch", kSig_v_v)
    .addBody([
        kExprBlock, kWasmRef, cont_v_v,
          kExprRefFunc, switch_tag0.index,
          kExprContNew, cont_v_v,
          kExprResume, cont_v_v, 1,
            kOnSuspend, tag0_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprDrop,
    ]).exportFunc();

let instance = builder.instantiate();

(function TestSwitchSuccess() {
  print(arguments.callee.name);
  instance.exports.resume_switch();
})();

(function TestSwitchNoHandler() {
  print(arguments.callee.name);
  // resume_suspend_handle_switch only has an on_suspend handler.
  // The switch instruction should not be caught by it.
  assertThrows(() => instance.exports.resume_suspend_handle_switch(),
               WebAssembly.SuspendError, /WasmFX: unhandled suspend/);
})();

(function TestSwitchArgs() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_i_v = builder.addType(kSig_i_v);
  let cont_i_v = builder.addCont(sig_i_v);
  let sig_v_v = builder.addType(kSig_v_v);
  let cont_v_v = builder.addCont(sig_v_v);
  let sig_i_c = builder.addType(makeSig([kWasmI32, wasmRefType(cont_v_v)], []));
  let cont_i_c = builder.addCont(sig_i_c);
  let types = [kWasmI32, kWasmI64, kWasmF32, kWasmF64, kWasmEqRef];
  // Target takes types + return continuation (cont_i_c).
  let sig_target = builder.addType(makeSig(types.concat([wasmRefType(cont_i_c)]), []));
  let cont_target = builder.addCont(sig_target);
  let tag_switch = builder.addTag(makeSig(types, []));
  let g_ref = builder.addGlobal(kWasmEqRef, true).exportAs('g_ref');

  let target = builder.addFunction("target", sig_target)
      .addBody([
          kExprLocalGet, 0, ...wasmI32Const(10), kExprI32Eq,
          kExprLocalGet, 1, ...wasmI64Const(20n), kExprI64Eq, kExprI32And,
          kExprLocalGet, 2, ...wasmF32Const(3.0), kExprF32Eq, kExprI32And,
          kExprLocalGet, 3, ...wasmF64Const(4.0), kExprF64Eq, kExprI32And,
          kExprLocalGet, 4, kExprGlobalGet, g_ref.index, kExprRefEq, kExprI32And,
          kExprIf, kWasmVoid,
          kExprElse,
            kExprUnreachable,
          kExprEnd,
          kExprI32Const, 42,
          kExprLocalGet, 5, // The return continuation (cont_i_c)
          kExprSwitch, cont_i_c, tag_switch,
          kExprUnreachable,
      ]).exportFunc();

  let switch_func = builder.addFunction("switch_func", sig_i_v)
      .addBody([
          ...wasmI32Const(10),
          ...wasmI64Const(20n),
          ...wasmF32Const(3.0),
          ...wasmF64Const(4.0),
          kExprGlobalGet, g_ref.index,
          kExprRefFunc, target.index,
          kExprContNew, cont_target,
          kExprSwitch, cont_target, tag_switch,
          // switch pushes the parameters of the return continuation: i32, cont
          kExprDrop, // drop the dummy cont
          kExprReturn,
      ]).exportFunc();

  builder.addFunction("main", kSig_i_v)
      .addBody([
          kExprRefFunc, switch_func.index,
          kExprContNew, cont_i_v,
          kExprResume, cont_i_v, 1,
            kOnSwitch, tag_switch,
      ]).exportFunc();

  let ref = 123;
  let instance = builder.instantiate();
  instance.exports.g_ref.value = ref;
  assertEquals(42, instance.exports.main());
})();

(function TestSwitchBackIntercepted() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_v = builder.addType(kSig_v_v);
  let cont_v_v = builder.addCont(sig_v_v);
  // Target of switch must take a continuation as its last parameter.
  let sig_v_c = builder.addType(makeSig([wasmRefNullType(cont_v_v)], []));
  let cont_v_c = builder.addCont(sig_v_c);
  // Target for switch back: must also take a continuation.
  let sig_v_c2 = builder.addType(makeSig([wasmRefNullType(cont_v_c)], []));
  let cont_v_c2 = builder.addCont(sig_v_c2);

  let tag0 = builder.addTag(kSig_v_v);
  let g = builder.addGlobal(kWasmI32, true).exportAs("g");

  let switcher = builder.addFunction("switcher", sig_v_c2)
      .addBody([
          kExprI32Const, 1,
          kExprGlobalSet, g.index,
          kExprLocalGet, 0, // return cont (type cont_v_c)
          kExprSwitch, cont_v_c, tag0,
          kExprUnreachable,
      ]).exportFunc();

  let switch_func = builder.addFunction("switch_func", sig_v_c)
      .addBody([
          kExprLocalGet, 0, kExprDrop,
          kExprRefFunc, switcher.index,
          kExprContNew, cont_v_c2,
          kExprSwitch, cont_v_c2, tag0,
          kExprDrop,
      ]).exportFunc();

  builder.addFunction("main", kSig_v_v)
      .addBody([
          kExprRefNull, cont_v_v,
          kExprRefFunc, switch_func.index,
          kExprContNew, cont_v_c,
          kExprResume, cont_v_c, 1,
            kOnSwitch, tag0,
      ]).exportFunc();

  let instance = builder.instantiate();
  instance.exports.g.value = 0;
  instance.exports.main();
  assertEquals(1, instance.exports.g.value);
})();

(function TestSwitchToSameStack() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sig_v_v = builder.addType(kSig_v_v);
  let cont_v_v = builder.addCont(sig_v_v);
  let sig_v_c = builder.addType(makeSig([wasmRefNullType(cont_v_v)], []));
  let cont_v_c = builder.addCont(sig_v_c);
  let tag0 = builder.addTag(kSig_v_v);

  let g_cont = builder.addGlobal(wasmRefNullType(cont_v_c), true);

  let switcher = builder.addFunction("switcher", sig_v_c)
      .addBody([
          kExprLocalGet, 0,
          kExprGlobalGet, g_cont.index,
          kExprSwitch, cont_v_c, tag0,
          kExprDrop,
      ]).exportFunc();

  builder.addFunction("main", kSig_v_v)
      .addBody([
          kExprRefFunc, switcher.index,
          kExprContNew, cont_v_c,
          kExprGlobalSet, g_cont.index,
          kExprRefNull, cont_v_v,
          kExprGlobalGet, g_cont.index,
          kExprResume, cont_v_c, 0,
      ]).exportFunc();

  let instance = builder.instantiate();
  // Switching to a continuation that is already on the stack should fail.
  assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError, /resuming an invalid continuation/);
})();
