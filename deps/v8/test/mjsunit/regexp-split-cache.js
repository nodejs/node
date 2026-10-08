// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

// A hit skips the match loop, so it must replay the legacy RegExp statics the
// loop would have updated. A match can update those statics without
// contributing an element, so the replay does not follow from the result.

function statics() {
  return [RegExp.input, RegExp.lastMatch, RegExp.leftContext,
          RegExp.rightContext, RegExp.$1, RegExp.$2].join('|');
}

// Overwrites every static, so a missing replay shows up as the leftovers.
function clobberStatics() {
  /seed(s)/.test('hello seeds world');
  assertEquals('hello seeds world', RegExp.input);
}

const kClobbered = 'hello seeds world|seeds|hello | world|s|';

function split(subject, regexp) {
  clobberStatics();
  return {result: subject.split(regexp), statics: statics()};
}

// String literals are internalized, so all these subjects are cacheable.
const cases = [
  // The splitter is sticky and only probes below the subject length, so /$/
  // never matches and the statics stay clobbered.
  {subject: 'abc', regexp: /$/, result: ['abc'], statics: kClobbered},
  // Zero-length match at position 0: records statics, contributes no element.
  {subject: 'abc', regexp: /(?=abc)/, result: ['abc'],
   statics: 'abc|||abc||'},
  {subject: 'abc', regexp: /^/, result: ['abc'], statics: 'abc|||abc||'},
  // Ordinary separators.
  {subject: 'abc', regexp: /b/, result: ['a', 'c'], statics: 'abc|b|a|c||'},
  {subject: 'a,b,c', regexp: /,/, result: ['a', 'b', 'c'],
   statics: 'a,b,c|,|a,b|c||'},
  // A capture group that participates: $1 must be replayed.
  {subject: 'abc', regexp: /(b)/, result: ['a', 'b', 'c'],
   statics: 'abc|b|a|c|b|'},
  // A non-participating group yields undefined, which must round-trip through
  // the cached elements.
  {subject: 'abc', regexp: /(x)|(b)/, result: ['a', undefined, 'b', 'c'],
   statics: 'abc|b|a|c||b'},
  // No match at all: nothing to replay, statics must stay clobbered.
  {subject: 'abc', regexp: /zzz/, result: ['abc'], statics: kClobbered},
  // Long enough that the parts array is worth sharing.
  {subject: 'aXbXcXdXeXfXgXhXiXjXkXlXmXnXoXp', regexp: /X/,
   result: ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
            'n', 'o', 'p'],
   statics: 'aXbXcXdXeXfXgXhXiXjXkXlXmXnXoXp|X|aXbXcXdXeXfXgXhXiXjXkXlXmXnXo' +
            '|p||'},
  // Same subject, different patterns, so never served from each other's entry.
  {subject: '1a2b3', regexp: /a/, result: ['1', '2b3'],
   statics: '1a2b3|a|1|2b3||'},
  {subject: '1a2b3', regexp: /b/, result: ['1a2', '3'],
   statics: '1a2b3|b|1a2|3||'},
  {subject: '1a2b3', regexp: /[ab]/, result: ['1', '2', '3'],
   statics: '1a2b3|b|1a2|3||'},
  // Flags are part of the key. Both leave the same statics, so only the result
  // tells them apart.
  {subject: 'aXbxc', regexp: /x/, result: ['aXb', 'c'],
   statics: 'aXbxc|x|aXb|c||'},
  {subject: 'aXbxc', regexp: /x/i, result: ['a', 'b', 'c'],
   statics: 'aXbxc|x|aXb|c||'},
  // A zero-length match at the end is not probed either, so the entry holds
  // only the whole subject.
  {subject: 'abc', regexp: /(?=$)/, result: ['abc'], statics: kClobbered},
];

function check(testCase) {
  const observed = split(testCase.subject, testCase.regexp);
  assertEquals(testCase.result, observed.result);
  assertEquals(testCase.statics, observed.statics);
  // A hit builds the array in generated code, but must return the same map.
  assertTrue(%HaveSameMap(observed.result, testCase.uncached));
}

for (const testCase of cases) {
  assertTrue(%IsInternalizedString(testCase.subject));

  // The first split of a pair cannot be a cache hit. It fills the entry, so
  // every check below is served from the cache and a few rounds are enough.
  testCase.uncached = split(testCase.subject, testCase.regexp).result;
  for (let i = 0; i < 3; ++i) check(testCase);

  // Entries are dropped on major GC; refilling must behave the same way.
  gc();
  for (let i = 0; i < 3; ++i) check(testCase);
}

// The slot pair is picked by the subject hash alone, so cases sharing a subject
// compete for two entries. Interleaved, they evict each other instead of each
// settling into a steady-state hit.
for (let i = 0; i < 20; ++i) {
  for (const testCase of cases) check(testCase);
}

// The cases below vary what the table holds fixed: the receiver, the limit, or
// whether the subject is cacheable at all.

// A hit returns a fresh array over shared copy-on-write elements. Writing to it
// must not corrupt later results.
const shared = 'a,b,c';
for (let i = 0; i < 3; ++i) {
  const array = shared.split(/,/);
  assertEquals(['a', 'b', 'c'], array);
  array[1] = 'mutated';
  array.push('appended');
}
assertEquals(['a', 'b', 'c'], shared.split(/,/));

// A limited split holds only some parts, so it must neither be cached nor
// served from an unlimited split's entry.
const limited = 'p,q,r,s';
assertEquals(['p', 'q', 'r', 's'], limited.split(/,/));
for (let i = 0; i < 3; ++i) {
  assertEquals(['p', 'q'], limited.split(/,/, 2));
  assertEquals(['p', 'q', 'r', 's'], limited.split(/,/));
}

// Splitting never touches the receiver's lastIndex, cached or not.
for (const regexp of [/b/, /b/g, /b/y]) {
  regexp.lastIndex = 2;
  for (let i = 0; i < 3; ++i) {
    'abc'.split(regexp);
    assertEquals(2, regexp.lastIndex);
  }
}

// Distinct RegExp objects with the same pattern share RegExpData, and so share
// cache entries. Sound only because they produce identical results.
const shared_data = 'u-v-w';
for (let i = 0; i < 3; ++i) {
  assertEquals(shared_data.split(new RegExp('-')), shared_data.split(/-/));
}

// Non-internalized subjects are never cached, and must still split correctly.
const rope = ('a,b,c ').slice(0, 5);
assertFalse(%IsInternalizedString(rope));
for (let i = 0; i < 3; ++i) {
  assertEquals(['a', 'b', 'c'], rope.split(/,/));
}
