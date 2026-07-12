// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wasmfx

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

const sig_v_v = builder.addType(kSig_v_v);
const cont_v_v = builder.addCont(sig_v_v);
const sig_v_ii = builder.addType(makeSig([kWasmI32, kWasmI32], []));
const cont_v_ii = builder.addCont(sig_v_ii);

const tag_more_returns =
    builder.addTag(makeSig([kWasmI32], [kWasmI32, kWasmI32]));

const kRet0 = 0x1111;
const kRet1 = 0x2222;
const kExpectedSum = (kRet0 + kRet1) | 0;

const block_sig =
    builder.addType(makeSig([], [kWasmI32, wasmRefType(cont_v_ii)]));

const suspend_more_returns = builder.addFunction("suspend_more_returns", kSig_v_v)
  .addBody([
    kExprI32Const, 6,
    kExprSuspend, tag_more_returns,
    kExprI32Add,
    ...wasmI32Const(kExpectedSum),
    kExprI32Eq,
    kExprBrIf, 0,
    kExprUnreachable,
  ])
  .exportFunc();

builder.addFunction("test", kSig_v_v)
  .addLocals(wasmRefType(cont_v_ii), 1)
  .addBody([
    kExprBlock, block_sig,
      kExprRefFunc, suspend_more_returns.index,
      kExprContNew, cont_v_v,
      kExprResume, cont_v_v, 1, kOnSuspend, tag_more_returns, 0,
      kExprUnreachable,
    kExprEnd,

    kExprLocalSet, 0,
    kExprDrop,

    ...wasmI32Const(kRet0),
    ...wasmI32Const(kRet1),
    kExprLocalGet, 0,
    kExprResume, cont_v_ii, 0,
  ])
  .exportFunc();

builder.instantiate().exports.test();
