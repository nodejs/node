// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

// Regression test for crbug.com/588266445: d8.serializer.deserialize must copy
// in-sandbox ArrayBuffer bytes to trusted memory before passing them to
// v8::ValueDeserializer, so concurrent in-sandbox worker writes cannot mutate
// UTF-8 property keys between length computation and decoding.

const kPayloadLength = 64 * 1024;
// Wire format v16: kBeginJSObject (0x6f), kUtf8String (0x53), varint(65536).
const kPrefix = [0xff, 0x10, 0x6f, 0x53, 0x80, 0x80, 0x04];
// Value kInt32(1) (0x49, 0x02), kEndJSObject(1) (0x7b, 0x01).
const kSuffix = [0x49, 0x02, 0x7b, 0x01];

const serialized =
    new ArrayBuffer(kPrefix.length + kPayloadLength + kSuffix.length);
const bytes = new Uint8Array(serialized);
bytes.set(kPrefix, 0);
for (let i = 0; i < kPayloadLength; i += 2) {
  bytes[kPrefix.length + i] = 0xc2;
  bytes[kPrefix.length + i + 1] = 0xa2;
}
bytes.set(kSuffix, kPrefix.length + kPayloadLength);

const kTypedArrayType = Sandbox.getInstanceTypeIdFor('JS_TYPED_ARRAY_TYPE');
const kExternalPointerOffset =
    Sandbox.getFieldOffset(kTypedArrayType, 'external_pointer');
const cageView = new DataView(new Sandbox.MemoryView(0, 0x100000000));
const backingStoreOffset = Number(
    cageView.getBigUint64(
        Sandbox.getAddressOf(bytes) + kExternalPointerOffset, true) >>
    24n);
const payloadOffset = backingStoreOffset + kPrefix.length;

const control = new SharedArrayBuffer(8);
const state = new Int32Array(control);

const worker = new Worker(function() {
  onmessage = function(e) {
    const payload = new Uint16Array(
        new Sandbox.MemoryView(e.data.payloadOffset, e.data.payloadLength));
    const state = new Int32Array(e.data.control);
    Atomics.store(state, 0, 1);
    Atomics.notify(state, 0);
    while (Atomics.load(state, 1) === 0) {
      payload.fill(0x4141);  // Two 1-byte ASCII chars ('AA').
      payload.fill(0xa2c2);  // One 2-byte UTF-8 char (U+00A2).
    }
  };
}, {type: 'function'});

worker.postMessage({payloadOffset, payloadLength: kPayloadLength, control});
while (Atomics.load(state, 0) === 0) {
  Atomics.wait(state, 0, 0, 100);
}

try {
  for (let i = 0; i < 1000; i++) {
    try {
      const obj = d8.serializer.deserialize(serialized);
      assertEquals(1, Object.keys(obj).length);
    } catch (e) {
      // Torn UTF-8/wire edits are allowed to fail deserialization cleanly.
    }
  }
} finally {
  Atomics.store(state, 1, 1);
  worker.terminate();
}
