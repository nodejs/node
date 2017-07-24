// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestBufferByteLengthNonSmi() {
  var non_smi_byte_length = %_MaxSmi() + 1;

  try {
    var buffer = new ArrayBuffer(non_smi_byte_length);
  } catch (e) {
    // The ArrayBuffer allocation can fail on 32-bit archs, so no need to try to
    // construct the typed array.
    return;
  }
  var arr = new Uint16Array(buffer);

  assertEquals(non_smi_byte_length, arr.byteLength);
  assertEquals(non_smi_byte_length / 2, arr.length);

  arr = new Uint32Array(buffer);
  assertEquals(non_smi_byte_length, arr.byteLength);
  assertEquals(non_smi_byte_length / 4, arr.length);
})();

(function TestByteOffsetNonSmi() {
  var non_smi_byte_length = %_MaxSmi() + 11;
  try {
    var buffer = new ArrayBuffer(non_smi_byte_length);
  } catch (e) {
    // The ArrayBuffer allocation can fail on 32-bit archs, so no need to try to
    // construct the typed array.
    return;
  }
  print(buffer.byteLength);
  var whole = new Uint16Array(buffer);
  whole[non_smi_byte_length / 2 - 1] = 1;
  whole[non_smi_byte_length / 2 - 2] = 2;
  whole[non_smi_byte_length / 2 - 3] = 3;
  whole[non_smi_byte_length / 2 - 4] = 4;
  whole[non_smi_byte_length / 2 - 5] = 5;

  assertEquals(non_smi_byte_length / 2, whole.length);
  assertEquals(1, whole[non_smi_byte_length / 2 - 1]);

  var arr = new Uint16Array(buffer, non_smi_byte_length - 10, 5);

  assertEquals(non_smi_byte_length, arr.buffer.byteLength);
  assertEquals(10, arr.byteLength);
  assertEquals(5, arr.length);

  assertEquals(5, arr[0]);
  assertEquals(4, arr[1]);
  assertEquals(3, arr[2]);
  assertEquals(2, arr[3]);
  assertEquals(1, arr[4]);
})();
