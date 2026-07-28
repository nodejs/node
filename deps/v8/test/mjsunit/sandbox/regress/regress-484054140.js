// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --sandbox-testing

const kIcuBreakIteratorOffset = 12;
const kRawStringOffset = 16;
const kUnicodeStringOffset = 20;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const segmenter = new Intl.Segmenter('en', {granularity: 'grapheme'});
const text = 'a'.repeat(120000);
const iterator = segmenter.segment(text)[Symbol.iterator]();

const addr = Sandbox.getAddressOf(iterator);
const biField = mem.getUint32(addr + kIcuBreakIteratorOffset, true);
const rawStrField = mem.getUint32(addr + kRawStringOffset, true);
const usField = mem.getUint32(addr + kUnicodeStringOffset, true);

mem.setUint32(addr + kUnicodeStringOffset, rawStrField, true);

gc();

iterator.next();
