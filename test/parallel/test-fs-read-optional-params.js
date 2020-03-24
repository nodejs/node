'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');
const defaultBufferAsync = Buffer.alloc(16384);
const bufferAsOption = Buffer.allocUnsafe(expected.length);

// Test passing in an empty options object
fs.read(fd, { position: 0 }, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.length);
  assert.deepStrictEqual(defaultBufferAsync.length, buffer.length);
}));

// Test not passing in any options object
fs.read(fd, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.length);
  assert.deepStrictEqual(defaultBufferAsync.length, buffer.length);
}));

// Test passing in options
fs.read(fd, {
  buffer: bufferAsOption,
  offset: 0,
  length: bufferAsOption.length,
  position: 0
},
        common.mustCall((err, bytesRead, buffer) => {
          assert.strictEqual(bytesRead, expected.length);
          assert.deepStrictEqual(bufferAsOption.length, buffer.length);
        }));
