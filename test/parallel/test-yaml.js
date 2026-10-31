// Flags: --experimental-yaml
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { parse } = require('node:yaml');

common.expectWarning('ExperimentalWarning', 'YAML is an experimental feature and might change at any time');

// Scalars of the core schema.
assert.deepStrictEqual(parse(`
nothing: ~
also: null
empty:
yes: true
no: FALSE
int: -42
hex: 0x1F
octal: 0o17
float: 1.5e3
inf: -.inf
str: yes
version: 1.2.3
quoted: "0x10"
`), {
  nothing: null,
  also: null,
  empty: null,
  yes: true,
  no: false,
  int: -42,
  hex: 31,
  octal: 15,
  float: 1500,
  inf: -Infinity,
  str: 'yes',
  version: '1.2.3',
  quoted: '0x10',
});
assert.ok(Number.isNaN(parse('.NaN')));

// Collections.
assert.deepStrictEqual(parse(`
- a
- - nested
  - seq
- key: value
  other: 2
-
  late: true
- [1, 2, {x: y}]
`), [
  'a',
  ['nested', 'seq'],
  { key: 'value', other: 2 },
  { late: true },
  [1, 2, { x: 'y' }],
]);

assert.deepStrictEqual(parse(`
map:
  seq:
  - compact
  - entries
  flow: { a: 1, "b":2, c, d: }
? explicit key
: explicit value
? only key
`), {
  'map': { seq: ['compact', 'entries'], flow: { a: 1, b: 2, c: null, d: null } },
  'explicit key': 'explicit value',
  'only key': null,
});

assert.deepStrictEqual(parse('[a: 1, b, "c": d]'),
                       [{ a: 1 }, 'b', { c: 'd' }]);
assert.deepStrictEqual(parse('{ 1: one, true: yes, ~: nothing }'),
                       { 1: 'one', true: 'yes', null: 'nothing' });
// Collection keys become their JSON text.
assert.deepStrictEqual(parse('? [a, b]\n: c\n{x: 1}: d'),
                       { '["a","b"]': 'c', '{"x":1}': 'd' });

// Scalar styles.
assert.deepStrictEqual(parse(`
plain: a multi
  line plain

  scalar
single: 'it''s
  folded'
double: "tab\\there\\
  joined \\u00e9 \\x41"
literal: |
  line one
   indented

  line three
folded: >-
  folded
  text

  kept
keep: |+
  text

strip: |-
  text
indicator: |2
    two extra
`), {
  plain: 'a multi line plain\nscalar',
  single: "it's folded",
  double: 'tab\therejoined \u00e9 A',
  literal: 'line one\n indented\n\nline three\n',
  folded: 'folded text\nkept',
  keep: 'text\n\n',
  strip: 'text',
  indicator: '  two extra\n',
});

// Anchors, aliases and tags.
{
  const value = parse(`
base: &base
  name: base
copy: *base
list: &list
  - *base
  - *list
str: !!str 123
int: !!int "42"
float: !!float 1
tagged: !custom 123
`);
  assert.strictEqual(value.copy, value.base);
  assert.strictEqual(value.list[0], value.base);
  assert.strictEqual(value.list[1], value.list);
  assert.strictEqual(value.str, '123');
  assert.strictEqual(value.int, 42);
  assert.deepStrictEqual(parse('[!!int -7, !!int 0x1f, !!int 0o17, !!float 2, !!float .inf]'),
                         [-7, 31, 15, 2, Infinity]);
  assert.strictEqual(value.float, 1);
  assert.strictEqual(value.tagged, '123');
}

// Documents.
assert.strictEqual(parse(''), null);
assert.strictEqual(parse('# only a comment\n'), null);
assert.strictEqual(parse('---\n'), null);
assert.strictEqual(parse('--- text\n...\n'), 'text');
assert.deepStrictEqual(parse('%YAML 1.2\n---\na: 1\n...\n---\n- 2\n'),
                       [{ a: 1 }, [2]]);
assert.strictEqual(parse('%TAG !e! tag:example.com,2000:\n--- !e!thing\nx\n'), 'x');
assert.deepStrictEqual(parse('...\n---\n...\nvalue\n...\n...\n- 1'), [null, 'value', [1]]);
assert.strictEqual(parse('!!binary abc'), 'abc');
assert.strictEqual(parse('\uFEFFbom'), 'bom');
assert.deepStrictEqual(parse('\uFEFF  a: 1\n  b: 2'), { a: 1, b: 2 });

// Binary input is decoded as UTF-8.
assert.deepStrictEqual(parse(Buffer.from('caf\u00e9: 1')), { 'caf\u00e9': 1 });
assert.deepStrictEqual(parse(new TextEncoder().encode('[1]').buffer), [1]);
{
  const bytes = new TextEncoder().encode('xx[2]yy');
  assert.deepStrictEqual(parse(new Uint8Array(bytes.buffer, 2, 3)), [2]);
  assert.deepStrictEqual(parse(new DataView(bytes.buffer, 2, 3)), [2]);
  const shared = new SharedArrayBuffer(3);
  new Uint8Array(shared).set(bytes.subarray(2, 5));
  assert.deepStrictEqual(parse(shared), [2]);
  assert.deepStrictEqual(parse(new Uint8Array(shared)), [2]);
}

// Prototype keys are own properties, as with JSON.parse().
{
  const value = parse('__proto__: 1\nconstructor: 2\n');
  assert.strictEqual(Object.getPrototypeOf(value), Object.prototype);
  assert.deepStrictEqual(Object.keys(value), ['__proto__', 'constructor']);
  assert.strictEqual(Object.getOwnPropertyDescriptor(value, '__proto__').value, 1);
}

// Errors.
for (const input of [1, null, undefined, {}, [], Symbol('x')]) {
  assert.throws(() => parse(input), { code: 'ERR_INVALID_ARG_TYPE' });
}
const invalid = [
  ['a: 1\n b: 2', /Implicit mapping keys must be on a single line \(line 1, column 4\)/],
  ['a: [1]\n  b: 2', /Bad indentation \(line 2, column 3\)/],
  ['a: 1\na: 2', /Duplicate mapping key "a" \(line 2, column 1\)/],
  ['[1, 2', /Unterminated flow collection/],
  ['"open', /Unterminated double-quoted scalar \(line 1, column 1\)/],
  ['"\\q"', /Unknown escape sequence "\\q"/],
  ['*missing', /Unknown anchor "missing"/],
  ['a\n b: 1', /Implicit mapping keys must be on a single line/],
  ['!!int abc', /"abc" is not an integer/],
  ['!!int 1.0', /"1\.0" is not an integer/],
  ['!!int 1e2', /"1e2" is not an integer/],
  ['!!int 0x', /"0x" is not an integer/],
  ['!!float abc', /"abc" is not a float/],
  ['!!seq {}', /cannot be applied to a mapping/],
  ['%YAML 1.2\na: 1', /Directives must be followed by "---"/],
  ['%YAML 2.0\n---\na', /Unsupported YAML version "2\.0"/],
  ['a: |\n  x\n b', /Bad indentation/],
  ['a:\n\tb: 1', /Tabs cannot be used for indentation/],
  ['key: "a"#c', /Comments must be separated/],
  ['... trailing', /Unexpected content after "\.\.\."/],
  ['--- a: 1', /A block mapping is not allowed here/],
  ['key: - a', /A block sequence is not allowed here/],
  ['a: b: c', /A block mapping is not allowed here/],
  ['-\t- a', /Tabs cannot be used for indentation/],
  ['key: "a\nb"', /Bad indentation in a quoted scalar/],
  ['key: [a,\nb]', /Bad indentation in a flow collection/],
];
for (const [input, message] of invalid) {
  assert.throws(() => parse(input), {
    code: 'ERR_INVALID_YAML',
    name: 'SyntaxError',
    message,
  }, input);
}

// Line endings, leading spaces and the special floats.
assert.deepStrictEqual(parse('a: 1\r\nb: |\r\n  x\r\n\r\n  y\rc: 3\r'),
                       { a: 1, b: 'x\n\ny\n', c: 3 });
assert.deepStrictEqual(parse('  a: 1\n  b: 2'), { a: 1, b: 2 });
assert.ok(Number.isNaN(parse('.nan')));
assert.deepStrictEqual(parse('[.inf, +.INF, -.Inf, -0, +3, 1e3, 1E+3, 1.5e-1]'),
                       [Infinity, Infinity, -Infinity, 0, 3, 1000, 1000, 0.15]);
// Things that look like numbers but are strings in the core schema.
assert.deepStrictEqual(parse('[0xZZ, 0o9, 1e, 1e+, 1.2.3, +, ., ~x, 0x, 1_000, .e1]'),
                       ['0xZZ', '0o9', '1e', '1e+', '1.2.3', '+', '.', '~x', '0x', '1_000', '.e1']);

// Every escape sequence of double-quoted scalars.
assert.strictEqual(
  parse('"\\0\\a\\b\\t\\\t\\n\\v\\f\\r\\e\\ \\"\\/\\\\\\N\\_\\L\\P\\x41\\u00e9\\U0001F600"'),
  '\0\x07\b\t\t\n\v\f\r\x1B "/\\\x85\xA0\u2028\u2029A\u00e9\u{1F600}');

// Tags.
assert.deepStrictEqual(parse('%TAG !e! tag:example.com,2000:\n--- [!e!a%41 x, !<tag:x> y, !!null ~, !!bool True]'),
                       ['x', 'y', null, true]);
{
  const value = parse('&a\n[1, 2]\n');
  assert.deepStrictEqual(value, [1, 2]);
  assert.deepStrictEqual(parse('- &a\n  [1]\n- *a')[1], [1]);
}
assert.deepStrictEqual(parse('x: 1\n[a, b]: 2\n{c: 3}: 4'),
                       { 'x': 1, '["a","b"]': 2, '{"c":3}': 4 });
assert.deepStrictEqual(parse('[ ? a, ? b : c, ? : d ]'), [{ a: null }, { b: 'c' }, { null: 'd' }]);
assert.deepStrictEqual(parse('? a\n? b\n:\n? \n'), { a: null, b: null, null: null });
assert.strictEqual(parse('|+\n\n\n'), '\n\n');
assert.strictEqual(parse('- >+\n\n\n')[0], '\n\n');
assert.strictEqual(parse('|-\n\n'), '');
assert.strictEqual(parse('a: |\n  text\n ').a, 'text\n');
assert.strictEqual(parse('a: |+\n  text\n ').a, 'text\n\n');
assert.strictEqual(parse('a: |+\n\n').a, '\n');
assert.deepStrictEqual(parse('a: |+\n\nb: 1'), { a: '\n', b: 1 });
assert.deepStrictEqual(parse('{:}'), { null: null });
assert.deepStrictEqual(parse('[ ? ]'), [{ null: null }]);
// Properties at the end of the input are an empty scalar.
assert.strictEqual(parse('!!str'), '');
assert.strictEqual(parse('&a #c'), null);
assert.deepStrictEqual(parse('a: !!str'), { a: '' });
{
  // An alias may refer to the collection that holds it, with the anchor on
  // the same line or on the line before.
  const value = parse('...\n--- &a [*a, {b: *a}]');
  assert.strictEqual(value[0], value);
  assert.strictEqual(value[1].b, value);
  const seq = parse('&a\n[*a]');
  assert.strictEqual(seq[0], seq);
  const map = parse('&a\n{k: *a}');
  assert.strictEqual(map.k, map);
  const block = parse('&a\n- *a\n- &b\n  k: *b');
  assert.strictEqual(block[0], block);
  assert.strictEqual(block[1].k, block[1]);
  const nested = parse('key: &a\n  [*a]\nother: !!seq &b\n  - *b');
  assert.strictEqual(nested.key[0], nested.key);
  assert.strictEqual(nested.other[0], nested.other);
}

// Raw control characters are rejected, their escaped forms are not.
assert.strictEqual(parse('"\\0\\a\\x7f\\x85\\x9f"'), '\0\x07\x7f\x85\x9f');
for (const c of ['\0', '\x07', '\x0b', '\x1f', '\x7f', '\x80', '\x9f', '\ufffe']) {
  const expected = { code: 'ERR_INVALID_YAML', message: /is not allowed in YAML/ };
  assert.throws(() => parse(`"a${c}b"`), expected);
  assert.throws(() => parse(`'a${c}b'`), expected);
  assert.throws(() => parse(`a${c}b`), expected);
  assert.throws(() => parse(`|\n a${c}b`), expected);
  assert.throws(() => parse(`# ${c}`), expected);
}
assert.strictEqual(parse('"\u0085\u00a0\u2028\ud83d\ude00"'), '\u0085\u00a0\u2028\ud83d\ude00');
assert.strictEqual(parse('"😀"'), '😀');
for (const lone of ['\ud83d', '\ude00', '\ude00\ud83d', '\ud83d\ud83d']) {
  assert.throws(() => parse(`"${lone}"`), { code: 'ERR_INVALID_YAML', message: /U\+d(83d|e00) is not allowed/ });
}

// An escaped line break joins the lines; empty lines in between are kept.
assert.strictEqual(parse('"a\\\n  b"'), 'ab');
assert.strictEqual(parse('"a\\\n\n\n  b"'), 'a\n\nb');
assert.strictEqual(parse('key: "a \\\n  \\ b"').key, 'a  b');
assert.throws(() => parse('key: "a\\\nb"'), { code: 'ERR_INVALID_YAML', message: /Bad indentation in a quoted scalar/ });
assert.throws(() => parse('"a\\\n---\nb"'), { code: 'ERR_INVALID_YAML', message: /Unexpected document marker inside a quoted scalar/ });
assert.throws(() => parse('"a\\\n'), { code: 'ERR_INVALID_YAML', message: /Unterminated quoted scalar/ });

// A collection key is bounded, so aliases cannot blow it up.
{
  let yaml = 'a0: &a0 [x, x, x, x, x, x, x, x, x, x]\n';
  for (let i = 1; i < 12; i++) {
    const prev = `*a${i - 1}`;
    yaml += `a${i}: &a${i} [${Array(10).fill(prev).join(', ')}]\n`;
  }
  assert.throws(() => parse(`${yaml}? *a11\n: 1\n`), {
    code: 'ERR_INVALID_YAML',
    message: /A collection key must not exceed 1024 characters \(line 13, column 1\)/,
  });
  assert.strictEqual(parse(yaml).a11.length, 10);
}
assert.deepStrictEqual(parse('- ? a'), [{ a: null }]);

const moreInvalid = [
  ['key: "a\n  b', /Unterminated double-quoted scalar \(line 1, column 6\)/],
  ["'open", /Unterminated single-quoted scalar/],
  ['"\\x4"', /Invalid hexadecimal escape sequence/],
  ['"\\U00110000"', /Invalid code point in escape sequence/],
  ['"\\UD800DC00"', /Invalid code point in escape sequence/],
  ['"\\', /Unknown escape sequence/],
  ['"a\\\n', /Unterminated quoted scalar/],
  ['|--\n a', /Repeated chomping indicator/],
  ['|22\n a', /Repeated indentation indicator/],
  ['|0\n a', /Indentation indicator must be between 1 and 9/],
  ['| x\n a', /Unexpected characters after block scalar header/],
  ['|#c\n a', /Comments must be separated/],
  ['& a', /Anchor name must not be empty/],
  ['&a &b c', /A node can have only one anchor/],
  ['!!str !!int 1', /A node can have only one tag/],
  ['&a\n&b c', /A node can have only one anchor/],
  ['!!str\n!!int 1', /A node can have only one tag/],
  ['&a *b', /An alias cannot have properties/],
  ['- &b b\n- &a\n  *b', /An alias cannot have properties/],
  ['[&a *b]', /An alias cannot have properties/],
  ['&a *b: 1', /An alias cannot have properties/],
  ['!e!x a', /Undeclared tag handle "!e!"/],
  ['!!str%4 a', /Invalid escape in tag/],
  ['!<tag:x', /Unterminated verbatim tag/],
  ['!<> a', /Verbatim tag must not be empty/],
  ['!<tag:x>a', /Unexpected characters after tag/],
  ['!!str!x a', /Invalid character in tag/],
  ['!! a', /Tag suffix must not be empty/],
  ['!!bool yes', /"yes" is not a boolean/],
  ['!!null x', /"x" is not null/],
  ['!!map 1', /cannot be applied to a scalar/],
  ['!!str [a]', /cannot be applied to a sequence/],
  ['!!int {a: 1}', /cannot be applied to a mapping/],
  ['[ | ]', /Block scalars are not allowed in flow collections/],
  ['[ a, ]b', /Unexpected content after the document root/],
  ['[ "a" b ]', /Expected "," or "\]" in flow sequence/],
  ['[ , ]', /Unexpected character in flow sequence/],
  ['{ , }', /Unexpected character in flow mapping/],
  ['{ *a:1 }', /Unknown anchor/],
  ['- &a x\n- { *a :b }', /Expected whitespace after ":" in flow mapping/],
  ['&a ? b', /A block mapping is not allowed here/],
  ['{ a: "1" b }', /Expected "," or "}" in flow mapping/],
  ['{ a: 1\n', /Unterminated flow collection/],
  ['[ a,\n--- ]', /Unexpected document marker inside a flow collection/],
  ['"a\n---\n"', /Unexpected document marker inside a quoted scalar/],
  ['? a\n- b', /Expected a mapping key/],
  ['a: 1\n- b', /Unexpected sequence entry in a mapping/],
  ['*a: 1', /Unknown anchor "a:"/],
  ['- &a a\n- *a : 1\n  *a x: 2', /Expected ":" after mapping key/],
  ['a: 1\n"b" c: 2', /Expected ":" after mapping key/],
  ['a: 1\n@b: 2', /Expected a mapping key/],
  ['a: 1\n&x\nb: 2', /Expected a mapping key/],
  ['%YAML 1.2#c\n---\na', /Invalid YAML version "1\.2#c"/],
  ['%YAML 1.2\n%YAML 1.2\n---\na', /Repeated %YAML directive/],
  ['%YAML\n---\na', /The %YAML directive takes one argument/],
  ['%YAML x.2\n---\na', /Invalid YAML version "x\.2"/],
  ['%YAML 1.x\n---\na', /Invalid YAML version "1\.x"/],
  ['%TAG !e!\n---\na', /The %TAG directive takes two arguments/],
  ['%TAG e! tag:x\n---\na', /Invalid tag handle "e!"/],
  ['%TAG !e tag:x\n---\na', /Invalid tag handle "!e"/],
  ['%TAG !e! tag:x\n%TAG !e! tag:y\n---\na', /Repeated tag handle "!e!"/],
  ['%YAML 1.2', /Directives must be followed by "---"/],
  ['%YAML 1.2\n...', /Directives must be followed by "---"/],
  ['-\t- a', /Tabs cannot be used for indentation/],
  ['- \t? a', /Tabs cannot be used for indentation/],
  ['- \ta: 1', /Tabs cannot be used for indentation/],
  ['- &a x\n-\t*a : v', /Tabs cannot be used for indentation/],
  ['- &a x\n- k: *a : v', /A block mapping is not allowed here/],
  ['a: : b', /A block mapping is not allowed here/],
  ['-\t: b', /Tabs cannot be used for indentation/],
  ['x: 1\ry: "a\r b', /Unterminated double-quoted scalar \(line 2, column 4\)/],
  ['a: 1\n| x', /Expected a mapping key/],
  ['!!int 0x1g', /"0x1g" is not an integer/],
  ['!!int 0o8', /"0o8" is not an integer/],
  ['!!int +', /"\+" is not an integer/],
  ['[ a: }', /Expected "," or "\]" in flow sequence/],
  ['{ a: ] }', /Expected "," or "}" in flow mapping/],
  ['"a":b', /Unexpected content after the document root/],
  ['[a]:b', /Unexpected content after the document root/],
  ['a: 1\n"b":2', /Expected ":" after mapping key/],
  ['a: 1\n&b', /Expected a mapping key/],
  ['a:\n  b\n\tc: 1', /Tabs cannot be used for indentation|Bad indentation/],
];
for (const [input, message] of moreInvalid) {
  assert.throws(() => parse(input), {
    code: 'ERR_INVALID_YAML',
    name: 'SyntaxError',
    message,
  }, input);
}

// Deep nesting is bounded.
assert.throws(() => parse('['.repeat(1001) + ']'.repeat(1001)),
              { code: 'ERR_INVALID_YAML', message: /Nesting deeper than 1000 levels/ });
assert.ok(Array.isArray(parse('['.repeat(1000) + ']'.repeat(1000))));
