'use strict';

const common = require('../common');

const assert = require('assert');
const fs = require('fs');

const fd = fs.openSync(__filename, 'r');

fs.close(fd, common.mustCall(function(...args) {
  assert.deepStrictEqual(args, [null]);
}));
