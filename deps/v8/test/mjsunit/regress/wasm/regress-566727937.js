// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verify that Wasm memory deserialization fails before adding an incomplete
// WasmMemoryObject to the ID map when no delegate is present.
const stream = [
  0xFF, 13,
  0x41, 0x02,
  0x42, 0,
  0x56, 0x62, 0, 0,
  0x44,
  0x5E, 0x04,
  0x24, 0x00, 0x02,
  0x00, 0x00, 0x00,
  0x6D, 0x00, 0x00
];
const bytes = new Uint8Array(stream);
assertThrows(
    () => d8.serializer.deserialize(bytes.buffer), Error,
    'Unable to deserialize cloned data.');
