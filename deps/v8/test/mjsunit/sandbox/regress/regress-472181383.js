// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --sandbox-testing --expose-gc

const UNICODE_STRING_OFFSET = 20;

const segmenter = new Intl.Segmenter("en", { granularity: "word" });
const segments = segmenter.segment("aaa bar");
const iterator = segments[Symbol.iterator]();

const addr = Sandbox.getAddressOf(iterator);
const memory = new DataView(new Sandbox.MemoryView(0, 0x100000000));

memory.setUint32(addr + UNICODE_STRING_OFFSET, 0x1, true);

gc();

iterator.next();
