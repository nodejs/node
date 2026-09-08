// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --wasm-fast-api

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Regression test for chromium:532625494.
// When a function that return_calls a FastAPI import is inlined into a caller
// whose call site is inside a try block (mode_ == kInlinedWithCatch), the
// FastAPI value_out_of_range fallback emits a BuildWasmCall with
// kCatchInParentFrame. In unreachable code that call has an invalid OpIndex,
// and MaybeSetPositionToParent tried to attach a position to it, hitting the
// index.valid() DCHECK in the operation_origins sidetable.

const fast_c_api = new d8.test.FastCAPI();
const throwNoFallbackBound =
    Function.prototype.call.bind(fast_c_api.throw_no_fallback);

let builder = new WasmModuleBuilder();
let sig = makeSig([kWasmExternRef], [kWasmI32]);
let kFunc = builder.addImport('env', 'throw_no_fallback', sig);
// f: return_call the FastAPI import in tail position.
let f = builder.addFunction("f", sig)
  .addBody([
    kExprLocalGet, 0,
    kExprReturnCall, kFunc
  ]);
// g: calls f inside a try block, so the inlined call is kInlinedWithCatch.
let g = builder.addFunction("g", sig)
  .exportFunc()
  .addBody([
    kExprTry, kWasmI32,
      kExprLocalGet, 0,
      kExprCallFunction, f.index,
    kExprCatchAll,
      kExprI32Const, 42,
    kExprEnd
  ]);
let instance =
    builder.instantiate({ env: { throw_no_fallback: throwNoFallbackBound } });
%WasmTierUpFunction(instance.exports.g);
