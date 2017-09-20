'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = 'xyz\n';

// Error must be thrown with string
assert.throws(() => {
  fs.read(fd,
          expected.length,
          0,
          'utf-8',
          common.mustNotCall());
}, /^TypeError: Second argument needs to be a buffer$/);

assert.throws(() => {
  fs.readSync(fd, expected.length, 0, 'utf-8');
}, /^TypeError: Second argument needs to be a buffer$/);
