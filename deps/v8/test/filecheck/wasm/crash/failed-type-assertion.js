// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enable the validation (turbofan only):
// Flags: --wasm-assert-types --no-liftoff
// Type confusion at home:
// Flags: --experimental-wasm-ref-cast-nop
// CHECK: V8 is running with an unsupported configuration.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let structI32 = builder.addStruct([makeField(kWasmI32, true)]);
let structAny = builder.addStruct([makeField(kWasmAnyRef, true)]);
let structStruct = builder.addStruct([makeField(wasmRefType(structI32), true)]);
builder.addFunction("createStructAny", makeSig([kWasmAnyRef], [kWasmAnyRef]))
.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprStructNew, structAny,
]).exportFunc();

builder.addFunction("main", makeSig([kWasmAnyRef], [kWasmI32]))
.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprRefCastNop, structStruct,
  kGCPrefix, kExprStructGet, structStruct, 0,
  kGCPrefix, kExprStructGet, structI32, 0,
]).exportFunc();

let wasm = builder.instantiate().exports;
// CHECK: Wasm type assertion violation
wasm.main(wasm.createStructAny(123));
assertUnreachable();
