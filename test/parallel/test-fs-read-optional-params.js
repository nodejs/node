'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');
const defaultBufferAsync = Buffer.alloc(16384);

// Optional buffer, offset, length, position
// fs.read(fd, callback);
fs.read(fd, {}, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.length);
  assert.deepStrictEqual(defaultBufferAsync.length, buffer.length);
}));
