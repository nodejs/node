// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --mock-arraybuffer-allocator

(function TestBufferByteLengthNonSmi() {
  var non_smi_byte_length = %_MaxSmi() + 1;

  var buffer = new ArrayBuffer(non_smi_byte_length);

  var arr = new Uint16Array(buffer);

  assertEquals(non_smi_byte_length, arr.byteLength);
  assertEquals(non_smi_byte_length / 2, arr.length);

  arr = new Uint32Array(buffer);
  assertEquals(non_smi_byte_length, arr.byteLength);
  assertEquals(non_smi_byte_length / 4, arr.length);
})();

(function TestByteOffsetNonSmi() {
  var non_smi_byte_length = %_MaxSmi() + 11;

  var buffer = new ArrayBuffer(non_smi_byte_length);

  var whole = new Uint16Array(buffer);

  assertEquals(non_smi_byte_length, whole.byteLength);
  assertEquals(non_smi_byte_length / 2, whole.length);

  var arr = new Uint16Array(buffer, non_smi_byte_length - 10, 5);

  assertEquals(non_smi_byte_length, arr.buffer.byteLength);
  assertEquals(10, arr.byteLength);
  assertEquals(5, arr.length);
})();
