'use strict';
// Flags: --expose-internals
require('../common');
const assert = require('assert').strict;
const util = require('internal/util');

try {
  // Activate colors even if the tty does not support colors.
  process.env.COLORTERM = '1';
  assert.deepStrictEqual([1, 2], [2, 2]);
} catch (err) {
  const expected = 'Input A expected to deepStrictEqual input B:\n' +
    '\u001b[32m+ expected\u001b[39m \u001b[31m- actual\u001b[39m\n\n' +
    '  [\n' +
    '\u001b[31m-\u001b[39m   1,\n' +
    '\u001b[32m+\u001b[39m   2,\n' +
    '    2\n' +
    '  ]';
  assert.strictEqual(err.message, expected);
  assert.strictEqual(err.rawMessage, util.removeColors(expected));
  assert.notStrictEqual(err.message, err.rawMessage);
}
