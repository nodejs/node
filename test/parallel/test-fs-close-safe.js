'use strict';

const common = require('../common');

const assert = require('assert');
const fs = require('fs');

const fdObj = fs.openSyncSafe(__filename, 'r');

fs.closeSafe(fdObj, common.mustCall(function(...args) {
  assert.deepStrictEqual(args, [null]);
}));
