'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var filename = path.join(common.fixturesDir, 'does_not_exist.txt');

fs.readFile(filename, 'latin1', common.mustCall(function(err, content) {
  assert.ok(err);
}));
