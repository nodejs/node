'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.isOSX) common.skip('MacOS-only test.');

assert.strictEqual(fs.realpathSync.native('/users'), '/Users');
fs.realpath.native('/users', common.mustCall((err, res) => {
  assert.ifError(err);
  assert.strictEqual(res, '/Users');
}));
