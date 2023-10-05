// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Files: tools/clusterfuzz/foozzie/v8_mock.js

// Test foozzie typed-array-specific mocks for differential fuzzing.

// Max typed array length is mocked and restricted to 1MiB buffer.
const maxBytes = 1048576;

// The maximum also holds for array buffer and shared array buffer.
assertEquals(maxBytes, new ArrayBuffer(maxBytes + 1).byteLength);
assertEquals(maxBytes, new SharedArrayBuffer(maxBytes + 1).byteLength);

function testArrayType(type) {
  const name = type.name;
  const bytesPerElem = type.BYTES_PER_ELEMENT;
  const maxElem = maxBytes / bytesPerElem;

  function testLength(expectedLength, arr) {
    const expectedBytes = expectedLength * bytesPerElem;
    assertEquals(expectedBytes, arr.byteLength, name);
    assertEquals(expectedLength, arr.length, name);
  }

  // Test length argument in constructor.
  testLength(maxElem - 1, new type(maxElem - 1));
  testLength(maxElem, new type(maxElem));
  testLength(maxElem, new type(maxElem + 1));

  // Test buffer argument in constructor.
  // Unaligned offsets don't throw.
  const buffer = new ArrayBuffer(maxBytes);
  new type(buffer, 1);
  new type(buffer, 3);

  // Offsets work or are capped.
  function bytes(elements) {
    return elements * bytesPerElem;
  }
  testLength(maxElem, new type(buffer, 0));
  testLength(maxElem - 1, new type(buffer, bytes(1)));
  testLength(1, new type(buffer, bytes(maxElem - 1)));
  testLength(0, new type(buffer, bytes(maxElem)));
  testLength(0, new type(buffer, bytes(maxElem + 1)));

  // Offset and length work or are capped.
  testLength(1, new type(buffer, 0, 1));
  testLength(1, new type(buffer, bytesPerElem, 1));
  testLength(maxElem - 2, new type(buffer, bytes(1), maxElem - 2));
  testLength(maxElem - 1, new type(buffer, bytes(1), maxElem - 1));
  testLength(maxElem - 1, new type(buffer, bytes(1), maxElem));
  testLength(0, new type(buffer, bytes(maxElem - 1), 0));
  testLength(1, new type(buffer, bytes(maxElem - 1), 1));
  testLength(1, new type(buffer, bytes(maxElem - 1), 2));

  // Length works or is capped despite bogus offset.
  testLength(1, new type(buffer, "bad", 1));
  testLength(maxElem, new type(buffer, "bad", maxElem + 1));
  testLength(1, new type(buffer, {"x": 4}, 1));
  testLength(maxElem, new type(buffer, {"x": 4}, maxElem + 1));

  // Insertion with "set" works or is capped.
  let set0 = 0;
  let set1 = 1;
  if (name.startsWith("Big")) {
    set0 = 0n;
    set1 = 1n;
  }
  arr = new type(4);
  arr.set([set1], 1);
  assertEquals(new type([set0, set1, set0, set0]), arr, name);
  arr.set([set1, set1], 3);  // Capped to 2.
  assertEquals(new type([set0, set1, set1, set1]), arr, name);
}

testArrayType(Int8Array);
testArrayType(Uint8Array);
testArrayType(Uint8ClampedArray);
testArrayType(Int16Array);
testArrayType(Uint16Array);
testArrayType(Int32Array);
testArrayType(Uint32Array);
testArrayType(BigInt64Array);
testArrayType(BigUint64Array);
testArrayType(Float32Array);
testArrayType(Float64Array);
