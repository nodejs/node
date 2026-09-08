// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const UNICODE_STRING_OFFSET = 20;

const segmenter = new Intl.Segmenter('en', { granularity: 'word' });
const seg1 = segmenter.segment("foo bar");
const seg2 = segmenter.segment("baz");

const addr1 = Sandbox.getAddressOf(seg1);
const addr2 = Sandbox.getAddressOf(seg2);
const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const seg2_us_value = mem.getUint32(addr2 + UNICODE_STRING_OFFSET, true);
mem.setUint32(addr1 + UNICODE_STRING_OFFSET, seg2_us_value, true);

gc();

seg1.containing(0);
