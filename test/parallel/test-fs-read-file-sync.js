'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');

var fn = path.join(common.fixturesDir, 'elipses.txt');

var s = fs.readFileSync(fn, 'utf8');
for (var i = 0; i < s.length; i++) {
  assert.equal('\u2026', s[i]);
}
assert.equal(10000, s.length);
