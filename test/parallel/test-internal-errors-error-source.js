// Flags: --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const { getErrorSourceMessage } = require('internal/errors/error_source');

assert.strictEqual(
  getErrorSourceMessage('file.js', 1, undefined, 0, 1),
  undefined,
);
assert.strictEqual(
  getErrorSourceMessage('file.js', Number.NaN, 'const value = 1;', 0, 1),
  undefined,
);

assert.strictEqual(
  getErrorSourceMessage('file.js', 1, '\tconst value = 1;', -5, 0),
  'file.js:1\n\tconst value = 1;\n^\n',
);

assert.strictEqual(
  getErrorSourceMessage('file.js', 7, `${'x'.repeat(20)}target${'y'.repeat(140)}`, 20, 200),
  `file.js:7\n${'x'.repeat(20)}target${'y'.repeat(94)}...\n${' '.repeat(20)}${'^'.repeat(100)}\n`,
);
