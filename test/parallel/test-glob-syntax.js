'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const cases = /* [path, pattern, matches] */ [
  // Path separators.
  ['a/b', 'a/b', true],
  ['a/b', 'a//b', true],          // Repeated separators collapse.
  ['a//b', 'a/b', true],
  ['a/', 'a', true],              // A trailing separator on the path is ignored.
  ['a/b', 'a/b/', false],
  ['', '', true],
  ['a', '', false],
  ['', '*', false],
  ['/a', '/a', true],
  ['a', '/a', false],

  // `*` matches any run of characters within one segment.
  ['abc', '*', true],
  ['a/b', '*', false],
  ['a/b', '*/*', true],
  ['abc', 'a*c', true],
  ['ac', 'a*c', true],
  ['a/b', 'a*b', false],
  ['a.txt', '*.txt', true],
  ['a.c', '*.*', true],
  ['abc', '*.*', false],

  // `?` matches exactly one character within one segment.
  ['abc', 'a?c', true],
  ['ac', 'a?c', false],
  ['a/c', 'a?c', false],

  // Bracket expressions.
  ['a', '[abc]', true],
  ['d', '[abc]', false],
  ['b', '[a-c]', true],
  ['d', '[!abc]', true],
  ['d', '[^abc]', true],
  ['a', '[!abc]', false],
  [']', '[]]', true],             // A `]` first in the set is a literal `]`.
  ['a', '[]a]', true],
  ['-', '[-a]', true],
  ['-', '[a-]', true],
  ['[a', '[a', true],             // An unterminated `[` is a literal `[`.
  ['a', '[a', false],
  ['/', '[/]', false],            // A set never matches a separator.
  ['*', '[*]', true],             // Sets are how magic characters are escaped.
  ['x', '[*]', false],
  ['?', '[?]', true],
  ['{', '[{]', true],

  // POSIX classes cover the whole of Unicode.
  ['5', '[[:digit:]]', true],
  ['a', '[[:digit:]]', false],
  ['a', '[![:digit:]]', true],
  ['5', '[![:digit:]]', false],
  ['a', '[[:alpha:]]', true],
  ['é', '[[:alpha:]]', true],
  ['é', '[a-zA-Z]', false],
  ['_', '[[:word:]]', true],
  ['A', '[[:upper:]]', true],
  ['a', '[[:foo:]]', false],      // An unknown class is not a class.
  ['a', '[[=a=]]', false],        // Equivalence classes are unsupported.
  ['ch', '[[.ch.]]', false],      // So are collating symbols.

  // A `**` alone in a segment matches zero or more segments.
  ['a/b/c', 'a/**/c', true],
  ['a/c', 'a/**/c', true],
  ['a/x/y/c', 'a/**/c', true],
  ['a/b/c/d', 'a/**', true],
  ['a/b/c', 'a/**/**/c', true],
  ['a/b/c', '**/c', true],
  ['a/b', '**', true],
  // Anywhere else, `**` is just a `*`.
  ['a/xb', 'a/**b', true],
  ['a/xb', 'a/*b', true],
  ['a/x/b', 'a/**b', false],
  // And it does not descend into dot directories.
  ['a/.b/c', 'a/**/c', false],
  ['.a/b', '**', false],

  // Extended globs.
  ['abc', '@(abc|def)', true],
  ['def', '@(abc|def)', true],
  ['abcdef', '@(abc|def)', false],
  ['ac', 'a?(b)c', true],
  ['abc', 'a?(b)c', true],
  ['abbc', 'a?(b)c', false],
  ['ac', 'a*(b)c', true],
  ['abbc', 'a*(b)c', true],
  ['ac', 'a+(b)c', false],
  ['abc', 'a+(b)c', true],
  ['abbc', 'a+(b)c', true],
  ['abd', '!(abc)', true],
  ['abc', '!(abc)', false],
  ['abc', '*(a|@(b|c))', true],   // They nest.
  ['index.ts', '*.@(js|ts)', true],
  ['index.css', '!(*.js)', true],
  ['ab', '!(a)*', false],

  // Brace expansion.
  ['a', '{a,b}', true],
  ['c', '{a,b}', false],
  ['abd', 'a{b,c}d', true],
  ['a', 'a{,b}', true],
  ['ab', 'a{,b}', true],
  ['ab', 'a{b,{c,d}}', true],     // Braces nest.
  ['ac', 'a{b,{c,d}}', true],
  ['ad', 'a{b,{c,d}}', true],
  ['a1b', 'a{1..3}b', true],      // Numeric sequences.
  ['a4b', 'a{1..3}b', false],
  ['a-1b', 'a{-1..1}b', true],
  ['a01b', 'a{01..03}b', true],   // Zero padding is kept.
  ['a1b', 'a{1..9..3}b', true],   // Sequences may take a step.
  ['a4b', 'a{1..9..3}b', true],
  ['a5b', 'a{1..9..3}b', false],
  ['ac', 'a{b..d}', true],        // Alphabetic sequences.
  ['a{b}c', 'a{b}c', true],       // No comma and no sequence means no expansion.
  ['abc', 'a{b}c', false],
  ['{}', '{}', true],

  // A leading dot is only matched by a literal dot.
  ['.hidden', '*', false],
  ['.hidden', '.*', true],
  ['.a', '?a', false],
  ['.a', '[.]a', true],
  ['a/.b/c', 'a/*/c', false],
  ['a/.b/c', 'a/.*/c', true],
  // `.` and `..` are never matched by a wildcard.
  ['.', '*', false],
  ['..', '*', false],
  ['.', '.', true],

  // There is no escape character: a backslash is a path separator.
  ['a/b', 'a\\b', true],
  ['*', '\\*', false],
  ['a*b', 'a\\*b', false],

  // A leading `!` does not negate and a leading `#` does not comment.
  ['!a', '!a', true],
  ['a', '!a', false],
  ['#a', '#a', true],

  // Patterns are normalized for walking a file system.
  ['a/c', 'a/b/../c', true],
  ['a/b', 'a/./b', true],
];

// These are the odd cases that minimatch explicitly tests, so
// we ensure compatiility.
//
// See https://github.com/isaacs/minimatch/blob/main/test/patterns.js
const minimatchCases = [
  // Runs of stars and question marks collapse
  ['abc', 'a***c', true],
  ['abc', 'a*****?c', true],
  ['abc', '?*****??', true],
  ['abc', '*****??', true],
  ['abc', '?***?****c', true],
  ['abc', '?***?****?', true],
  ['abc', '*******c', true],
  ['abc', '*******?', true],
  ['abc', '??**********?****?', false],
  ['abc', '*c*?**', false],
  ['abc', 'a********???*******', false],
  ['abcdecdhjk', 'a*cd**?**??k', true],
  ['abcdecdhjk', 'a**?**cd**?**??***k**', true],
  ['a/b/b', 'a/*/b', true],

  // Bracket expression edge cases
  ['[', '[', true],
  ['[abc', '[*', true],
  ['a', '[]', false],
  ['p', '[a-z]', true],
  [']', '[]-]', true],
  ['-', '[-abc]', true],
  ['-', '[abc-]', true],
  ['[', '[[]', true],
  ['a]b', 'a[]]*', true],
  ['a[]b', 'a[]*', true],
  ['a[[]b', 'a[[]*', true],
  ['a', '[z-a]', false],
  ['fffff', '[z-af]*', true],
  ['fffff', '[f-gz-a]*', true],
  ['a', '[a-0][a-Ā]', false],
  ['a', '[a-b-c]', true],
  ['[!ab', '[!a*', true],
  ['[ab', '[!a*', false],
  ['[#ab', '[#a*', true],

  // POSIX classes
  ['åéîøü', '[[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]]', true],
  ['aeiou', '[[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]]', true],
  ['0f7fa', '[[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]][[:alpha:]]', false],
  ['aeiou', '[[:ascii:]][[:ascii:]][[:ascii:]][[:ascii:]][[:ascii:]]', true],
  ['åéîøü', '[[:ascii:]][[:ascii:]][[:ascii:]][[:ascii:]][[:ascii:]]', false],
  ['0f7fa', '[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]',
   true],
  ['99999', '[[:xdigit:]][[:xdigit:]]???', true],
  ['0f7fa', '[[:graph:]]f*', true],
  ['fffff', '[[:graph:][:digit:]]f*', true],
  ['åéîøü', '[[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]][[:alnum:]]', true],
  ['a', '[a-[:alpha:]*]', false],

  // Extended globs
  ['foo.bar', '*.!(js)', true],
  ['foo.', '*.!(js)', true],
  ['boo.js.boo', '*.!(js)', true],
  ['foo.js.js', '*.!(js)', true],
  ['foo.js', '*.!(js)', false],
  ['blar.js', '*.!(js)', false],
  ['ac', '+(a)!(b)+(c)', true],
  ['acc', '+(a)!(b)+(c)', true],
  ['adc', '+(a)!(b)+(c)', true],
  ['abc', '+(a)!(b)+(c)', false],
  ['bac', '+(a)!(b)+(c)', false],
  ['a/b/c/bar/x', 'a/b/*/!(bar)/*', false],
  ['a/b/c/baz/x', 'a/b/*/!(bar)/*', true],
  ['ax', 'a?(b*)', false],
  ['ax', '?(a*|b)', true],
  ['x-ab', '?(x-!(y)|z)b', true],
  ['zb', '?(x-!(y)|z)b', true],
  ['x-a', '?(x-!(y)|z)', true],
  ['fool', '@(foo)*', true],
  ['oof', '@(foo)*', false],
  ['a/b', '*(a/b)', false],
  ['aa', '*(?)', true],
  ['aa.', '+(?)', true],
  ['ab', '+(a|?)', true],
  ['b.a', '+(a|!(b))', true],
  ['.aa', '+(.|a|!(b))', true],
  ['aa', '+(a|.)', true],
  ['x', '+()*(x|a)', true],
  ['a.y', '+(x|a[^)]y)', true],

  // Extended globs that never close stay literal
  ['!(a|B', '!(a|B', true],
  ['B', '!(a|B', false],
  ['?(a|B', '?(a|B', true],
  ['B', '?(a|B', false],
  ['+(a|B', '+(a|B', true],
  ['B', '+(a|B', false],
  ['*(a|B', '*(a|B', true],
  ['B', '*(a|B', false],
  ['@(a|B', '@(a|B', true],
  ['B', '@(a|B', false],

  // Nested extglobs collapse into one set
  ['xy', '@(!(a|b))y', true],
  ['by', '@(!(a|b))y', false],
  ['xy', '!(a|b)y', true],
  ['by', '!(a|b)y', false],
  ['xb', 'x@(!(a|b))', false],
  ['xy', 'x@(!(a|b))', true],
  ['xb', 'x!(a|b)', false],
  ['x', '@(!(a|b))', true],
  ['b', '@(!(a|b))', false],
  ['a', '!(!(a|b))', true],
  ['x', '!(!(a|b))', false],
  ['ab', '?(*(a|b))', true],
  ['', '?(+(a|b))', true],
  ['ab', '@(*(a|b))', true],
  ['axb', '@(a*b)', true],

  // Braces expand before anything else is parsed
  ['ab', '*(a|{b),c)}', true],
  ['ac', '*(a|{b),c)}', true],
  ['ad', '*(a|{b),c)}', false],
  ['ab', '*(a|{b,c})', true],
  ['bc', '*(a|{b|c,c})', true],
  ['d)', '{a,*(b|c,d)}', true],
  ['a{b{cdf}g}h', 'a{b{c{d,e}f}g}h', true],
  ['za,c}d', 'z{a,b},c}d', true],
  ['z{a,bcd', 'z{a,b{,c}d', true],
  // `${...}` is left alone by brace expansion.
  /* eslint-disable no-template-curly-in-string */
  ['a${c}${d}', '{a,b}${c}${d}', true],
  ['${a}${b}c', '${a}${b}{c,d}', true],
  /* eslint-enable no-template-curly-in-string */
  ['/a', '{/?,*}', true],
  ['bb', '{/?,*}', true],
  ['/b/b', '{/?,*}', false],
  ['/asdf/asdf/asdf', '{/*,*}', false],
  ['c', '{c*,./c*}', true],
  ['a5b', 'a{00..05}b', false],
  ['a05b', 'a{00..05}b', true],

  // Globstar and dot directories
  ['a/b/.x/c', '**/.x/**', true],
  ['a/b/.x/c/d/e', '**/.x/**', true],
  ['.x/a/b', '**/.x/**', true],
  ['a/.x/b/.x/c', '**/.x/**', false],
  ['.x/.y', '**/.x/**', false],
  ['.x/a/b', '.x/**/*', true],
  ['.x/a/b', '.x/*/**', true],
  ['.x/a/b', '.x/**/*/**', true],
  ['.x/.y', '.x/**/*', false],
  ['.x/', '.x/**/**/*', false],
  ['a/.d', '**', false],
  ['.a/.d', '**', false],
  ['a/b', '**', true],
  ['x/y/z', 'x/y/*/z', false],
  ['x/y/w/z', 'x/y/*/z', true],

  // A segment followed by `..` cancels
  ['x/a/b/c', 'x/*/../a/b/c', true],
  ['x/y/a/b/c', 'x/*/../a/b/c', false],
  ['a/b/c', 'x/*/../../a/b/c', true],
  ['x/a/b/c', 'x/*/../../a/b/c', false],
  ['x/y/a/b/c', 'x/z/../*/a/b/c', true],
  ['x/z/a/b/c', 'x/z/../*/a/b/c', true],
  ['a/c/b', 'a/*/b', true],
  ['a/./b', 'a/*/b', false],
  ['a/.d/b', 'a/*/b', false],
  ['a/.d/b', 'a/.*/b', true],

  // The optimized single-segment shapes
  ['a', '?', true],
  ['js', '??', true],
  ['.a', '??', false],
  ['.js', '???', false],
  ['a.js', '?.js', true],
  ['.js', '?js', false],
  ['a.js', '*.js', true],
  ['js', '*js', true],
  ['.a.js', '*.js', false],
  ['a.js', '*.*', true],
  ['.a', '*.*', false],
  ['.a.js', '.*', true],
  ['a', '*', true],
  ['.a', '*', false],
  ['x.y', '*.y', true],
  ['.z', '*.z', false],
];

// Most POSIX classes lower to Unicode property escapes (`\p{…}`), which V8
// only supports when built with ICU. Without it, such a pattern throws.
const kIntlClasses =
  /\[:(?:alnum|alpha|blank|cntrl|digit|graph|lower|print|punct|space|upper|word):\]/;

for (const [subject, pattern, expected] of [...cases, ...minimatchCases]) {
  if (!common.hasIntl && kIntlClasses.test(pattern)) continue;
  assert.strictEqual(
    path.posix.matchesGlob(subject, pattern),
    expected,
    `expected ${JSON.stringify(subject)} to ` +
      `${expected ? '' : 'not '}match ${JSON.stringify(pattern)}`,
  );
}

// However long a run of stars gets, it is equivalent to a single one.
assert.strictEqual(
  path.posix.matchesGlob('a/b/b', `a/${'*'.repeat(100)}/b`),
  path.posix.matchesGlob('a/b/b', 'a/*/b'),
);

// A pattern longer than the 64 KiB cap is rejected rather than matched.
assert.throws(() => path.matchesGlob('a', 'a'.repeat(64 * 1024 + 1)), {
  name: 'TypeError',
  message: 'pattern is too long',
});
assert.strictEqual(path.matchesGlob('a', 'a'.repeat(64 * 1024)), false);

// Braces and extended globs are parsed by recursing once per level of nesting,
// so a pattern that nests deeper than the parser can recurse is rejected
// instead of running the stack out. `{,` and `@(` cost two or three code units
// per level, which leaves room for thousands of levels under the 64 KiB cap.
{
  const braces = (n) => `${'{,'.repeat(n)}a${'}'.repeat(n)}`;
  const extglobs = (n) => `${'@('.repeat(n)}a${')'.repeat(n)}`;
  assert.strictEqual(path.matchesGlob('a', braces(256)), true);
  assert.strictEqual(path.matchesGlob('a', extglobs(256)), true);
  for (const pattern of [
    braces(257),
    braces(20000),
    extglobs(257),
    extglobs(20000),
    // An extended glob that adopts its child leaves minimatch's extDepth
    // where it is, so the nesting cap is the only thing bounding this one.
    '@('.repeat(20000),
  ]) {
    assert.throws(() => path.matchesGlob('a', pattern), {
      name: 'TypeError',
      message: 'pattern nests too deeply',
    }, pattern.slice(0, 8));
  }
  // fs.glob() compiles its patterns the same way.
  assert.throws(() => fs.globSync(braces(20000)), {
    name: 'TypeError',
    message: 'pattern nests too deeply',
  });
}

// Expanding braces holds literal braces aside as a code unit the pattern does
// not itself use. Naming every unit one could be drawn from leaves none free,
// which takes all 65408 units at or above U+0080 -- and that still fits under
// the 64 KiB cap. Reject such a pattern rather than rewrite one of its own
// literals into a brace.
{
  let every = '';
  for (let u = 0x80; u <= 0xFFFF; u++) every += String.fromCharCode(u);
  assert.strictEqual(every.length, 64 * 1024 - 0x80);
  assert.throws(() => path.matchesGlob('\uFDD0', `{\uFDD0,${every}}`), {
    name: 'TypeError',
    message: 'pattern is too long',
  });
  // Two spare units are all it takes to expand normally again.
  assert.strictEqual(
    path.matchesGlob('\uFDD0', `{\uFDD0,${every.slice(2)}}`),
    true,
  );
}

// Nesting past the extended glob and globstar caps stops matching rather
// than costing an unbounded amount of work.
assert.strictEqual(
  path.matchesGlob('a'.repeat(100), `${'*('.repeat(100)}a${')'.repeat(100)}`),
  true,
);
assert.strictEqual(path.matchesGlob('a/'.repeat(500), '**/'.repeat(500)), true);

// A nested extended glob against a long subject is what makes a backtracking
// matcher blow up: `minimatch` needs minutes on the first of these. Segments
// are matched by advancing every reachable position at once instead, so the
// cost stays bounded. A regression to backtracking would hang here.
{
  const subject = `${'a'.repeat(101)}z`;
  const start = process.hrtime.bigint();
  for (const pattern of [
    '*(*(*(a|a)))',
    '*(+(*(a|b)|c)|d)',
    '*(*(+(+(?(@(a|t)|u)|v)|w)|x)|y)',
    '+(a|*(b|c)|d)a',
  ]) {
    assert.strictEqual(path.posix.matchesGlob(subject, pattern), false, pattern);
  }
  // Chained non-adjacent globstars, from minimatch's GHSA-7r86-cg39-jmmj.
  const globstars = `${Array.from({ length: 50 }, () => '**/a').join('/')}/b/**`;
  assert.strictEqual(
    path.posix.matchesGlob(`${Array(100).fill('a').join('/')}/a`, globstars),
    false,
  );
  const elapsed = Number(process.hrtime.bigint() - start) / 1e6;
  assert.ok(
    elapsed < common.platformTimeout(5000),
    `adversarial patterns took ${elapsed}ms, matching is backtracking`,
  );
}

// The walker has to agree with the matcher, so run a few of the same rules
// against a real directory tree.
tmpdir.refresh();
const files = [
  'a/b/c.js',
  'a/b/c.ts',
  'a/d.js',
  'a/.hidden/e.js',
  'a/.dotfile.js',
  'f.js',
];
for (const file of files) {
  fs.mkdirSync(tmpdir.resolve(path.dirname(file)), { recursive: true });
  fs.writeFileSync(tmpdir.resolve(file), '');
}

function globbed(pattern) {
  return fs.globSync(pattern, { cwd: tmpdir.path })
    .map((p) => p.split(path.sep).join('/'))
    .sort();
}

// `**` crosses directories but skips dot directories, `*` does not cross.
assert.deepStrictEqual(globbed('**/*.js'), ['a/b/c.js', 'a/d.js', 'f.js']);
assert.deepStrictEqual(globbed('*.js'), ['f.js']);
assert.deepStrictEqual(globbed('a/*/*.js'), ['a/b/c.js']);
// A leading dot needs a literal dot, in a file name and in a directory name.
assert.deepStrictEqual(globbed('a/.*.js'), ['a/.dotfile.js']);
assert.deepStrictEqual(globbed('a/.*/*.js'), ['a/.hidden/e.js']);
// Braces and extended globs expand the same way here as in matchesGlob().
assert.deepStrictEqual(globbed('a/b/*.{js,ts}'), ['a/b/c.js', 'a/b/c.ts']);
assert.deepStrictEqual(globbed('a/b/*.@(js|ts)'), ['a/b/c.js', 'a/b/c.ts']);
assert.deepStrictEqual(globbed('a/b/!(*.ts)'), ['a/b/c.js']);
// A `<segment>/..` pair cancels.
assert.deepStrictEqual(globbed('a/b/../d.js'), ['a/d.js']);
