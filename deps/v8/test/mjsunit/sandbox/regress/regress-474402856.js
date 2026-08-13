// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --expose-gc

const UNICODE_STRING_OFFSET = 20;

const text1 = "A".repeat(100);
const segmenter1 = new Intl.Segmenter();
const segments1 = segmenter1.segment(text1);

const segmentsOffset1 = Sandbox.getAddressOf(segments1);
const buffer = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const text2 = "B".repeat(100);
const segmenter2 = new Intl.Segmenter();
const segments2 = segmenter2.segment(text2);
const segmentsOffset2 = Sandbox.getAddressOf(segments2);
const unicodeStringPtr2 = buffer.getUint32(
    Number(segmentsOffset2) + UNICODE_STRING_OFFSET, true);

buffer.setUint32(segmentsOffset1 + UNICODE_STRING_OFFSET, unicodeStringPtr2, true);

gc();

segments1.containing(0);
