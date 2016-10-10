'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

var filename = path.join(common.fixturesDir, 'does_not_exist.txt');
fs.readFile(filename, 'latin1', common.mustCall(function(err, content) {
  assert.ok(err);
}));
