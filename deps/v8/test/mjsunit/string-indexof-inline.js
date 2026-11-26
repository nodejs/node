// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

function refIndexOf(s, p, from = 0) {
  from = Math.max(0, from);
  if (p.length === 0) return from <= s.length ? from : -1;
  for (let i = from; i <= s.length - p.length; i++) {
    let ok = true;
    for (let j = 0; j < p.length; j++) {
      if (s.charCodeAt(i + j) !== p.charCodeAt(j)) {
        ok = false;
        break;
      }
    }
    if (ok) return i;
  }
  return -1;
}

function check(s, p, from) {
  assertEquals(refIndexOf(s, p, from), s.indexOf(p, from));
  assertEquals(refIndexOf(s, p, from) !== -1, s.includes(p, from));
}

// A two-byte string whose characters all fit in one byte.
function twoByte(s) {
  const t = createExternalizableTwoByteString(s);
  externalizeString(t);
  return t;
}

// Subject filler, pattern first/middle/last characters for each combination
// of one-byte and two-byte strings.
const kEncodings = [
  {name: 'one-byte', fill: 'b', first: 'A', mid: 'x', last: 'Z'},
  {name: 'two-byte', fill: '\u1234', first: '\u0100', mid: 'x', last: '\uFFFF'},
];

// Candidate counts around the inline limit of 16, and search lengths around
// the limit of 32.
for (const enc of kEncodings) {
  for (const searchLen of [1, 2, 3, 31, 32, 33]) {
    const p = searchLen === 1 ?
        enc.first :
        enc.first + enc.mid.repeat(searchLen - 2) + enc.last;
    for (const cand of [1, 2, 15, 16, 17, 18, 33]) {
      const len = searchLen + cand - 1;
      check(enc.fill.repeat(len), p, 0);
      for (const pos of new Set([0, 1, cand - 2, cand - 1])) {
        if (pos < 0 || pos >= cand) continue;
        const s = enc.fill.repeat(pos) + p + enc.fill.repeat(len - pos - searchLen);
        for (const from of [-1, 0, pos, pos + 1, len - searchLen,
                            len - searchLen + 1, len, len + 1]) {
          check(s, p, from);
        }
      }
      // False positives: first and last characters match, the middle doesn't.
      if (searchLen >= 3) {
        const fp = enc.first + enc.fill.repeat(searchLen - 2) + enc.last;
        const s = (fp + enc.fill).repeat(cand).slice(0, len);
        check(s, p, 0);
        check(s + p, p, 0);
      }
    }
  }
}

// One-byte search string in a two-byte subject, and vice versa.
{
  const p = 'Ax' + 'Z';
  for (const cand of [1, 15, 16, 17, 33]) {
    const len = p.length + cand - 1;
    const s = '\u1234'.repeat(cand - 1) + p;
    assertEquals(len, s.length);
    check(s, p, 0);
    check('\u1234'.repeat(len), p, 0);
  }
  const s = 'b'.repeat(20) + 'AxZ' + 'b'.repeat(5);
  check(s, twoByte('AxZ'), 0);
  check(s, twoByte('AxY'), 0);
  check(s, 'Ax\u1234', 0);
  check(twoByte(s), 'AxZ', 0);
}

// Surrogate pairs sharing a lead surrogate.
{
  const cards = '\uD83C\uDCA1\uD83C\uDCB2\uD83C\uDCC2\uD83C\uDCD5\uD83C\uDCA9';
  const pair = '\uD83C\uDCB2\uD83C\uDCC2';
  assertEquals(2, cards.indexOf(pair));
  assertEquals(8, cards.indexOf('\uD83C\uDCA9'));
  assertEquals(0, cards.indexOf('\uD83C'));
  assertEquals(-1, cards.indexOf('\uD83C\uDCA2'));
  const extCards = twoByte(cards);
  assertEquals(2, extCards.indexOf(pair));
  assertEquals(-1, extCards.indexOf('\uD83C\uDCA2'));
}

// External and sliced external subjects.
{
  const s = createExternalizableString('foo bar #include<x> baz');
  externalizeString(s);
  assertEquals(8, s.indexOf('#include<x>'));
  assertEquals(5, s.slice(3).indexOf('#include<x>'));
  assertEquals(-1, s.indexOf('#include<y>'));
}

// replaceAll and includes use the same search.
assertEquals('a-b-c', 'a, b, c'.replaceAll(', ', '-'));
assertTrue('abcdef'.includes('cde'));
assertFalse('abcdef'.includes('cdf'));
