// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

if (typeof Intl !== 'undefined') {
  // Build a ConsString DAG of 3-byte UTF-8 code units (U+0800) with >= 2^31
  // virtual characters, keeping intermediate lengths small so JS string
  // concatenation does not reject them.
  let s = '\u0800'.repeat(16);
  for (let i = 0; i < 27; i++) {
    s = s + s;
    Sandbox.corruptObjectField(s, 'length', 32);
  }
  // 0x55555555 * 3 == 0xFFFFFFFF UTF-8 bytes, which truncates to -1 in ICU's
  // 32-bit signed StringPiece length.
  Sandbox.corruptObjectField(s, 'length', 0x55555555);

  const kHeapObjectTag = 1;
  const taggedOf = obj => Sandbox.getAddressOf(obj) + kHeapObjectTag;

  const fmt = new Intl.NumberFormat('en-US');
  const expectedLocaleTagged = taggedOf(fmt.resolvedOptions().locale);
  const fmtSize = Sandbox.getSizeOf(fmt);
  let localeOffset = -1;
  for (let offset = 0; offset < fmtSize; offset += 4) {
    if (Sandbox.readObjectField(fmt, offset, 32) === expectedLocaleTagged) {
      localeOffset = offset;
      break;
    }
  }
  assertTrue(localeOffset >= 0);

  Sandbox.corruptObjectField(fmt, localeOffset, taggedOf(s));

  fmt.formatRange(1, 2);
  assertUnreachable('Corrupted String length should be caught by SBXCHECK');
}
