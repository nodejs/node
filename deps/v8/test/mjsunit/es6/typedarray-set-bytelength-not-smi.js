// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --mock-arraybuffer-allocator

(function TestBufferByteLengthNonSmi() {
  const source_buffer_length = %MaxSmi() + 1;
  const source_buffer = new ArrayBuffer(source_buffer_length);
  const source = new Uint16Array(source_buffer);
  assertEquals(source_buffer_length, source_buffer.byteLength);
  assertEquals(source_buffer_length / 2, source.length);

  const target_buffer_length = %MaxSmi() - 1;
  const target_buffer = new ArrayBuffer(target_buffer_length);
  const target = new Uint16Array(target_buffer);
  assertEquals(target_buffer_length, target_buffer.byteLength);
  assertEquals(target_buffer_length / 2, target.length);

  assertThrows(() => target.set(source), RangeError);
})();
