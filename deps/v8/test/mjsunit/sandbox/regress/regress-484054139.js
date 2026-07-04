// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --sandbox-testing

const kIcuBreakIteratorOffset = 12;
const kRawStringOffset = 16;
const kUnicodeStringOffset = 20;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const segm = new Intl.Segmenter('en', {granularity: 'word'});

const seg1 = segm.segment('A '.repeat(30000));
const seg2 = segm.segment('B '.repeat(30000));

const a1 = Sandbox.getAddressOf(seg1);
const a2 = Sandbox.getAddressOf(seg2);

const us1 = mem.getUint32(a1 + kUnicodeStringOffset, true);
const us2 = mem.getUint32(a2 + kUnicodeStringOffset, true);

mem.setUint32(a1 + kUnicodeStringOffset, us2, true);

gc();

seg1.containing(1);
