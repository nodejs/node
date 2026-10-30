// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unrolls to six copies of the literal, which exceeds the maximum character
// offset while keeping the pattern short enough to be optimized.
const kTooBig = '(?:(?:' + 'a'.repeat(5462) + '){3}){2}';
const subject = 'b'.repeat(1000);

function assertTooLarge(source) {
  assertThrows(() => new RegExp(source).exec(subject), SyntaxError,
               /Regular expression too large/);
}

// BEGIN_POSITIVE_SUBMATCH / BEGIN_NEGATIVE_SUBMATCH with a non-trivial trace.
assertTooLarge('x(?=' + kTooBig + ')|y');
assertTooLarge('x(?!' + kTooBig + ')|y');
