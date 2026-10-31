// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function execAt(re, str, lastIndex) {
  re.lastIndex = lastIndex;
  return re.exec(str);
}

function assertMatch(expected, index, lastIndex, result, re) {
  assertNotNull(result);
  assertEquals(expected, result[0]);
  assertEquals(index, result.index);
  assertEquals(lastIndex, re.lastIndex);
}

const kEmoji = '\u{1F600}';
const kTrailSurrogate = '\uDE00';

// Non-Unicode matching keeps the code unit position.
for (const re of [/./g, /./y]) {
  assertMatch(kTrailSurrogate, 1, 2, execAt(re, kEmoji, 1), re);
}

// lastIndex points at the trail surrogate of a pair. Matching starts at the
// lead surrogate; if that fails, the trail surrogate must not be retried.
for (const re of [/(?!.)/yu, /(?!.)|a^/yu, /(?!.)/yv, /(?![\s\S])(?!$)/gu,
                  /(?![\s\S])(?!$)/gv]) {
  assertNull(execAt(re, kEmoji, 1), re.source);
  assertEquals(0, re.lastIndex);
}

// With g, a failure from the lead surrogate must not rescan the rest of the
// subject from the trail surrogate.
{
  const re = /(?![\s\S])(?!$)|^x/gu;
  assertNull(execAt(re, kEmoji + 'x', 1));
}

// Positive controls: stepping back to the lead surrogate still works.
{
  const re = /./yu;
  assertMatch(kEmoji, 0, 2, execAt(re, kEmoji, 1), re);
}
{
  const re = /\u{1F600}/gu;
  assertMatch(kEmoji, 1, 3, execAt(re, 'a' + kEmoji, 2), re);
  assertMatch(kEmoji, 0, 2, execAt(re, kEmoji, 1), re);
}
{
  const re = /\u{1F600}/yv;
  assertMatch(kEmoji, 0, 2, execAt(re, kEmoji + 'a', 1), re);
}
{
  // Backreferences and lookbehinds see the stepped-back position.
  const re = /(?<=^)(.)\1/yu;
  assertMatch(kEmoji + kEmoji, 0, 4, execAt(re, kEmoji + kEmoji, 1), re);
  const re2 = /(?<!^)./gu;
  assertMatch('b', 2, 3, execAt(re2, kEmoji + 'b', 1), re2);
}
{
  // Global search continues forward from the lead surrogate.
  const re = /b/gu;
  assertMatch('b', 2, 3, execAt(re, kEmoji + 'b', 1), re);
}

// Global matching restarts at the next pair without repeating a match.
for (const re of [/./gu, /./gv]) {
  assertEquals([kEmoji, kEmoji], (kEmoji + kEmoji).match(re));
}

// lastIndex on a non-surrogate character: no step back.
{
  const re = /(?!.)/yu;
  assertNull(execAt(re, 'ab\u1234', 1));
  const re2 = /./yu;
  assertMatch('b', 1, 2, execAt(re2, 'ab\u1234', 1), re2);
}

// Lone trail surrogate not preceded by a lead surrogate: no step back.
{
  const re = /./yu;
  assertMatch(kTrailSurrogate, 1, 2, execAt(re, 'a' + kTrailSurrogate, 1), re);
  const re2 = /[\uDC00-\uDFFF]/yu;
  assertMatch(kTrailSurrogate, 0, 1, execAt(re2, kTrailSurrogate, 0), re2);
  const re3 = /(?!.)/yu;
  assertNull(execAt(re3, 'a' + kTrailSurrogate, 1));
  const re4 = /(?=\uDE00)/yu;
  assertMatch('', 1, 1, execAt(re4, 'a' + kTrailSurrogate, 1), re4);
}

// A lone trail surrogate at index 0 (nothing to step back over).
{
  const re = /\uDE00/yu;
  assertMatch(kTrailSurrogate, 0, 1, execAt(re, kTrailSurrogate + kEmoji, 0), re);
}

// The surrogate check must also allow matching at the end of the input.
for (const re of [/$/gu, /$/yu, /$/gv, /$/yv]) {
  assertMatch('', 0, 0, execAt(re, '', 0), re);
  assertMatch('', 2, 2, execAt(re, kEmoji, 2), re);
}

// /(?!.)/gu scans forward from the lead surrogate to the end of the string.
{
  const re = /(?!.)/gu;
  assertMatch('', 2, 2, execAt(re, kEmoji, 1), re);
  assertMatch('', 4, 4, execAt(re, kEmoji + kEmoji, 3), re);
  assertMatch('', 3, 3, execAt(re, kEmoji + 'a', 1), re);
}

// A lead surrogate at the end of the string and a trail surrogate preceded by
// a trail surrogate are not pairs: no step back.
{
  const re = /./yu;
  assertMatch('\uD83D', 1, 2, execAt(re, 'a\uD83D', 1), re);
  assertMatch(kTrailSurrogate, 1, 2, execAt(re, kTrailSurrogate + kTrailSurrogate, 1), re);
}
