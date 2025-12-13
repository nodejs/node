// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enable fuzzing flag for permissive error handling and to test that these
// functions are indeed enabled when fuzzing.
// Flags: --allow-natives-syntax --fuzzing

(function testNaNs() {
  const holeNaN = %GetHoleNaN();
  assertTrue(Number.isNaN(holeNaN));

  let dataView = new DataView(new ArrayBuffer(8));
  dataView.setUint32(0, %GetHoleNaNUpper(), true);
  dataView.setUint32(4, %GetHoleNaNLower(), true);
  let holeNaNViaDataView = dataView.getFloat64(0, true);
  assertTrue(Number.isNaN(holeNaNViaDataView));

  const undefinedNaN = %GetUndefinedNaN();
  assertTrue(Number.isNaN(undefinedNaN) || undefinedNaN === undefined);
})();

(function testStrings() {
  const consString = %ConstructConsString("Concatenated", "String");
  assertEquals("ConcatenatedString", consString);
  assertEquals(undefined, %ConstructConsString("a")); // Too few arguments.
  assertEquals(undefined, %ConstructConsString("a", 1)); // Wrongly typed argument.
  assertEquals(undefined, %ConstructConsString("a", "b")); // Too short.

  const slicedString = %ConstructSlicedString("StringLargeEnoughForSlicing", 6);
  assertEquals("LargeEnoughForSlicing", slicedString);
  assertEquals(undefined, %ConstructSlicedString("abcd", 2));
  assertEquals(undefined, %ConstructSlicedString("LargeStringButShortSlice", 19));
  assertEquals(undefined, %ConstructSlicedString("LargeStringButFullSlice", 0));
  assertEquals(undefined, %ConstructSlicedString("abcd"));
  assertEquals(undefined, %ConstructSlicedString("abcd", "ef"));

  const internalizedString = %ConstructInternalizedString("internalized")
  assertEquals("internalized", internalizedString);
  assertTrue(%IsInternalizedString(internalizedString));
  assertEquals(undefined, %ConstructInternalizedString());
  assertEquals(undefined, %ConstructInternalizedString(1));
  const internalized2Byte = %ConstructInternalizedString("2 byte string µ");
  assertEquals("2 byte string µ", internalized2Byte);
  assertTrue(%IsInternalizedString(internalized2Byte));

  const thinString = %ConstructThinString("StringLargeEnoguhForConsString");
  assertEquals("StringLargeEnoguhForConsString", thinString);
  assertEquals(undefined, %ConstructThinString());
  assertEquals(undefined, %ConstructThinString(1));
  assertEquals(undefined, %ConstructThinString("short"));
})();
