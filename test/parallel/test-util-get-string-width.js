'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

const { getStringWidth } = util;

assert.strictEqual(getStringWidth(''), 0);
assert.strictEqual(getStringWidth('a'), 1);
assert.strictEqual(getStringWidth('hello'), 5);
assert.strictEqual(getStringWidth('a你b'), 4);

// Full-width characters count as two columns.
assert.strictEqual(getStringWidth('丁'), 2);
assert.strictEqual(getStringWidth('你好'), 4);
assert.strictEqual(getStringWidth('가'), 2);
assert.strictEqual(getStringWidth('👅'), 2);
assert.strictEqual(getStringWidth('Ｆ'), 2);  // Fullwidth Latin letter.
assert.strictEqual(getStringWidth('ｱ'), 1);  // Halfwidth katakana.
assert.strictEqual(getStringWidth('\u{1D49C}'), 1);  // Astral, not wide.
assert.strictEqual(getStringWidth('\u{20000}'), 2);  // CJK Extension B.

// Zero-width characters count as zero.
assert.strictEqual(getStringWidth('\u0000'), 0);
assert.strictEqual(getStringWidth('\u0007'), 0);
assert.strictEqual(getStringWidth('\n'), 0);
assert.strictEqual(getStringWidth('\r\n'), 0);
assert.strictEqual(getStringWidth('\t'), 0);
assert.strictEqual(getStringWidth('́'), 0);
assert.strictEqual(getStringWidth('é'), 1);
assert.strictEqual(getStringWidth('​'), 0);
assert.strictEqual(getStringWidth('a‏b'), 2);  // Right-to-left mark.

// ANSI escape sequences are ignored.
assert.strictEqual(getStringWidth('\u001B[31mred\u001B[39m'), 3);
assert.strictEqual(getStringWidth(util.styleText('bold', 'hi')), 2);
assert.strictEqual(getStringWidth(util.styleText(['bold', 'red'], 'ok')), 2);
assert.strictEqual(
  getStringWidth('\u001B]8;;https://nodejs.org\u0007text\u001B]8;;\u0007'), 4);

// Lone surrogates take one column.
assert.strictEqual(getStringWidth('\uD83D'), 1);

if (common.hasIntl) {
  assert.strictEqual(getStringWidth(' '), 1);
  assert.strictEqual(getStringWidth('กิ'), 1);  // Thai with a vowel mark.
  // Joined sequences count each emoji; a terminal may render them narrower.
  assert.strictEqual(getStringWidth('\u{1F469}‍\u{1F469}‍\u{1F467}‍\u{1F467}'), 8);
  assert.strictEqual(getStringWidth('\u{1F44D}\u{1F3FD}'), 4);  // Skin tone modifier.
  assert.strictEqual(getStringWidth('\u{1F1EE}\u{1F1F9}'), 4);  // Flag.
  assert.strictEqual(getStringWidth('❤️'), 1);
  assert.strictEqual(getStringWidth('1️⃣'), 1);  // Keycap.
}

for (const value of [undefined, null, 1, true, {}, [], Symbol('s')]) {
  assert.throws(() => getStringWidth(value), {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  });
}
