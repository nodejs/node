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
  );
}

// Ref: ECMA-48 5.4. A control sequence is CSI, then any number of parameter
// bytes (0x30-0x3F), then any number of intermediate bytes (0x20-0x2F), then a
// single final byte (0x40-0x7E). Every sequence below is well formed and in use
// by terminals today, and every one of them used to have its tail left behind.
tests.push(
  // Parameter bytes other than digits and `;`.
  ['a\u001B[<35;10;20Mb', 'ab'], // SGR mouse report
  ['a\u001B[>1;2cb', 'ab'], // Secondary device attributes
  ['a\u001B[=5hb', 'ab'],
  ['a\u009B<35;10;20Mb', 'ab'], // Same, with the 8 bit CSI introducer
  // Final bytes that the reference implementation does not list.
  ['a\u001B[3@b', 'ab'], // ICH, insert character
  ['a\u001B[3Xb', 'ab'], // ECH, erase character
  ['a\u001B[5db', 'ab'], // VPA, line position absolute
  ['a\u001B[3bb', 'ab'], // REP, repeat preceding character
  ['a\u001B[2ab', 'ab'], // HPR, character position forward
  // Sub parameters are separated by `:`, which is a parameter byte as well.
  ['a\u001B[38:2:255:0:0mb', 'ab'], // Truecolour in its colon form
  ['a\u001B[4:3mb', 'ab'], // Curly underline
  // Intermediate bytes.
  ['a\u001B[2 qb', 'ab'], // DECSCUSR, set cursor style
  ['a\u001B[!pb', 'ab'], // DECSTR, soft terminal reset
);

test('util.stripVTControlCharacters', (t) => {
  for (const [before, expected] of tests) {
    t.assert.strictEqual(util.stripVTControlCharacters(before), expected);
  }
});
