'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');

var fn = path.join(common.fixturesDir, 'elipses.txt');

var s = fs.readFileSync(fn, 'utf8');
for (var i = 0; i < s.length; i++) {
  assert.equal('\u2026', s[i]);
}
assert.equal(10000, s.length);
