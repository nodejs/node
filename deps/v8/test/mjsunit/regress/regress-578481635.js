// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Compilation of this regexp fails with "Regular expression too large".
const too_big = '(?:(?:' + 'a'.repeat(5462) + '){3}){2}';
const re = new RegExp('x(?=' + too_big + ')|y', 'g');

// Resetting lastIndex in toString sends a global replace with an empty
// replacement string to the runtime.
const replacer = {
  toString() {
    re.lastIndex = 0;
    return '';
  },
};

function test(subject) {
  re.lastIndex = -1;
  assertThrows(
      () => subject.replace(re, replacer), SyntaxError,
      /Regular expression too large/);
}

test('b');
test('ሴb');
