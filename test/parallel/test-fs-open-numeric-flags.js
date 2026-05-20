'use strict';
require('../common');

const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// O_WRONLY without O_CREAT shall fail with ENOENT
const pathNE = tmpdir.resolve('file-should-not-exist');
assert.throws(
  () => fs.openSync(pathNE, fs.constants.O_WRONLY),
  (e) => e.code === 'ENOENT'
);
