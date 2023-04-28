'use strict';
require('../common');
const assert = require('assert').strict;

assert.throws(() => {
  process.env.FORCE_COLOR = '1';
  delete process.env.NODE_DISABLE_COLORS;
  delete process.env.NO_COLOR;
  assert.deepStrictEqual([1, 2, 2, 2, 2], [2, 2, 2, 2, 2]);
}, (err) => {
  const expected = 'Expected values to be strictly deep-equal:\n' +
    '\u001b[32m+ actual\u001b[39m \u001b[31m- expected\u001b[39m' +
      ' \u001b[34m...\u001b[39m Lines skipped\n\n' +
    '  [\n' +
    '\u001b[32m+\u001b[39m   1,\n' +
    '\u001b[31m-\u001b[39m   2,\n' +
    '    2,\n' +
    '\u001b[34m...\u001b[39m\n' +
    '    2,\n' +
    '    2\n' +
    '  ]';
  assert.strictEqual(err.message, expected);
  return true;
});
