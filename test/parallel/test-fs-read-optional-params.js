'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');
const defaultBufferAsync = Buffer.alloc(16384);
const bufferAsOption = Buffer.allocUnsafe(expected.byteLength);

// Test not passing in any options object
fs.read(fd, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.byteLength);
  assert.deepStrictEqual(defaultBufferAsync.byteLength, buffer.byteLength);
}));
fs.read(fd, bufferAsOption, { position: 0 }, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.byteLength);
  assert.deepStrictEqual(bufferAsOption.byteLength, buffer.byteLength);
}));

// Test passing in an empty options object
fs.read(fd, { position: 0 }, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.byteLength);
  assert.deepStrictEqual(defaultBufferAsync.byteLength, buffer.byteLength);
}));
fs.read(fd, bufferAsOption, { position: 0 }, common.mustCall((err, bytesRead, buffer) => {
  assert.strictEqual(bytesRead, expected.byteLength);
  assert.deepStrictEqual(bufferAsOption.byteLength, buffer.byteLength);
}));

// Test passing in options
fs.read(fd, {
  buffer: bufferAsOption,
  offset: 0,
  length: bufferAsOption.byteLength,
  position: 0
},
        common.mustCall((err, bytesRead, buffer) => {
          assert.strictEqual(bytesRead, expected.byteLength);
          assert.deepStrictEqual(bufferAsOption.byteLength, buffer.byteLength);
        }));
fs.read(fd, bufferAsOption, {
  offset: 0,
  length: bufferAsOption.byteLength,
  position: 0
},
        common.mustCall((err, bytesRead, buffer) => {
          assert.strictEqual(bytesRead, expected.byteLength);
          assert.deepStrictEqual(bufferAsOption.byteLength, buffer.byteLength);
        }));
