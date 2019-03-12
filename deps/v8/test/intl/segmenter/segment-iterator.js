// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-intl-segmenter

const text = "Hello World, Test 123! Foo Bar. How are you?";
for (const granularity of ["grapheme", "word", "sentence"]) {
  const segmenter = new Intl.Segmenter("en", { granularity });
  const iter = segmenter.segment(text);

  assertEquals("number", typeof iter.index);
  assertEquals(0, iter.index);
  assertEquals(undefined, iter.breakType);
}
