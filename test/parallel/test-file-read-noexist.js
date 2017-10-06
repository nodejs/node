'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');

const filename = fixtures.path('does_not_exist.txt');
fs.readFile(filename, 'latin1', common.mustCall(function(err, content) {
  assert.ok(err);
  assert.strictEqual(err.code, 'ENOENT');
}));
