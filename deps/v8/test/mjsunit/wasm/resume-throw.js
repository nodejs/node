// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_v_v = builder.addType(kSig_v_v);
let cont_index = builder.addCont(sig_v_v);
let suspend_tag_index = builder.addTag(kSig_v_v);
let throw_tag_index = builder.addTag(kSig_v_i);
let sig_i_c = makeSig([wasmRefNullType(cont_index)], [kWasmI32]);
const kReturn = 12;
const kSuspend = 34;
const kThrow = 56;
const kExceptionValue = 78;
let suspend = builder.addFunction("suspend", kSig_v_v)
.addBody([
    kExprSuspend, suspend_tag_index,
]).exportFunc();
let assert_equal = builder.addFunction("assert_equal", kSig_v_ii)
.addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32Eq,
    kExprBrIf, 0,
    kExprUnreachable,
]);
let try_suspend = builder.addFunction("try_suspend", kSig_v_v)
.addBody([
    kExprBlock, kWasmI32,
      kExprTryTable, kWasmVoid, 1,
      kCatchNoRef, throw_tag_index, 0,
        kExprSuspend, suspend_tag_index,
      kExprEnd,
      kExprUnreachable,
    kExprEnd,
    kExprI32Const, kExceptionValue,
    kExprCallFunction, assert_equal.index,
]).exportFunc();
let suspend_twice = builder.addFunction("suspend_twice", kSig_v_v)
.addBody([
    kExprBlock, kWasmI32,
      kExprTryTable, kWasmVoid, 1,
      kCatchNoRef, throw_tag_index, 0,
        kExprSuspend, suspend_tag_index,
      kExprEnd,
      kExprUnreachable,
    kExprEnd,
    kExprI32Const, kExceptionValue,
    kExprCallFunction, assert_equal.index,
    kExprSuspend, suspend_tag_index,
]).exportFunc();
let resume_throw = builder.addFunction("resume_throw", sig_i_c)
    .addBody([
        kExprBlock, kWasmI32,
          kExprTryTable, kWasmVoid, 1,
          kCatchNoRef, throw_tag_index, 0,
            kExprBlock, kWasmRef, cont_index,
              kExprI32Const, kExceptionValue,
              kExprLocalGet, 0,
              kExprResumeThrow, cont_index, throw_tag_index, 1,
              kOnSuspend, suspend_tag_index, 0,
              kExprI32Const, kReturn,
              kExprReturn,
            kExprEnd,
            kExprDrop,
            kExprI32Const, kSuspend,
            kExprReturn,
          kExprEnd,
          kExprUnreachable,
        kExprEnd,
        kExprI32Const, kExceptionValue,
        kExprCallFunction, assert_equal.index,
        kExprI32Const, kThrow,
    ]).exportFunc();
builder.addFunction("resume_throw_cont_new", kSig_i_v)
    .addBody([
        kExprRefFunc, suspend.index,
        kExprContNew, cont_index,
        kExprCallFunction, resume_throw.index,
    ]).exportFunc();
// Check that a new cont is invalidated after resume_throw.
builder.addFunction("resume_throw_cont_new_bad", kSig_i_v)
    .addLocals(wasmRefNullType(cont_index), 1)
    .addBody([
        kExprBlock, kWasmI32,
          kExprTryTable, kWasmVoid, 1,
          kCatchNoRef, throw_tag_index, 0,
            kExprI32Const, kExceptionValue,
            kExprRefFunc, suspend.index,
            kExprContNew, cont_index,
            kExprLocalTee, 0,
            kExprResumeThrow, cont_index, throw_tag_index, 0,
          kExprEnd,
          kExprUnreachable,
        kExprEnd,
        kExprLocalGet, 0,
        kExprResume, cont_index, 0,
    ]).exportFunc();
builder.addFunction("resume_throw_suspended_uncaught", kSig_i_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprRefFunc, suspend.index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, suspend_tag_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprCallFunction, resume_throw.index,
    ]).exportFunc();
builder.addFunction("resume_throw_suspended_caught", kSig_i_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprRefFunc, try_suspend.index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, suspend_tag_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprCallFunction, resume_throw.index,
    ]).exportFunc();
builder.addFunction("resume_throw_resuspend", kSig_i_v)
    .addBody([
        kExprBlock, kWasmRef, cont_index,
          kExprRefFunc, suspend_twice.index,
          kExprContNew, cont_index,
          kExprResume, cont_index, 1, kOnSuspend, suspend_tag_index, 0,
          kExprUnreachable,
        kExprEnd,
        kExprCallFunction, resume_throw.index,
    ]).exportFunc();

let instance = builder.instantiate();
assertEquals(kThrow, instance.exports.resume_throw_cont_new());
assertThrows(instance.exports.resume_throw_cont_new_bad,
    WebAssembly.RuntimeError, /resuming an invalid continuation/);
assertEquals(kThrow, instance.exports.resume_throw_suspended_uncaught());
assertEquals(kReturn, instance.exports.resume_throw_suspended_caught());
assertEquals(kSuspend, instance.exports.resume_throw_resuspend());
