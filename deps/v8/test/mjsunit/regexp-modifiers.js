// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function test_invalid(re) {
  assertEarlyError(`/${re}/`);
  assertThrowsAtRuntime(`new RegExp('${re}')`, SyntaxError);
}

test_invalid('(?-:.)');
test_invalid('(?--:.)');
test_invalid('(?mm:.)');
test_invalid('(?ii:.)');
test_invalid('(?ss:.)');
test_invalid('(?-mm:.)');
test_invalid('(?-ii:.)');
test_invalid('(?-ss:.)');
test_invalid('(?g-:.)');
test_invalid('(?-u:.)');
test_invalid('(?m-m:.)');
test_invalid('(?i-i:.)');
test_invalid('(?s-s:.)');
test_invalid('(?msi-ims:.)');
test_invalid('(?i--m:.)');
test_invalid('(?i<)');
test_invalid('(?i=)');
test_invalid('(?i!)');
test_invalid('(?m<)');
test_invalid('(?m=)');
test_invalid('(?m!)');
test_invalid('(?s<)');
test_invalid('(?s=)');
test_invalid('(?s!)');
test_invalid('(?-<)');
test_invalid('(?-=)');
test_invalid('(?-!)');

function test(re, expectedMatch, expectedNoMatch = []) {
  for (const match of expectedMatch) {
    assertTrue(re.test(match), `${re}.test(${match})`);
  }
  for (const match of expectedNoMatch) {
    assertFalse(re.test(match), `${re}.test(${match})`);
  }
}

test(/(?i:ba)r/, ['bar', 'Bar', 'BAr'], ['BAR', 'BaR']);
test(/(?-i:ba)r/i, ['bar', 'baR'], ['Bar', 'BAR']);
test(/F(?i:oo(?-i:b)a)r/, ['Foobar', 'FoObAr'], ['FooBar', 'FoobaR']);
test(/F(?i:oo(?i:b)a)r/, ['Foobar', 'FoObAr', 'FOOBAr'], ['FoobaR']);
test(/^[a-z](?-i:[a-z])$/i, ['ab', 'Ab'], ['aB']);
test(/^(?i:[a-z])[a-z]$/, ['ab', 'Ab'], ['aB']);
test(/(?i:foo|bar)/, ['FOO', 'FOo', 'Foo', 'fOO', 'BAR', 'BAr', 'Bar', 'bAR']);
test(/(?i:foo|bar|baz)/, [
  'FOO', 'FOo', 'Foo', 'fOO', 'BAR', 'BAr', 'Bar', 'bAR', 'BAZ', 'BAz', 'Baz',
  'bAZ'
]);
test(
    /Foo(?i:B[\q{ĀĂĄ|AaA}--\q{āăą}])r/v, ['FooBaaar', 'FoobAAAr'],
    ['FooBĀĂĄr', 'FooBaaaR']);

test(/(?m:^foo$)/, ['foo', '\nfoo', 'foo\n', '\nfoo\n'], ['xfoo', 'foox']);

test(
    /(?s:^.$)/, ['a', 'A', '0', '\n', '\r', '\u2028', '\u2029', 'π'],
    ['\u{10300}']);

test(
    /(?ms-i:^f.o$)/i, ['foo', '\nf\ro', 'f\no\n', '\nfπo\n'],
    ['Foo', '\nf\nO', 'foO\n', '\nFOO\n']);
test(
    /(?m:^f(?si:.o)$)/, ['foo', '\nfoO', 'f\no\n', '\nf\rO\n'],
    ['Foo', 'F\no\n']);
test(/(?i:.oo)/, ['Foo', 'FOO', 'fOo', 'foO']);

test(/(?i:foo)[x-z]/v, ['Foox', 'fOoz'], ['fooX','FooZ']);

// A modifier group inside a negative lookaround must not leak its flags into
// the continuation. Analysis visits the two branches of the lookaround choice
// in sequence, so without a reset the continuation's classes are made case
// independent under the group's flags rather than the pattern's.
test(/(?!(?i:x|y))[K]/, ['K'], ['k']);
test(/(?<!(?i:x|y))[K]/, ['K'], ['k']);
test(/(?!(?-i:x|y))[K]/i, ['K', 'k']);

// Emitting a modifier group must leave the outer flags behind. The graph has a
// ModifyFlags node that restores them, but a branching body shares it between
// its alternatives, so it is not necessarily emitted last. The trailing
// alternatives are there to defer a node to the work list, which is then
// emitted after the main walk, under the group's flags. Only the tail is case
// independent; (?-i:) still rejects an uppercase X.
test(
    /^K(?-i:(?:x|y))(?:a|b)(?:c|d)(?:e|f)(?:g|h)/i,
    ['kxaceg', 'KxaceG', 'kxACEH'], ['kXaceg', 'KXaceg']);

// Nodes inside a modifier group deferred to the work list must be compiled with
// the group's inner flags, not the outer flags.
test(
    /^(?i:(?:a|b)(?:c|d)(?:e|f)(?:g|h)(?:i|j)(?:k|l)(?:m|n)X)/,
    ['acegikmx', 'acegikmX', 'bdfhjlnx', 'bdfhjlnX'],
    ['acegikmy', 'bdfhjlny']);

// Symmetrically, nodes inside a (?-i:...) group under outer /i must compile
// case-sensitively even when deferred.
test(
    /^(?-i:(?:a|b)(?:c|d)(?:e|f)(?:g|h)(?:i|j)(?:k|l)(?:m|n)x)/i,
    ['acegikmx', 'bdfhjlnx'],
    ['acegikmX', 'bdfhjlnX']);
