// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --allow-natives-syntax
// Flags: --wasm-wrapper-tiering-budget=1
// Suppress Deoptimizer::ZapCode when marking `maglev_fn` for deoptimization:
// Flags: --maglev-inline-api-calls

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Canonical address > 4GB outside the sandbox (with HeapObject tag bit 0 set).
let u64 = new BigUint64Array([0x0000700012345679n]);
let target_float = new Float64Array(u64.buffer)[0];

// Populate WasmExportWrapperCache with a compiled (i32) -> i32 wrapper, while
// keeping `instance_tierup.exports.f` on the generic wrapper so we also cover
// Runtime_TierUpJSToWasmWrapper's cache lookup.
const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_i).exportFunc().addBody([kExprLocalGet, 0]);
const module = builder.toModule();
const instance1 = new WebAssembly.Instance(module);
const instance_tierup = new WebAssembly.Instance(module);
instance1.exports.f(42);

// Compile a Maglev function with matching JS parameter count (receiver + 1 arg)
// that spills `target_float` across a throwing `+obj` call inside `try`.
function maglev_fn(obj) {
  let v1 = obj.f1 + target_float;
  let v2 = obj.f2 + 2.2;
  let v3 = obj.f3 + 3.3;
  let v4 = obj.f4 + 4.4;
  let v5 = obj.f5 + 5.5;
  let v6 = obj.f6 + 6.6;
  let v7 = obj.f7 + 7.7;
  let v8 = obj.f8 + 8.8;
  let v9 = obj.f9 + 9.9;
  let v10 = obj.f10 + 10.1;
  let v11 = obj.f11 + 11.2;
  let v12 = obj.f12 + 12.3;
  let v13 = obj.f13 + 13.4;
  let v14 = obj.f14 + 14.5;
  let v15 = obj.f15 + 15.6;
  let target_obj = obj.target_obj;
  try {
    let num = +obj;
    target_obj.x = 42;
    return v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 +
           v14 + v15 + num;
  } catch (err) {
    return 0;
  }
}

let sample_obj = {
  f1: 0.0, f2: 2.2, f3: 3.3, f4: 4.4, f5: 5.5, f6: 6.6,
  f7: 7.7, f8: 8.8, f9: 9.9, f10: 10.1, f11: 11.2, f12: 12.3,
  f13: 13.4, f14: 14.5, f15: 15.6,
  target_obj: {x: 1},
  valueOf: () => 1.5,
};
%PrepareFunctionForOptimization(maglev_fn);
maglev_fn(sample_obj);
%OptimizeMaglevOnNextCall(maglev_fn);
maglev_fn(sample_obj);

// Read `maglev_fn`'s CodeWrapper from `sample_obj`'s Map::dependent_code (when
// Maglev optimization is enabled).
const kMapDependentCodeOffset = 28;
const kWeakArrayListLengthOffset = 8;
const kWeakArrayListDataOffset = 12;
const kCodeWrapperCodeOffset = 4;
const kSfiOffset = Sandbox.getFieldOffset(
    Sandbox.getInstanceTypeIdFor('JS_FUNCTION_TYPE'), 'shared_function_info');

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
let sample_map = memory.getUint32(Sandbox.getAddressOf(sample_obj), true) & ~3;
let dep_code =
    memory.getUint32(sample_map + kMapDependentCodeOffset, true) & ~3;
if (memory.getUint32(dep_code + kWeakArrayListLengthOffset, true) > 0) {
  let maglev_wrapper =
      memory.getUint32(dep_code + kWeakArrayListDataOffset, true) & ~3;
  let code_wrapper_map = memory.getUint32(maglev_wrapper, true);
  let maglev_code_handle =
      memory.getUint32(maglev_wrapper + kCodeWrapperCodeOffset, true);

  // Mark `maglev_fn`'s Code object for deoptimization.
  %DeoptimizeFunction(maglev_fn);

  // Overwrite all in-sandbox CodeWrappers on the old-space page(s) with the
  // deoptimized Maglev Code handle. Use `f`'s SharedFunctionInfo (allocated in
  // OLD_SPACE) rather than `f` itself (allocated in young space).
  let f_sfi = memory.getUint32(
      Sandbox.getAddressOf(instance1.exports.f) + kSfiOffset, true) & ~3;
  let pages = new Set([
    f_sfi & ~0x3ffff,
    maglev_wrapper & ~0x3ffff,
  ]);
  for (let page_start of pages) {
    for (let addr = page_start; addr < page_start + 0x40000 - 8; addr += 4) {
      if (memory.getUint32(addr, true) === code_wrapper_map) {
        memory.setUint32(
            addr + kCodeWrapperCodeOffset, maglev_code_handle, true);
      }
    }
  }
}

// Re-instantiating looks up the `(i32) -> i32` wrapper in
// WasmExportWrapperCache.
const instance2 = new WebAssembly.Instance(module);
const bad_f = instance2.exports.f;

let throwing_obj = {
  f1: 0.0, f2: 2.2, f3: 3.3, f4: 4.4, f5: 5.5, f6: 6.6,
  f7: 7.7, f8: 8.8, f9: 9.9, f10: 10.1, f11: 11.2, f12: 12.3,
  f13: 13.4, f14: 14.5, f15: 15.6,
  target_obj: {x: 1},
  valueOf() {
    throw 123;
  },
};

assertThrowsEquals(() => bad_f(throwing_obj), 123);
assertEquals(7, bad_f(7));

// Also trigger wrapper tier-up on `instance_tierup` via
// Runtime_TierUpJSToWasmWrapper.
instance_tierup.exports.f(42);
assertThrowsEquals(() => instance_tierup.exports.f(throwing_obj), 123);
assertEquals(9, instance_tierup.exports.f(9));
