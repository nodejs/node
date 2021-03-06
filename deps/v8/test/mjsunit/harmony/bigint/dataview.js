// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var buffer = new ArrayBuffer(64);
var dataview = new DataView(buffer, 8, 24);
var bytes = new Uint8Array(buffer);

var b1 = 0xff1234567890abcdefn;
var b1_64 = BigInt.asUintN(64, b1);

dataview.setBigInt64(8, b1);
assertEquals(0x12, bytes[16]);
assertEquals(0x34, bytes[17]);
assertEquals(0x56, bytes[18]);
assertEquals(0x78, bytes[19]);
assertEquals(0x90, bytes[20]);
assertEquals(0xab, bytes[21]);
assertEquals(0xcd, bytes[22]);
assertEquals(0xef, bytes[23]);
assertEquals(b1_64, dataview.getBigInt64(8));

dataview.setBigInt64(8, b1, true);  // Little-endian.
assertEquals(0xef, bytes[16]);
assertEquals(0xcd, bytes[17]);
assertEquals(0xab, bytes[18]);
assertEquals(0x90, bytes[19]);
assertEquals(0x78, bytes[20]);
assertEquals(0x56, bytes[21]);
assertEquals(0x34, bytes[22]);
assertEquals(0x12, bytes[23]);
assertEquals(b1_64, dataview.getBigInt64(8, true));

dataview.setBigUint64(8, b1);
assertEquals(0x12, bytes[16]);
assertEquals(0x34, bytes[17]);
assertEquals(0x56, bytes[18]);
assertEquals(0x78, bytes[19]);
assertEquals(0x90, bytes[20]);
assertEquals(0xab, bytes[21]);
assertEquals(0xcd, bytes[22]);
assertEquals(0xef, bytes[23]);
assertEquals(b1_64, dataview.getBigUint64(8));

dataview.setBigUint64(8, b1, true);  // Little-endian.
assertEquals(0xef, bytes[16]);
assertEquals(0xcd, bytes[17]);
assertEquals(0xab, bytes[18]);
assertEquals(0x90, bytes[19]);
assertEquals(0x78, bytes[20]);
assertEquals(0x56, bytes[21]);
assertEquals(0x34, bytes[22]);
assertEquals(0x12, bytes[23]);
assertEquals(b1_64, dataview.getBigUint64(8, true));

var b2 = -0x76543210fedcba98n;
dataview.setBigInt64(8, b2, true);
assertEquals(0x68, bytes[16]);
assertEquals(0x45, bytes[17]);
assertEquals(0x23, bytes[18]);
assertEquals(0x01, bytes[19]);
assertEquals(0xef, bytes[20]);
assertEquals(0xcd, bytes[21]);
assertEquals(0xab, bytes[22]);
assertEquals(0x89, bytes[23]);
assertEquals(b2, dataview.getBigInt64(8, true));
assertEquals(0x89abcdef01234568n, dataview.getBigUint64(8, true));

var b3 = -0x8000000000000000n; // The int64_t minimum value.
dataview.setBigInt64(8, b3);
assertEquals(b3, dataview.getBigInt64(8));
assertEquals(-b3, dataview.getBigUint64(8));

var b4 = 0x8000000000000000n;
dataview.setBigInt64(8, b4);
assertEquals(-b4, dataview.getBigInt64(8));
assertEquals(b4, dataview.getBigUint64(8));

assertThrows(() => dataview.setBigInt64(0, 1), TypeError);
assertThrows(() => dataview.setBigUint64(0, 1), TypeError);
assertThrows(() => dataview.setInt32(0, 1n), TypeError);
assertThrows(() => dataview.setUint32(0, 1n), TypeError);

// None of the stores wrote out of bounds.
for (var i = 0; i < 16; i++) assertEquals(0, bytes[i]);
for (var i = 24; i < 64; i++) assertEquals(0, bytes[i]);
