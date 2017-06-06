'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
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
