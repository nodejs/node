'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const filepath = path.join(common.fixturesDir, 'x.txt');
const fd = fs.openSync(filepath, 'r');
const expected = '';

fs.read(fd, 0, 0, 'utf-8', common.mustCall(function(err, str, bytesRead) {
  assert.ok(!err);
  assert.equal(str, expected);
  assert.equal(bytesRead, 0);
}));

const r = fs.readSync(fd, 0, 0, 'utf-8');
assert.equal(r[0], expected);
assert.equal(r[1], 0);
