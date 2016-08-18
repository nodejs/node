'use strict';
const common = require('../common');
const assert = require('assert');

const expected =
  'Using Buffer without `new` will soon stop working. ' +
  'Use `new Buffer()`, or preferably ' +
  '`Buffer.from()`, `Buffer.allocUnsafe()` or `Buffer.alloc()` instead.';

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'DeprecationWarning');
  assert.strictEqual(warning.message, expected,
                     `unexpected error message: "${warning.message}"`);
}, 1));

Buffer(1);
Buffer(1);
