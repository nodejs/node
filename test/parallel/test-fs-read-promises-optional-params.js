'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const read = require('util').promisify(fs.read);
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');
const defaultBufferAsync = Buffer.alloc(16384);

read(fd, {})
  .then(function({ bytesRead, buffer }) {
    assert.strictEqual(bytesRead, expected.length);
    assert.deepStrictEqual(defaultBufferAsync.length, buffer.length);
  })
  .then(common.mustCall());
