// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test() {
  assertEquals('ba', /(?<=(ba|a|c))x/.exec('bax')[1]);
  assertEquals('cba', /(?<=(cba|ba|a|d))x/.exec('cbax')[1]);
  assertTrue(/(?<=(ba|a|c))x\1/.test('baxba'));
  assertFalse(/(?<=(ba|a|c))x\1$/.test('baxa'));

  // the same capture case with /u, and /i
  assertEquals('ba', /(?<=(ba|a|c))x/u.exec('bax')[1]);
  assertEquals('ba', /(?<=(BA|a|c))x/i.exec('bax')[1]);

  // replace
  assertEquals('ba[ba]', 'bax'.replace(/(?<=(ba|a|c))x/, '[$1]'));

  // Forward-mode factoring is unchanged
  assertEquals('ab', /(?=(ab|a|c))/.exec('ab')[1]);
}

test();
