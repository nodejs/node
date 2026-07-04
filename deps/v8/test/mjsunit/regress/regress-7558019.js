// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kQuotes = 1 << 26;

const str = '"'.repeat(kQuotes);
try {
  const link = 'CLICK ME'.link(str);
  const expected = '<a href="' + '&quot;'.repeat(kQuotes) + '">CLICK ME</a>';
  assertEquals(expected, link);
} catch (e) {
  // On 32-bit platforms the string will be too large.
  // We expect a RangeError to be thrown in that case.
  assertInstanceof(e, RangeError);
}
