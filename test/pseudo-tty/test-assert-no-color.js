'use strict';
require('../common');
const assert = require('assert').strict;

process.env.NODE_DISABLE_COLORS = true;

try {
  assert.strictEqual(1, 3);
} catch (error) {
  const expected = 'Expected values to be strictly equal:\n\n1 !== 3\n';
  assert.strictEqual(error.message, expected);
}
