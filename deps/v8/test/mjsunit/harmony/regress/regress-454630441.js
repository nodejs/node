// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// setFromHex
const rab_hex = new ArrayBuffer(8, {maxByteLength: 16});
const uint8_array_hex = new Uint8Array(rab_hex);
const result_hex = uint8_array_hex.setFromHex('cafed00dcafed00d');
assertEquals(16, result_hex.read);
assertEquals(8, result_hex.written);

// setFromBase64
const rab_b64 = new ArrayBuffer(8, {maxByteLength: 16});
const uint8_array_b64 = new Uint8Array(rab_b64);
const result_b64 = uint8_array_b64.setFromBase64('Zm9vYmFyZm9vYmFy');
assertEquals(8, result_b64.read);
assertEquals(6, result_b64.written);

// toBase64
const rab_to_b64 = new ArrayBuffer(6, {maxByteLength: 16});
const uint8_array_to_b64 = new Uint8Array(rab_to_b64);
uint8_array_to_b64.set([102, 111, 111, 98, 97, 114]);
assertEquals('Zm9vYmFy', uint8_array_to_b64.toBase64());

// toHex
const rab_to_hex = new ArrayBuffer(6, {maxByteLength: 16});
const uint8_array_to_hex = new Uint8Array(rab_to_hex);
uint8_array_to_hex.set([102, 111, 111, 98, 97, 114]);
assertEquals('666f6f626172', uint8_array_to_hex.toHex());
