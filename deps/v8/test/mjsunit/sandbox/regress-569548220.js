// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --trace-gc-object-stats --sandbox-testing

const memory = new DataView(new Sandbox.MemoryView(0, 2**32));
function setupFakeBytecodeArray() {
  const fakeObj = new Array().fill();
  const fakeAddr = Sandbox.getAddressOf(fakeObj);
  for (let addr = 0x800; addr < 0x10000; addr += 4) {
    if (Sandbox.getInstanceTypeOfObjectAt(addr) === "MAP_TYPE") {
      memory.setUint32(fakeAddr, addr | 1, true);
      if (Sandbox.getInstanceTypeOfObjectAt(fakeAddr) === "BYTECODE_ARRAY_TYPE") {
        break;
      }
    }
  }
}
setupFakeBytecodeArray();
gc();
