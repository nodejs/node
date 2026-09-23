// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let b = new WasmModuleBuilder();
let $s = b.addStruct([makeField(kWasmI32, true)]);
b.addFunction('mk', makeSig([kWasmI32], [kWasmExternRef])).addBody([
  kExprLocalGet, 0, kGCPrefix, kExprStructNew, $s,
  kGCPrefix, kExprExternConvertAny,
]).exportFunc();
b.addFunction('get', makeSig([kWasmExternRef], [kWasmI32])).addBody([
  kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, $s, kGCPrefix, kExprStructGet, $s, 0,
]).exportFunc();
b.addFunction('set', makeSig([kWasmExternRef, kWasmI32], [])).addBody([
  kExprLocalGet, 0, kGCPrefix, kExprAnyConvertExtern,
  kGCPrefix, kExprRefCast, $s, kExprLocalGet, 1,
  kGCPrefix, kExprStructSet, $s, 0,
]).exportFunc();
const w = b.instantiate().exports;

// Megamorphic callbacks — keeps kJSCall in TurboFan graph (not inlined).
function f1(r) { w.set(r, 999); }
function f2(r) { w.set(r, 999); }
function f3(r) { w.set(r, 999); }
function f4(r) { w.set(r, 999); }
function f5(r) { w.set(r, 999); }
let fs = [f1, f2, f3, f4, f5];

function victim(ref, cb) {
  let a = w.get(ref);  // WasmStructGet — value cached.
  cb(ref);             // kJSCall — should invalidate cache, but doesn't.
  let b = w.get(ref);  // WasmStructGet — eliminated, uses stale value.
  return b - a;        // correct: 957, buggy: 0.
}

let s = w.mk(42);

// Before optimization (interpreter).
%PrepareFunctionForOptimization(victim);
w.set(s, 42);
let before = victim(s, f1);
assertEquals(before, 957);

// Megamorphic warmup.
for (let i = 0; i < 500; i++) { w.set(s, 42); victim(s, fs[i % 5]); }

// After optimization (TurboFan + JS-to-Wasm inlining).
%OptimizeFunctionOnNextCall(victim);
w.set(s, 42);
let after = victim(s, f1);
assertEquals(after, 957);
