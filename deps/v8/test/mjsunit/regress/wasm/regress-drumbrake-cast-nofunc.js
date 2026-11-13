// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless --jitless

// Regression test for a bug in the Wasm interpreter, which decoded a function's
// local declarations without passing a module to DecodeLocalDecls, which left
// indexed reference-type locals with a default RefTypeKind::kOther.
// When the bytecode generator later ran a subtyping check for a ref.cast whose
// operand was such a local, UpcastToStandardType() hit UNREACHABLE.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(kSig_v_v);

builder.addFunction('w0', kSig_v_v)
    // local 0: (ref null func)
    .addLocals(wasmRefNullType(kWasmFuncRef), 1)
    // local 1: (ref null $sig)
    .addLocals(wasmRefNullType(sig), 1)
    // local 2: (ref null nofunc)
    .addLocals(wasmRefNullType(kWasmNullFuncRef), 1)
    .addBody([
      kExprRefNull, kFuncRefCode,
      kExprLocalSet, 0,
      kExprLocalGet, 0,
      kGCPrefix, kExprRefCastNull, sig,
      kExprLocalSet, 1,
      kExprLocalGet, 1,
      kGCPrefix, kExprRefCastNull, kNullFuncRefCode,
      kExprLocalSet, 2,
    ])
    .exportFunc();

const instance = builder.instantiate();
instance.exports.w0();
