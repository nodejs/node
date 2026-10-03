// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sandbox-testing

function CreateRAB(n, m) { return new ArrayBuffer(n, {maxByteLength: m}); }
function foo(val) { return val.length; }
let int8Arr = new Int8Array(new ArrayBuffer(16));
let rabInt32 = new Int32Array(CreateRAB(16, 40));
%PrepareFunctionForOptimization(foo);
foo(int8Arr);
foo(rabInt32);
new ArrayBuffer(4).transfer();
%OptimizeFunctionOnNextCall(foo);
foo(int8Arr);
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const mapAddr = mem.getUint32(Sandbox.getAddressOf(int8Arr), true) & ~1;
const bf2Off = mapAddr + 0x0B;
const origBf2 = mem.getUint8(bf2Off);
mem.setUint8(bf2Off, origBf2 & 0x03);
foo(int8Arr);
