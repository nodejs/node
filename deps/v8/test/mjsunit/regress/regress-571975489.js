// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --multi-mapped-mock-allocator

let buf;
try {
  buf = new ArrayBuffer(0x80000000 + 16);
} catch (e) {
  // 32-bit platforms cannot allocate > 2GB ArrayBuffers.
  quit(0);
}

const view = new Uint8Array(buf);

// SerializationTag::kBeginJSObject (0x6f), followed by
// SerializationTag::kUtf8String (0x53) property key with
// byte_length = 0x80000000.
view.set([0xff, 0x10, 0x6f, 0x53, 0x80, 0x80, 0x80, 0x80, 0x08]);
assertThrows(() => d8.serializer.deserialize(buf), RangeError);
