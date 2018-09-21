// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let module_size = 19;
let string_len = 0x00fffff0 - module_size;

print("Allocating backing store: " + (string_len + module_size));
let backing = new ArrayBuffer(string_len + module_size);

print("Allocating typed array buffer");
let buffer = new Uint8Array(backing);

print("Filling...");
buffer.fill(0x41);

print("Setting up array buffer");
// Magic
buffer.set([0x00, 0x61, 0x73, 0x6D], 0);
// Version
buffer.set([0x01, 0x00, 0x00, 0x00], 4);
// kUnknownSection (0)
buffer.set([0], 8);
// Section length
buffer.set([0x80, 0x80, 0x80, 0x80, 0x00],  9);
// Name length
let x = string_len + 1;
let b1 = ((x >> 0) & 0x7F) | 0x80;
let b2 = ((x >> 7) & 0x7F) | 0x80;
let b3 = ((x >> 14) & 0x7F) | 0x80;
let b4 = ((x >> 21) & 0x7F);
//buffer.set([0xDE, 0xFF, 0xFF, 0x7F], 14);
 buffer.set([b1, b2, b3, b4], 14);

print("Parsing module...");
let m = new WebAssembly.Module(buffer);

print("Triggering!");
let c = WebAssembly.Module.customSections(m, "A".repeat(string_len + 1));
assertEquals(0, c.length);
