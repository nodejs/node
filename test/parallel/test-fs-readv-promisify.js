'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const readv = require('util').promisify(fs.readv);
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = [Buffer.from('xyz\n')];

readv(fd, expected)
  .then(function({ bytesRead, buffers }) {
    assert.deepStrictEqual(bytesRead, expected[0].length);
    assert.deepStrictEqual(buffers, expected);
  })
  .then(common.mustCall());
