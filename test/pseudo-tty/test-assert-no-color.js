'use strict';
require('../common');
const assert = require('assert').strict;

process.env.NODE_DISABLE_COLORS = true;

try {
  assert.deepStrictEqual({}, { foo: 'bar' });
} catch (error) {
  const expected =
    'Expected values to be strictly deep-equal:\n' +
    '+ actual - expected\n' +
    '\n' +
    '+ {}\n' +
    '- {\n' +
    '-   foo: \'bar\'\n' +
    '- }';
  assert.strictEqual(error.message, expected);
}
