'use strict';

require('../common');
const util = require('util');
const { test } = require('node:test');

// Ref: https://github.com/chalk/ansi-regex/blob/main/test.js
const tests = [
  // [before, expected]
  ['\u001B[0m\u001B[4m\u001B[42m\u001B[31mfoo\u001B[39m\u001B[49m\u001B[24mfoo\u001B[0m', 'foofoo'], // Basic ANSI
  ['\u001B[0;33;49;3;9;4mbar\u001B[0m', 'bar'], // Advanced colors
  ['foo\u001B[0gbar', 'foobar'], // Clear tabs
  ['foo\u001B[Kbar', 'foobar'], // Clear line
  ['foo\u001B[2Jbar', 'foobar'], // Clear screen
];

for (const ST of ['\u0007', '\u001B\u005C', '\u009C']) {
  tests.push(
    [`\u001B]8;;mailto:no-replay@mail.com${ST}mail\u001B]8;;${ST}`, 'mail'],
    [`\u001B]8;k=v;https://example-a.com/?a_b=1&c=2#tit%20le${ST}click\u001B]8;;${ST}`, 'click'],
    [`\u001B]8;;https://example.com/(foo${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo)bar${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/!foo${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo+bar${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/[foo]${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo$bar${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo'bar${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo*bar${ST}label\u001B]8;;${ST}`, 'label'],
    [`\u001B]8;;https://example.com/foo,bar${ST}label\u001B]8;;${ST}`, 'label'],
  );
}

// Colon-delimited CSI sub-parameters (SGR) should be stripped like the
// semicolon-delimited form.
tests.push(
  ['\u001B[38:2:255:0:0mHello\u001B[0m', 'Hello'],
  ['\u001B[4:3mUnderline\u001B[4:0m', 'Underline'],
);

// Malformed/truncated OSC sequences without any string terminator are left
// unmatched rather than being partially stripped.
tests.push(
  ['\u001B]8;;https://example.com/no-terminator', 'ttps://example.com/no-terminator'],
);

test('util.stripVTControlCharacters', (t) => {
  for (const [before, expected] of tests) {
    t.assert.strictEqual(util.stripVTControlCharacters(before), expected);
  }
});
