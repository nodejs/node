// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 1. Functional correctness of consecutive assertions
{
  let re1 = /\b/;
  let re2 = /\b\b/;
  assertEquals(re1.test('a b'), re2.test('a b'));
  assertEquals(re1.test('ab'), re2.test('ab'));
}

{
  let re1 = /\B/;
  let re2 = /\B\B/;
  assertEquals(re1.test('a b'), re2.test('a b'));
  assertEquals(re1.test('ab'), re2.test('ab'));
}

{
  let re1 = /^/;
  let re2 = /^^/;
  assertEquals(re1.test('a'), re2.test('a'));
  assertEquals(re1.test('\na'), re2.test('\na'));
}

{
  let re1 = /$/;
  let re2 = /$$/;
  assertEquals(re1.test('a'), re2.test('a'));
  assertEquals(re1.test('a\n'), re2.test('a\n'));
}

// 2. Functional correctness of nested redundant assertions (group flattening)
{
  let re1 = /\b/;
  let re2 = /\b(?:\b)/;
  let re3 = /\b(?:\b)(?:\b)/;
  assertEquals(re1.test('a b'), re2.test('a b'));
  assertEquals(re1.test('a b'), re3.test('a b'));
  assertEquals(re1.test('ab'), re2.test('ab'));
  assertEquals(re1.test('ab'), re3.test('ab'));
}

{
  let re1 = /^/;
  let re2 = /^(?:^)/;
  assertEquals(re1.test('a'), re2.test('a'));
}

// 3. Mixed assertions (should NOT be optimized/merged if different)
{
  let re = /\b\B/;  // This should still fail to match anything because a
                    // position cannot be both a boundary and a non-boundary.
  assertFalse(re.test('a b'));
  assertFalse(re.test('ab'));
}

// 4. Pathological case
{
  let re = new RegExp(
      '(I(()((?:\\b(?:\\b0?\\xb7n?\\B)\\b(?:\\b)\\b(?:\\b)\\b(?:\\b)\\b){3,}\xcd)?\xa2_))*',
      'my');
  re.exec('f\ud83d\udca9ba\u2603');
}
