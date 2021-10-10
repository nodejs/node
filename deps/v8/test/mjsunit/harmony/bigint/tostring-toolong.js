// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kLongString = 100_000;  // Bigger than kMaxRenderedLength in bigint.cc.

const str = 'z'.repeat(kLongString);
try {
  BigInt(str);
  assertUnreachable("should have thrown");
} catch (e) {
  assertTrue(e instanceof SyntaxError);
  assertTrue(e.message.startsWith("Cannot convert zzz"));
  // Despite adding "Cannot convert", the overall message is shorter than
  // the invalid string, because it only includes a prefix of the string.
  assertTrue(e.message.length < kLongString);
}
