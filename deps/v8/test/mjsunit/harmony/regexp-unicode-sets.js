// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-regexp-unicode-sets

// u and v are not allowed together.
assertEarlyError('/./uv');
assertThrowsAtRuntime("new RegExp('.','uv')", SyntaxError);

assertEquals('v', /./v.flags);
assertTrue(/./v.unicodeSets);

// Characters that require escaping within a character class in /v mode
assertEarlyError('/[(]/v');
assertEarlyError('/[)]/v');
assertEarlyError('/[[]/v');
assertEarlyError('/[]]/v');
assertEarlyError('/[{]/v');
assertEarlyError('/[}]/v');
assertEarlyError('/[/]/v');
assertEarlyError('/[-]/v');
// Need to escape the backslash, as assertEarlyError uses eval().
assertEarlyError('/[\\]/v');
assertEarlyError('/[|]/v');

assertEarlyError('/[&&]/v');
assertEarlyError('/[!!]/v');
assertEarlyError('/[##]/v');
assertEarlyError('/[$$]/v');
assertEarlyError('/[%%]/v');
assertEarlyError('/[**]/v');
assertEarlyError('/[++]/v');
assertEarlyError('/[,,]/v');
assertEarlyError('/[..]/v');
assertEarlyError('/[::]/v');
assertEarlyError('/[;;]/v');
assertEarlyError('/[<<]/v');
assertEarlyError('/[==]/v');
assertEarlyError('/[>>]/v');
assertEarlyError('/[??]/v');
assertEarlyError('/[@@]/v');
// The first ^ negates the class. The following two are not valid.
assertEarlyError('/[^^^]/v');
assertEarlyError('/[``]/v');
assertEarlyError('/[~~]/v');

assertEarlyError('/[a&&&]/v');
assertEarlyError('/[&&&a]/v');

const allAscii = Array.from(
    {length: 127}, (v, i) => { return String.fromCharCode(i); });

function check(re, expectMatch, expectNoMatch) {
  if (expectNoMatch === undefined) {
    const expectSet = new Set(expectMatch.map(val => {
      return (typeof val == 'number') ? String(val) : val; }));
    expectNoMatch = allAscii.filter(val => !expectSet.has(val));
  }
  for (const match of expectMatch) {
    assertTrue(re.test(match), `${re}.test(${match})`);
  }
  for (const noMatch of expectNoMatch) {
    assertFalse(re.test(noMatch), `${re}.test(${noMatch})`);
  }
  // Nest the current RegExp in a negated class and check expectations are
  // inversed.
  const inverted = new RegExp(`[^${re.source}]`, re.flags);
  for (const match of expectMatch) {
    assertFalse(inverted.test(match), `${inverted}.test(${match})`);
  }
  for (const noMatch of expectNoMatch) {
    assertTrue(inverted.test(noMatch), `${inverted}.test(${noMatch})`);
  }
}

// Union with nested class
check(
    /[\da-f[xy][^[^z]]]/v, Array.from('0123456789abcdefxyz'),
    Array.from('ghijklmnopqrstuv!?'));

// Intersections
check(/[\d&&[0-9]]/v, Array.from('0123456789'), []);
check(/[\d&&0]/v, [0], Array.from('123456789'));
check(/[\d&&9]/v, [9], Array.from('012345678'));
check(/[\d&&[02468]]/v, Array.from('02468'), Array.from('13579'));
check(/[\d&&[13579]]/v, Array.from('13579'), Array.from('02468'));
check(
    /[\w&&[^a-zA-Z_]]/v, Array.from('0123456789'),
    Array.from('abcdxyzABCDXYZ_!?'));
check(
    /[^\w&&[a-zA-Z_]]/v, Array.from('0123456789!?'),
    Array.from('abcdxyzABCDXYZ_'));

// Subtractions
check(/[\d--[!-%]]/v, Array.from('0123456789'));
check(/[\d--[A-Z]]/v, Array.from('0123456789'));
check(/[\d--[0-9]]/v, []);
check(/[\d--[\w]]/v, []);
check(/[\d--0]/v, Array.from('123456789'));
check(/[\d--9]/v, Array.from('012345678'));
check(/[[\d[a-c]]--9]/v, Array.from('012345678abc'));
check(/[\d--[02468]]/v, Array.from('13579'));
check(/[\d--[13579]]/v, Array.from('02468'));
check(/[[3-7]--[0-9]]/v, []);
check(/[[3-7]--[0-7]]/v, []);
check(/[[3-7]--[3-9]]/v, []);
check(/[[3-79]--[0-7]]/v, [9]);
check(/[[3-79]--[3-9]]/v, []);
check(/[[3-7]--[0-3]]/v, Array.from('4567'));
check(/[[3-7]--[0-5]]/v, Array.from('67'));
check(/[[3-7]--[7-9]]/v, Array.from('3456'));
check(/[[3-7]--[5-9]]/v, Array.from('34'));
check(/[[3-7a-c]--[0-3]]/v, Array.from('4567abc'));
check(/[[3-7a-c]--[0-5]]/v, Array.from('67abc'));
check(/[[3-7a-c]--[7-9]]/v, Array.from('3456abc'));
check(/[[3-7a-c]--[5-9]]/v, Array.from('34abc'));
check(/[[2-8]--[0-3]--5--[7-9]]/v, Array.from('46'));
check(/[[2-57-8]--[0-3]--[5-7]]/v, Array.from('48'));
check(/[[0-57-8]--[1-34]--[5-7]]/v, Array.from('08'));
check(/[\d--[^02468]]/v, Array.from('02468'));
check(/[\d--[^13579]]/v, Array.from('13579'));

// Ignore-Case
check(/[Ā-č]/v, Array.from('ĀāĂăĄąĆć'), Array.from('abc'));
check(/[ĀĂĄĆ]/vi, Array.from('ĀāĂăĄąĆć'), Array.from('abc'));
check(/[āăąć]/vi, Array.from('ĀāĂăĄąĆć'), Array.from('abc'));

// Some more sophisticated tests taken from
// https://v8.dev/features/regexp-v-flag
assertFalse(/[\p{Script_Extensions=Greek}--π]/v.test('π'));
assertFalse(/[\p{Script_Extensions=Greek}--[αβγ]]/v.test('α'));
assertFalse(/[\p{Script_Extensions=Greek}--[α-γ]]/v.test('β'));
assertTrue(/[\p{Decimal_Number}--[0-9]]/v.test('𑜹'));
assertFalse(/[\p{Decimal_Number}--[0-9]]/v.test('4'));
assertTrue(/[\p{Script_Extensions=Greek}&&\p{Letter}]/v.test('π'));
assertFalse(/[\p{Script_Extensions=Greek}&&\p{Letter}]/v.test('𐆊'));
assertTrue(/[\p{White_Space}&&\p{ASCII}]/v.test('\n'));
assertFalse(/[\p{White_Space}&&\p{ASCII}]/v.test('\u2028'));
assertTrue(/[\p{Script_Extensions=Mongolian}&&\p{Number}]/v.test('᠗'));
assertFalse(/[\p{Script_Extensions=Mongolian}&&\p{Number}]/v.test('ᠴ'));
assertEquals('XXXXXX4#', 'aAbBcC4#'.replaceAll(/\p{Lowercase_Letter}/giv, 'X'));
assertEquals('XXXXXX4#', 'aAbBcC4#'.replaceAll(/[^\P{Lowercase_Letter}]/giv, 'X'));
