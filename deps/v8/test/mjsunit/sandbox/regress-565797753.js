// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

let memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

// Create two direct sequential one-byte strings
let s1 = JSON.parse('"' + 'A'.repeat(100) + '"');
let s2 = JSON.parse('"' + 'B'.repeat(100) + '"');

// Concatenate them to create a ConsString whose halves are both direct (SeqOneByteString)
let cons = s1 + s2;

// Create a SlicedString
let base_str = JSON.parse('"' + 'C'.repeat(100) + '"');
let slice = base_str.substring(0, 50);

let slice_addr = Sandbox.getAddressOf(slice);
let cons_addr = Sandbox.getAddressOf(cons);

// Overwrite slice.parent_ with cons (tagged pointer)
memory.setUint32(slice_addr + 12, cons_addr | 1, true);

// Overwrite slice.length_ with 10 (length < cons.first length of 100)
memory.setUint32(slice_addr + 8, 10, true);

// Trigger StringTable::TryStringToIndexOrLookupExisting via Object.prototype.hasOwnProperty
Object.prototype.hasOwnProperty.call({}, slice);
