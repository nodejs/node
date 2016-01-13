'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const fn = path.join(common.fixturesDir, 'empty.txt');

fs.readFile(fn, function(err, data) {
  assert.ok(data);
});

fs.readFile(fn, 'utf8', function(err, data) {
  assert.strictEqual('', data);
});

assert.ok(fs.readFileSync(fn));
assert.strictEqual('', fs.readFileSync(fn, 'utf8'));
