// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --sandbox-testing

const kLocaleOffset = 12;
const kBreakIteratorOffset = 16;
const kUnicodeStringOffset = 20;

const mem = new DataView(new Sandbox.MemoryView(0, 0x100000000));

const bi = new Intl.v8BreakIterator('en', {type: 'word'});
const txt = 'foo '.repeat(20000);
bi.adoptText(txt);

const addr = Sandbox.getAddressOf(bi);
const locale_field = mem.getUint32(addr + kLocaleOffset, true);
const bi_field = mem.getUint32(addr + kBreakIteratorOffset, true);
const us_field = mem.getUint32(addr + kUnicodeStringOffset, true);

mem.setUint32(addr + kUnicodeStringOffset, locale_field, true);

gc();

bi.first();
bi.next();
bi.current();
